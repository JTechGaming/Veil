#include "ShaderLoader.hpp"

#include <fstream>
#include <iostream>

namespace Veil {
    VkShaderModule ShaderLoader::loadShaderModule(VkDevice device, const std::string& path) {
        std::ifstream file(path, std::ios::ate | std::ios::binary);
        size_t fileSize = file.tellg();
        std::vector<uint32_t> buffer(fileSize / 4);
        file.seekg(0);
        file.read(reinterpret_cast<char*>(buffer.data()), fileSize);

        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = fileSize;
        createInfo.pCode = buffer.data();

        VkShaderModule shaderModule;
        vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule);
        return shaderModule;
    }
}
