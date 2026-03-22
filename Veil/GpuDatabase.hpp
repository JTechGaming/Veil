#pragma once
#include <algorithm>
#include <string>
#include <vector>
#include <optional>

namespace Veil {
    struct GpuEntry {
        std::string name;
        std::string nameNormalised;
        float blenderScore = 0.0f;
        std::optional<float> vram;
        std::optional<float> memBandwidthGbps;
        std::optional<std::string> architecture;
    };

    class GpuDatabase {
    public:
        void load(const std::string& path);
        int findByName(const std::string& name);
        std::vector<int> getWeakerGpuIndices(int hostIndex) const;
        const GpuEntry& get(int index) const;
        
    private:
        std::vector<GpuEntry> m_loadedGpus;
        
        const float FUZZY_THRESHOLD = 82;
    };
    
    static std::string normaliseGpuName(const std::string& name) {
        std::string result = name;
        // lowercase
        std::transform(result.begin(), result.end(), result.begin(),
            [](unsigned char c) { return std::tolower(c); });
        // strip vendor tokens
        for (const auto& token : {"nvidia", "amd", "intel", "geforce", "radeon",
                                   "graphics", "(r)", "(tm)", "series", "oem",
                                   "mobile", "laptop", "notebook"}) {
            size_t pos;
            while ((pos = result.find(token)) != std::string::npos)
                result.replace(pos, strlen(token), " ");
                                   }
        // collapse whitespace
        std::string out;
        bool lastSpace = false;
        for (char c : result) {
            if (c == ' ') { if (!lastSpace) out += ' '; lastSpace = true; }
            else { out += c; lastSpace = false; }
        }
        // trim
        while (!out.empty() && out.front() == ' ') out.erase(out.begin());
        while (!out.empty() && out.back() == ' ') out.pop_back();
        return out;
    }
    
    inline GpuDatabase g_GpuDatabase;
}
