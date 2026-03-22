#include "ThrottleEngine.h"

#include "ShaderLoader.hpp"

using namespace Veil;

void ThrottleEngine::init(VkDevice device, VkPhysicalDevice physicalDevice, VkQueue queue,
                          uint32_t queueFamilyIndex, std::mutex* queueMutex, Veil::Benchmark* benchmark) {
    m_device = device;
    m_physicalDevice = physicalDevice;
    m_queue = queue;
    m_queueFamilyIndex = queueFamilyIndex;
    m_queueMutex = queueMutex;
    m_benchmark = benchmark;

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = m_queueFamilyIndex;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    VkResult result = vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create command pool for benchmark");
    }

    initStallPipeline();
    initOverdrawPipeline();
}

void ThrottleEngine::start(float targetScore) {
    if (m_running.load()) return;
    if (m_thread.joinable()) m_thread.join();

    m_targetScore = targetScore;
    m_running.store(true);

    m_thread = std::thread([this]() {
        float stallIterations = 0.0f;
        float overdrawMultiplier = 1.0f;
        int measureCounter = 0;

        while (m_running.load()) {
            if (measureCounter++ % 10 == 0) {
                float current = m_benchmark->measureScore();
                m_currentScore.store(current);

                float delta = current - m_targetScore;
                if (delta > 0.0f) {
                    stallIterations += delta * 2.0f;
                    overdrawMultiplier += delta * 1.0f;
                }
                else if (delta < -m_targetScore * 0.03f) {
                    stallIterations = std::max(0.0f, stallIterations - 0.5f);
                    overdrawMultiplier = std::max(1.0f, overdrawMultiplier - 0.25f);
                }
            }

            runComputeStall(static_cast<uint32_t>(stallIterations));
            runOverdraw(static_cast<uint32_t>(overdrawMultiplier));
        }
    });
}

void ThrottleEngine::stop() {
    releaseVramClamp();

    m_running.store(false);
}

bool ThrottleEngine::isRunning() {
    return m_running;
}

ThrottleEngine::~ThrottleEngine() {
    m_running.store(false);
    if (m_thread.joinable())
        m_thread.join();

    releaseVramClamp();

    if (m_commandPool != VK_NULL_HANDLE)
        vkDestroyCommandPool(m_device, m_commandPool, nullptr);

    vkDestroyPipeline(m_device, m_stallPipeline, nullptr);
    vkDestroyPipelineLayout(m_device, m_stallPipelineLayout, nullptr);
    vkDestroyDescriptorPool(m_device, m_stallDescriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_stallDescriptorSetLayout, nullptr);
    vkFreeMemory(m_device, m_stallMemory, nullptr);
    vkDestroyBuffer(m_device, m_stallBuffer, nullptr);
    vkDestroyPipeline(m_device, m_overdrawPipeline, nullptr);
    vkDestroyPipelineLayout(m_device, m_overdrawPipelineLayout, nullptr);
    vkDestroyFramebuffer(m_device, m_overdrawFramebuffer, nullptr);
    vkDestroyRenderPass(m_device, m_overdrawRenderPass, nullptr);
    vkDestroyImageView(m_device, m_overdrawImageView, nullptr);
    vkFreeMemory(m_device, m_overdrawMemory, nullptr);
    vkDestroyImage(m_device, m_overdrawImage, nullptr);
}

void ThrottleEngine::runComputeStall(uint32_t iterations) {
    uint32_t workgroups = 1024 / 64; // divide by 64 for local gpu group size

    // allocate and record it
    VkCommandBufferAllocateInfo cmdAllocInfo{};
    cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAllocInfo.commandPool = m_commandPool;
    cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAllocInfo.commandBufferCount = 1;

    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(m_device, &cmdAllocInfo, &cmd);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_stallPipeline);

    vkCmdBindDescriptorSets(
        cmd,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        m_stallPipelineLayout,
        0, // first set index
        1,
        &m_stallDescriptorSet,
        0,
        nullptr // no dynamic offsets
    );

    for (uint32_t i = 0; i < iterations; i++) {
        vkCmdDispatch(cmd, workgroups, 1, 1); // 1D workload over the buffer
    }

    vkEndCommandBuffer(cmd);

    // submit and wait
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

    VkFence fence;
    vkCreateFence(m_device, &fenceInfo, nullptr, &fence);

    {
        std::scoped_lock lock(*m_queueMutex);
        vkQueueSubmit(m_queue, 1, &submitInfo, fence);
    }
    vkWaitForFences(m_device, 1, &fence, VK_TRUE, UINT64_MAX);

    vkDestroyFence(m_device, fence, nullptr);
    vkFreeCommandBuffers(m_device, m_commandPool, 1, &cmd);
}

