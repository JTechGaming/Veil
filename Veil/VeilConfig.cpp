#include "VeilConfig.h"

#include <fstream>
#include <iostream>
#include <json.hpp>

namespace Veil {
    void VeilConfig::load(const std::string& path) {
        std::ifstream file(path);
        m_path = path;
        if (!file.is_open()) {
            return;
        }
        
        try {
            nlohmann::json data = nlohmann::json::parse(file);
            m_data.firstLaunchComplete = data.value("firstLaunchComplete", false);
            m_data.telemetryEnabled    = data.value("telemetryEnabled", false);
        } catch (...) {
            std::cout << "Loaded defaults\n";
        }
    }

    void VeilConfig::save() {
        nlohmann::json data;
        
        data["firstLaunchComplete"] = m_data.firstLaunchComplete;
        data["telemetryEnabled"] = m_data.telemetryEnabled;
        
        std::ofstream file(m_path);
        if (!file.is_open())  {
            std::cout << "\n Failed to open output file";
        } else {
            file << data;
            file.close();
        }
    }
}
