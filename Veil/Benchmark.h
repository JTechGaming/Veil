#pragma once
#include <atomic>
#include <mutex>
#include <thread>

#include "VulkanDevice.h"

namespace Veil {
    class Benchmark {
    public:
        void init(VkDevice device, VkPhysicalDevice physicalDevice, VkQueue queue, uint32_t queueFamilyIndex, std::mutex* queueMutex);
        void run();

        ~Benchmark() {
            if (m_thread.joinable())
                m_thread.join();
            if (m_commandPool != VK_NULL_HANDLE)
                vkDestroyCommandPool(m_device, m_commandPool, nullptr);
        }

        [[nodiscard]] float getProgress() const {
            return m_progress.load();
        }

        [[nodiscard]] float getScore() const {
            return m_score.load();
        }

        [[nodiscard]] bool getComplete() const {
            return m_complete.load();
        }

        [[nodiscard]] bool getRunning() const {
            return m_running.load();
        }

    private:
        VkDevice m_device;
        VkPhysicalDevice m_physicalDevice;
        VkQueue m_queue;
        uint32_t m_queueFamilyIndex;

        VkCommandPool m_commandPool;

        VkPhysicalDeviceProperties m_deviceProps;
        
        std::mutex* m_queueMutex = nullptr;

        std::thread m_thread;

        std::atomic<float> m_progress{0.0f};
        std::atomic<float> m_score{0.0f};
        std::atomic<bool> m_complete{false};
        std::atomic<bool> m_running{false};

        float runFillRatePass();
        float runBandwidthPass();
        float runComputePass();
        float runDrawCallOverheadPass();
        uint32_t findMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties);
    };
}