void ThrottleEngine::runOverdraw(uint32_t multiplier) {
    // allocate and record it
    VkCommandBufferAllocateInfo cmdAllocInfo{};
    cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAllocInfo.commandPool = m_commandPool;
    cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAllocInfo.commandBufferCount = 1;

    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(m_device, &cmdAllocInfo, &cmd);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);

    // draw
    VkClearValue clearValue{};
    clearValue.color = {{0.0f, 0.0f, 0.0f, 1.0f}};

    VkRenderPassBeginInfo passBeginInfo{};
    passBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    passBeginInfo.renderPass = m_overdrawRenderPass;
    passBeginInfo.framebuffer = m_overdrawFramebuffer;
    passBeginInfo.renderArea = m_overdrawScissor;
    passBeginInfo.clearValueCount = 1;
    passBeginInfo.pClearValues = &clearValue;

    vkCmdBeginRenderPass(cmd, &passBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_overdrawPipeline);

    for (uint32_t i = 0; i < multiplier; i++) {
        vkCmdDraw(cmd, 3, 1, 0, 0);
    }

    vkCmdEndRenderPass(cmd);
    vkEndCommandBuffer(cmd);

    // submit and wait
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

    VkFence fence;
    vkCreateFence(m_device, &fenceInfo, nullptr, &fence);

    {
        std::scoped_lock lock(*m_queueMutex);
        vkQueueSubmit(m_queue, 1, &submitInfo, fence);
    }
    vkWaitForFences(m_device, 1, &fence, VK_TRUE, UINT64_MAX);

    vkDestroyFence(m_device, fence, nullptr);
    vkFreeCommandBuffers(m_device, m_commandPool, 1, &cmd);
}

void ThrottleEngine::setVramClampGb(float targetVramGb) {
    m_vramClamp = targetVramGb;
}

void ThrottleEngine::clampVram(uint64_t hostVramBytes, float targetVramGb) {
    releaseVramClamp();

    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProps);

    uint32_t memTypeIndex = UINT32_MAX;
    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
        if (memProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
            memTypeIndex = i;
            break;
        }
    }
    if (memTypeIndex == UINT32_MAX) {
        m_vramClamped = false;
        return;
    }

    VkDeviceSize totalClamp = static_cast<VkDeviceSize>(
        (hostVramBytes - static_cast<VkDeviceSize>(targetVramGb * 1e9f)) * 0.85f
    );
    VkDeviceSize chunkSize = 256 * 1024 * 1024; // 256MB chunks
    VkDeviceSize allocated = 0;

    while (allocated < totalClamp) {
        VkDeviceSize remaining = totalClamp - allocated;
        VkDeviceSize thisChunk = std::min(remaining, chunkSize);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = thisChunk;
        allocInfo.memoryTypeIndex = memTypeIndex;

        VkDeviceMemory mem;
        VkResult result = vkAllocateMemory(m_device, &allocInfo, nullptr, &mem);
        if (result != VK_SUCCESS) {
            // can't allocate more — stop here, partial clamp is still useful
            break;
        }

        m_vramClampAllocations.push_back(mem);
        allocated += thisChunk;
    }

    m_vramClamped = !m_vramClampAllocations.empty();
}

void ThrottleEngine::releaseVramClamp() {
    for (VkDeviceMemory mem : m_vramClampAllocations) {
        vkFreeMemory(m_device, mem, nullptr);
    }
    m_vramClampAllocations.clear();
    m_vramClamped = false;
}

