#include "GpuDatabase.hpp"

#include <fstream>
#include "json.hpp"
#include "fuzz.hpp"

namespace Veil {
    void GpuDatabase::load(const std::string& path) {
        std::ifstream f(path);
        nlohmann::json data = nlohmann::json::parse(f);

        auto& gpus = data["gpus"];
        
        m_loadedGpus.reserve(gpus.size());
        
        for (const auto& entry : gpus) {
            GpuEntry gpuEntry;
            gpuEntry.name = entry["name"].get<std::string>();
            gpuEntry.nameNormalised = entry["name_normalised"].get<std::string>();
            gpuEntry.blenderScore = entry["blender_score"].get<float>();
            gpuEntry.vram = entry["vram_gb"].is_null() ? std::nullopt : std::optional(entry["vram_gb"].get<float>());
            gpuEntry.memBandwidthGbps = entry["mem_bandwidth_gbps"].is_null() ? std::nullopt : std::optional(entry["mem_bandwidth_gbps"].get<float>());
            gpuEntry.architecture = entry["architecture"].is_null() ? std::nullopt : std::optional(entry["architecture"].get<std::string>());
            
            m_loadedGpus.emplace_back(gpuEntry);
        }
    }

    int GpuDatabase::findByName(const std::string& name) {
        std::string normalisedName = name;
        std::transform(normalisedName.begin(), normalisedName.end(), normalisedName.begin(),
                   [](unsigned char c){ return std::tolower(c); });
        int bestIndex = 0;
        double bestScore = 0.0f;
        for (int i = 0; i < static_cast<int>(m_loadedGpus.size()); i++) {
            auto& candidate = m_loadedGpus[i];
            double score = rapidfuzz::fuzz::ratio(normalisedName, candidate.nameNormalised);
            if (score > bestScore) {
                bestScore = score;
                bestIndex = i;
            }
        }
        return bestScore >= FUZZY_THRESHOLD ? bestIndex : -1;
    }

    const GpuEntry& GpuDatabase::get(int index) const {
        if (index < 0 || index >= static_cast<int>(m_loadedGpus.size())) {
            throw std::out_of_range("index out of range for m_loadedGpus");
        }
        return m_loadedGpus[index];
    }
}
