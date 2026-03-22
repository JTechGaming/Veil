import argparse
import io
import json
import os
import re
import sys
import zipfile
from collections import defaultdict
from pathlib import Path
import requests
import dbgpu
from thefuzz import process as fuzz_process


# ---------------------------------------------------------------------------
# Config
# ---------------------------------------------------------------------------

BLENDER_SNAPSHOT_URL = "https://opendata.blender.org/snapshots/opendata-latest.zip"
BLENDER_CACHE_PATH   = Path("opendata-latest.zip")
BLENDER_JSONL_NAME   = "opendata-latest.jsonl"  # name inside the zip
MIN_BLENDER_VERSION  = (3, 0, 0) # min blender version the tests were done in

# Scene weights for composite score. These three scenes cover different
# workload profiles and together give a reasonable general GPU score.
# Weights must sum to 1.0.
SCENE_WEIGHTS = {
    "bmw27":              1/9,
    "classroom":          1/9,
    "fishy_cat":          1/9,
    "koro":               1/9,
    "pavillon_barcelona": 1/9,
    "victor":             1/9,
    "monster":            1/9,
    "junkshop":           1/9,
    "barbershop_interior":1/9,
}

# Fuzzy match threshold (0-100). Below this score, a dbgpu match is rejected.
FUZZY_THRESHOLD = 82

# ---------------------------------------------------------------------------
# Code
# ---------------------------------------------------------------------------

def normalise_gpu_name(name: str) -> str:
    """Lowercase, strip redundant words, collapse whitespace."""
    name = name.lower()
    # Remove vendor prefixes and suffixes that vary across datasets
    for token in ["nvidia", "amd", "intel", "geforce", "radeon", "graphics",
                  "/", "(r)", "(tm)", "®", "™", "series", "oem",
                  "mobile", "laptop", "notebook"]:
        name = name.replace(token, " ")
    # Normalise whitespace
    name = re.sub(r"\s+", " ", name).strip()
    return name

def parse_blender_version(version_str: str):
    """'4.1.0' -> (4, 1, 0)"""
    try:
        parts = version_str.split(".")
        return tuple(int(p) for p in parts[:3])
    except Exception:
        return (0, 0, 0)

def samples_per_minute(render_time_seconds: float) -> float:
    """Blender benchmark stores render time; higher samples/min = faster GPU."""
    if render_time_seconds <= 0:
        return 0.0
    return 60.0 / render_time_seconds

def download_blender_snapshot(force: bool = False) -> Path:
    if BLENDER_CACHE_PATH.exists() and not force:
        print(f"[blender] Using cached snapshot: {BLENDER_CACHE_PATH}")
        return BLENDER_CACHE_PATH

    print(f"[blender] Downloading snapshot from {BLENDER_SNAPSHOT_URL} ...")
    response = requests.get(BLENDER_SNAPSHOT_URL, stream=True, timeout=120)
    response.raise_for_status()

    total = int(response.headers.get("content-length", 0))
    downloaded = 0
    with open(BLENDER_CACHE_PATH, "wb") as f:
        for chunk in response.iter_content(chunk_size=1024 * 256):
            f.write(chunk)
            downloaded += len(chunk)
            if total:
                pct = downloaded / total * 100
                print(f"\r  {pct:.1f}%  ({downloaded // 1024 // 1024} MB)", end="", flush=True)
    print(f"\n[blender] Saved to {BLENDER_CACHE_PATH}")
    return BLENDER_CACHE_PATH

