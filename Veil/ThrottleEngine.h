#pragma once
#include <atomic>
#include <thread>
#include <vector>

#include "Benchmark.h"
#include "VulkanDevice.h"

namespace Veil {
    class ThrottleEngine {
    public:
        void init(VkDevice device, VkPhysicalDevice physicalDevice, VkQueue queue,
                  uint32_t queueFamilyIndex, std::mutex* queueMutex, Veil::Benchmark* benchmark);

        void start(float targetScore);
        void stop();

        bool isRunning();

        [[nodiscard]] float getCurrentScore() const {
            return m_currentScore.load();
        }

        ~ThrottleEngine();
        void runComputeStall(uint32_t iterations);
        void runOverdraw(uint32_t multiplier);

        bool isVramClamped() const {
            return m_vramClamped;
        }

        void setVramClampGb(float targetVramGb);
        void clampVram(uint64_t hostVramBytes, float targetVramGb);
        void releaseVramClamp();

    private:
        VkDevice m_device;
        VkPhysicalDevice m_physicalDevice;
        VkQueue m_queue;
        uint32_t m_queueFamilyIndex;

        VkCommandPool m_commandPool;

        float m_targetScore;

        Veil::Benchmark* m_benchmark = nullptr;

        std::atomic<bool> m_running{false};
        std::atomic<float> m_currentScore{0.0f};
        std::thread m_thread;
        std::mutex* m_queueMutex = nullptr;

        float m_vramClamp;
        bool m_vramClamped;
        std::vector<VkDeviceMemory> m_vramClampAllocations;

        // compute stall pipeline
        VkBuffer m_stallBuffer = VK_NULL_HANDLE;
        VkDeviceMemory m_stallMemory = VK_NULL_HANDLE;
        VkDescriptorSetLayout m_stallDescriptorSetLayout = VK_NULL_HANDLE;
        VkDescriptorPool m_stallDescriptorPool = VK_NULL_HANDLE;
        VkDescriptorSet m_stallDescriptorSet = VK_NULL_HANDLE;
        VkPipelineLayout m_stallPipelineLayout = VK_NULL_HANDLE;
        VkPipeline m_stallPipeline = VK_NULL_HANDLE;

        // overdraw pipeline
        VkImage m_overdrawImage = VK_NULL_HANDLE;
        VkDeviceMemory m_overdrawMemory = VK_NULL_HANDLE;
        VkImageView m_overdrawImageView = VK_NULL_HANDLE;
        VkRenderPass m_overdrawRenderPass = VK_NULL_HANDLE;
        VkFramebuffer m_overdrawFramebuffer = VK_NULL_HANDLE;
        VkPipelineLayout m_overdrawPipelineLayout = VK_NULL_HANDLE;
        VkPipeline m_overdrawPipeline = VK_NULL_HANDLE;

        VkRect2D m_overdrawScissor{};

        void initStallPipeline();
        void initOverdrawPipeline();
    };
}
