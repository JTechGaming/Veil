#pragma once
#include <string>

namespace Veil {
    struct ConfigData {
        bool telemetryEnabled = false;
        bool firstLaunchComplete = false;
    };

    class VeilConfig {
    public:
        void load(const std::string& path);
        void save();

        [[nodiscard]] ConfigData& getData() {
            return m_data;
        }
    private:
        ConfigData m_data;
        std::string m_path;
    };

    inline VeilConfig g_VeilConfig{};
}
