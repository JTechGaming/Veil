#pragma once
#include <string>
#include <vulkan/vulkan_core.h>

class VulkanDevice {
public:
    void init(VkPhysicalDevice physicalDevice);

    [[nodiscard]] std::string getGpuName() const {
        return m_gpuName;
    }

    [[nodiscard]] uint64_t getVramBytes() const {
        return m_vramBytes;
    }

private:
    std::string m_gpuName;
    uint64_t m_vramBytes;
};
