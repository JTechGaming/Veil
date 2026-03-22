#include "Telemetry.h"

#include <json.hpp>

#include "secrets.h"

namespace Veil {
    void Telemetry::submit(const TelemetrySession& session) {
        nlohmann::json data;
        
        data["host_gpu_name"]              = session.hostGpu.gpuName;
        data["host_vram_bytes"]            = session.hostGpu.vramBytes;
        data["host_blender_score"]         = session.hostGpu.blenderScore;
        data["target_gpu_name"]            = session.targetGpu.gpuName;
        data["target_vram_bytes"]          = session.targetGpu.vramBytes;
        data["target_blender_score"]       = session.targetGpu.blenderScore;
        data["conversion_factor"]          = session.conversionFactor;
        data["expected_veil_target_score"] = session.expectedVeilTargetScore;
        data["achieved_veil_score"]        = session.achievedVeilScore;
        
        std::string payload = data.dump();
    
        std::thread([payload]() {
            CURL* curl = curl_easy_init();
            if (!curl) return;

            struct curl_slist* headers = nullptr;
            std::string apiKeyHeader = std::string("apikey: ") + Veil::Secrets::SUPABASE_ANON_KEY;
            std::string authHeader   = std::string("Authorization: Bearer ") + Veil::Secrets::SUPABASE_ANON_KEY;

            headers = curl_slist_append(headers, "Content-Type: application/json");
            headers = curl_slist_append(headers, apiKeyHeader.c_str());
            headers = curl_slist_append(headers, authHeader.c_str());
            headers = curl_slist_append(headers, "Prefer: return=minimal");

            curl_easy_setopt(curl, CURLOPT_URL, 
                (std::string(Veil::Secrets::SUPABASE_URL) + "/rest/v1/sessions").c_str());
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, payload.size());

            curl_easy_perform(curl);

            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
        }).detach();
    }
}
