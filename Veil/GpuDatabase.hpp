#pragma once
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
        const GpuEntry& get(int index) const;
        
    private:
        std::vector<GpuEntry> m_loadedGpus;
        
        constexpr float FUZZY_THRESHOLD = 82;
    };
    
    inline GpuDatabase g_GpuDatabase;
}
