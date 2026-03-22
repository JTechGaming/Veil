#pragma once
#include "Benchmark.h"
#include "GpuDatabase.hpp"
#include "VulkanDevice.h"

namespace Veil {
    enum class Step { FirstLaunch, Calibrate, PickTarget, Emulate };
    
    class VeilUI {
    public:
        void init(int hostIndex, VulkanDevice* device, GpuDatabase* database, const std::vector<int>& weakerGpuIndices, Benchmark* benchmark);
        void render();
        void renderFirstLaunch();

    private:
        Step m_currentStep = Step::FirstLaunch;
        
        int m_hostIndex = 0;
        VulkanDevice* m_device = nullptr;
        GpuDatabase* m_database = nullptr;
        Benchmark* m_benchmark = nullptr;
        std::vector<int> m_weakerGpuIndices;

        void renderCalibrate();
        void renderPickTarget();
        void renderEmulate();
        
        bool m_calibrated = false;
        bool m_calibrating = false;
        float m_calibrationProgress = 0.0f;
        
        char m_searchBuffer[256]{};
        int m_selectedTargetIndex = -1;
        
        float m_hostScore = 0.0f;
        float m_targetScore = 0.0f;
        float m_currentThrottledScore = 0.0f;
        
        float m_conversionFactor = 0.0f;
        
        bool m_isEmulating = false;
    };
}
