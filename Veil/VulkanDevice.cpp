#include "VulkanDevice.h"

void VulkanDevice::init(VkPhysicalDevice physicalDevice) {
    VkPhysicalDeviceProperties2 properties2{};
    properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    vkGetPhysicalDeviceProperties2(physicalDevice, &properties2);
    
    m_gpuName = properties2.properties.deviceName;
    
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    m_vramBytes = 0;
    for (uint32_t i = 0; i < memProperties.memoryHeapCount; i++) {
        if (memProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            m_vramBytes = std::max(m_vramBytes, memProperties.memoryHeaps[i].size);
        }
    }
}