void ThrottleEngine::initStallPipeline() {
    // buffer
    VkBufferCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    info.size = 1024 * sizeof(float);
    info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    vkCreateBuffer(m_device, &info, nullptr, &m_stallBuffer);

    VkMemoryRequirements memoryRequirements{};
    vkGetBufferMemoryRequirements(m_device, m_stallBuffer, &memoryRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memoryRequirements.size;
    allocInfo.memoryTypeIndex = m_benchmark->findMemoryType(
        memoryRequirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );

    vkAllocateMemory(m_device, &allocInfo, nullptr, &m_stallMemory);
    vkBindBufferMemory(m_device, m_stallBuffer, m_stallMemory, 0);

    // descriptor set layout
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0; // storage buffer
    binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &binding;

    vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &m_stallDescriptorSetLayout);

    // pool
    VkDescriptorPoolSize poolSize;
    poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;

    vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_stallDescriptorPool);

    VkDescriptorSetAllocateInfo setAllocInfo{};
    setAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    setAllocInfo.descriptorPool = m_stallDescriptorPool;
    setAllocInfo.descriptorSetCount = 1;
    setAllocInfo.pSetLayouts = &m_stallDescriptorSetLayout;

    vkAllocateDescriptorSets(m_device, &setAllocInfo, &m_stallDescriptorSet);

    // update pool to point to buffer
    VkDescriptorBufferInfo bufferInfo;
    bufferInfo.buffer = m_stallBuffer;
    bufferInfo.offset = 0;
    bufferInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_stallDescriptorSet;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write.pBufferInfo = &bufferInfo;

    vkUpdateDescriptorSets(m_device, 1, &write, 0, nullptr);

    // compute pipeline
    VkShaderModule computeShader = Veil::ShaderLoader::loadShaderModule(m_device, "shaders/stall_comp.spv");

    VkPipelineShaderStageCreateInfo stageInfo{};
    stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageInfo.module = computeShader;
    stageInfo.pName = "main";

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &m_stallDescriptorSetLayout;

    vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_stallPipelineLayout);

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage = stageInfo;
    pipelineInfo.layout = m_stallPipelineLayout;

    vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_stallPipeline);

    vkDestroyShaderModule(m_device, computeShader, nullptr);
}

void ThrottleEngine::initOverdrawPipeline() {
    m_overdrawScissor = {{0, 0}, {1920, 1080}};

    VkImageCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.format = VK_FORMAT_R8G8B8A8_UNORM;
    info.imageType = VK_IMAGE_TYPE_2D;
    info.extent = {1920, 1080, 1};
    info.mipLevels = 1;
    info.arrayLayers = 1;
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    vkCreateImage(m_device, &info, nullptr, &m_overdrawImage);

    VkMemoryRequirements memoryRequirements{};
    vkGetImageMemoryRequirements(m_device, m_overdrawImage, &memoryRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memoryRequirements.size;
    allocInfo.memoryTypeIndex = m_benchmark->findMemoryType(
        memoryRequirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );

    vkAllocateMemory(m_device, &allocInfo, nullptr, &m_overdrawMemory);
    vkBindImageMemory(m_device, m_overdrawImage, m_overdrawMemory, 0);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_overdrawImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    vkCreateImageView(m_device, &viewInfo, nullptr, &m_overdrawImageView);

    // pass
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = VK_FORMAT_R8G8B8A8_UNORM;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;

    vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_overdrawRenderPass);

    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = m_overdrawRenderPass;
    framebufferInfo.attachmentCount = 1;
    framebufferInfo.pAttachments = &m_overdrawImageView;
    framebufferInfo.width = 1920;
    framebufferInfo.height = 1080;
    framebufferInfo.layers = 1;

    vkCreateFramebuffer(m_device, &framebufferInfo, nullptr, &m_overdrawFramebuffer);

    // shaders
    VkShaderModule vertexShader = Veil::ShaderLoader::loadShaderModule(m_device, "shaders/fillrate_vert.spv");
    VkShaderModule fragmentShader = Veil::ShaderLoader::loadShaderModule(m_device, "shaders/fillrate_frag.spv");

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertexShader;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragmentShader;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    // pipeline layout
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkViewport viewport{0.0f, 0.0f, 1920.0f, 1080.0f, 0.0f, 1.0f};

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &m_overdrawScissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT
        | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_overdrawPipelineLayout);

    // pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.layout = m_overdrawPipelineLayout;
    pipelineInfo.renderPass = m_overdrawRenderPass;
    pipelineInfo.subpass = 0;

    vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_overdrawPipeline);

    // destroy shaders
    vkDestroyShaderModule(m_device, fragmentShader, nullptr);
    vkDestroyShaderModule(m_device, vertexShader, nullptr);
}