def parse_blender_snapshot(zip_path: Path) -> dict[str, dict]:
    print("[blender] Parsing snapshot...")

    # gpu_name, scene, list of samples/min values
    raw: dict[str, dict[str, list[float]]] = defaultdict(lambda: defaultdict(list))
    # display name per normalised key
    display_names: dict[str, str] = {}

    with zipfile.ZipFile(zip_path, "r") as zf:
        # Find the JSONL file
        jsonl_names = [n for n in zf.namelist() if n.endswith(".jsonl")]
        if not jsonl_names:
            print("[error] No .jsonl file found inside snapshot zip.")
            sys.exit(1)
        jsonl_name = jsonl_names[0]
        print(f"[blender] Reading {jsonl_name} ({zf.getinfo(jsonl_name).file_size // 1024 // 1024} MB uncompressed)")

        gpu_runs_found = 0

        GPU_DEVICE_TYPES = {"CUDA", "OPTIX", "OPENCL", "HIP", "METAL", "ONEAPI"}

        scenes = []

        with zf.open(jsonl_name) as f:
            for line_bytes in f:
                try:
                    entry = json.loads(line_bytes)
                except json.JSONDecodeError:
                    continue

                data = entry["data"]
                if not isinstance(data, list):
                    continue

                for run in data:
                    try:
                        if run["device_info"]["device_type"] not in GPU_DEVICE_TYPES:
                            continue
                        version_str = run["blender_version"]["version"]
                        device_name = run["device_info"]["compute_devices"][0]["name"]
                        scene = run["scene"]["label"].lower().strip()
                        render_time = run["stats"]["total_render_time"]
                    except (KeyError, TypeError, IndexError):
                        continue

                    if scene not in scenes:
                        scenes.append(scene)

                    if parse_blender_version(version_str) < MIN_BLENDER_VERSION:
                        continue
                    if not device_name:
                        continue
                    if scene not in SCENE_WEIGHTS:
                        continue
                    if render_time <= 0:
                        continue

                    spm = samples_per_minute(render_time)
                    norm = normalise_gpu_name(device_name)
                    raw[norm][scene].append(spm)
                    if norm not in display_names:
                        display_names[norm] = device_name

    print(scenes)

    print(f"[blender] Found {len(raw)} unique GPU entries across all scenes.")

    # Median per scene, then weighted composite
    result = {}
    for norm, scenes in raw.items():
        scene_scores = {}
        total_samples = 0
        for scene, values in scenes.items():
            values.sort()
            median = values[len(values) // 2]
            scene_scores[scene] = round(median, 4)
            total_samples += len(values)

        # Only include if there is at least one target scene
        weight_sum = sum(SCENE_WEIGHTS[s] for s in scene_scores)
        if weight_sum < 0.01:
            continue
        composite = sum(
            scene_scores[s] * SCENE_WEIGHTS[s] for s in scene_scores
        ) / weight_sum

        result[norm] = {
            "display_name":  display_names.get(norm, norm),
            "blender_score": round(composite, 4),
            "sample_count":  total_samples,
            "scene_scores":  scene_scores,
        }

    print(f"[blender] Kept {len(result)} GPUs after aggregation.")
    return result

def parse_dbgpu(blender_entries: dict[str, dict]) -> dict[str, dict]:
    print("[dbgpu] Loading database...")
    db = dbgpu.GPUDatabase.default()
    all_dbgpu_names = db.names
    norm_to_dbgpu   = {normalise_gpu_name(n): n for n in all_dbgpu_names}

    matched = 0
    unmatched = []

    for norm, entry in blender_entries.items():
        display = entry["display_name"]
        spec = None

        norm_candidates = list(norm_to_dbgpu.keys())
        candidate = fuzz_process.extractOne(norm, norm_candidates)
        if candidate != None:
            if candidate[1] >= FUZZY_THRESHOLD:
                original_name = norm_to_dbgpu[candidate[0]]
                spec = db.search(original_name)

        if spec:
            entry["vram_gb"]            = getattr(spec, "memory_size_gb", None)
            entry["mem_bandwidth_gbps"] = getattr(spec, "memory_bandwidth_gbps", None)
            entry["architecture"]       = getattr(spec, "architecture", None)
            entry["dbgpu_match"]        = spec.name
            matched += 1
        else:
            entry["vram_gb"]            = None
            entry["mem_bandwidth_gbps"] = None
            entry["architecture"]       = None
            entry["dbgpu_match"]        = None
            unmatched.append(display)

    print(f"[dbgpu] Matched {matched}/{len(blender_entries)} GPUs.")
    if unmatched:
        print(f"[dbgpu] {len(unmatched)} unmatched (no hardware data):")
        for name in sorted(unmatched)[:20]:
            print(f"         - {name}")
        if len(unmatched) > 20:
            print(f"         ... and {len(unmatched) - 20} more")

    return blender_entries

def build_output(entries: dict[str, dict]) -> dict:
    gpus = []
    for norm, entry in entries.items():
        vram = entry.get("vram_gb")
        if type(vram) is float:
            if vram < 0.5:
                vram = 0

        gpus.append({
            "name":               entry["display_name"],
            "name_normalised":    norm,
            "blender_score":      entry["blender_score"],
            "scene_scores":       entry.get("scene_scores", {}),
            "sample_count":       entry["sample_count"],
            "vram_gb":            vram,
            "mem_bandwidth_gbps": entry.get("mem_bandwidth_gbps"),
            "architecture":       entry.get("architecture"),
            "dbgpu_match":        entry.get("dbgpu_match"),
        })

    # Sort descending by blender_score as an optimization, so the C++ side can do binary search
    gpus.sort(key=lambda g: g["blender_score"], reverse=True)

    return {
        "version":   1,
        "gpu_count": len(gpus),
        "gpus":      gpus,
    }

def print_summary(output: dict):
    gpus = output["gpus"]
    with_vram    = sum(1 for g in gpus if g["vram_gb"] is not None)
    without_vram = len(gpus) - with_vram

    scores = [g["blender_score"] for g in gpus]
    print("\n========== Database Summary ==========")
    print(f"  Total GPUs:         {len(gpus)}")
    print(f"  With VRAM data:     {with_vram}")
    print(f"  Without VRAM data:  {without_vram}")
    print(f"  Score range:        {min(scores):.2f} – {max(scores):.2f} samples/min")
    print(f"  Top 5 GPUs:")
    for g in gpus[:5]:
        vram = f"{g['vram_gb']}GB" if g["vram_gb"] else "VRAM unknown"
        print(f"    {g['name']:<45} score={g['blender_score']:.2f}  {vram}")
    print("======================================\n")

def main():
    parser = argparse.ArgumentParser(description="Build Veil GPU database")
    parser.add_argument("--no-download", action="store_true",
                        help="Use cached blender snapshot zip, do not re-download")
    parser.add_argument("--force-download", action="store_true",
                        help="Re-download even if cache exists")
    parser.add_argument("--out", default="gpu_database.json",
                        help="Output JSON path (default: gpu_database.json)")
    args = parser.parse_args()

    if args.no_download:
        if not BLENDER_CACHE_PATH.exists():
            print(f"[error] --no-download specified but {BLENDER_CACHE_PATH} not found.")
            sys.exit(1)
        zip_path = BLENDER_CACHE_PATH
    else:
        zip_path = download_blender_snapshot(force=args.force_download)

    blender_entries = parse_blender_snapshot(zip_path)
    full_data = parse_dbgpu(blender_entries)
    output = build_output(full_data)

    out_path = Path(args.out)
    with open(out_path, "w", encoding="utf-8") as f:
        json.dump(output, f, indent=2, ensure_ascii=False)
    print(f"[done] Written to {out_path}  ({out_path.stat().st_size // 1024} KB)")

    print_summary(output)

if __name__ == "__main__":
    main()