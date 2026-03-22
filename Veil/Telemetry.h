#pragma once

#include <thread>
#include <string>
#include <curl/curl.h>

namespace Veil {
    struct AdjustmentFactors {
        // populated once throttle engine is implemented
    };

    struct GPUTelemetry {
        std::string gpuName;
        uint64_t vramBytes;
        float blenderScore;
    };

    struct TelemetrySession {
        GPUTelemetry hostGpu;
        GPUTelemetry targetGpu;
        float conversionFactor;
        float expectedVeilTargetScore;
        float achievedVeilScore;
        AdjustmentFactors adjustmentFactors;
    };

    class Telemetry {
    public:
        void submit(const TelemetrySession& session);
    };

    inline Telemetry g_Telemetry;
}
