#pragma once

#include <vector>
#include <xstring>
#include <vulkan/vulkan_core.h>

namespace Veil {
    class ShaderLoader {
    public:
        static VkShaderModule loadShaderModule(VkDevice device, const std::string& path);
    };
}
