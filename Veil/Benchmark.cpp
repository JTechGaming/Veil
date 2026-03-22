#include "Benchmark.h"

#include <iostream>
#include <stdexcept>

#include "ShaderLoader.hpp"

namespace Veil {
    void Benchmark::init(VkDevice device, VkPhysicalDevice physicalDevice, VkQueue queue, uint32_t queueFamilyIndex, std::mutex* queueMutex) {
        m_device = device;
        m_physicalDevice = physicalDevice;
        m_queue = queue;
        m_queueFamilyIndex = queueFamilyIndex;
        m_queueMutex = queueMutex;

        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = m_queueFamilyIndex;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        VkResult result = vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to create command pool for benchmark");
        }

        vkGetPhysicalDeviceProperties(m_physicalDevice, &m_deviceProps);
    }

    void Benchmark::run() {
        if (m_running.load())
            return;

        if (m_thread.joinable())
            m_thread.join();

        m_complete.store(false);
        m_progress.store(0.0f);
        m_score.store(0.0f);
        m_running.store(true);

        vkDeviceWaitIdle(m_device); // prevent crash in very rare cases
        m_thread = std::thread([this]() {
            float fillRate = runFillRatePass();
            m_progress.store(0.25f);
            std::cout << "fillrate\n";

            float bandwidth = runBandwidthPass();
            m_progress.store(0.5f);
            std::cout << "bandwidth\n";

            float compute = runComputePass();
            m_progress.store(0.75f);
            std::cout << "compute\n";

            float drawCalls = runDrawCallOverheadPass();
            m_progress.store(1.0f);
            std::cout << "drawcall\n";

            // normalise each score against expected high-end values, then take the average
            float normFillRate = fillRate / 1000000.0f;
            float normBandwidth = bandwidth / 1000.0f;
            float normCompute = compute / 100.0f;
            float normDrawCalls = drawCalls / 1000.0f;

            float composite = (normFillRate + normBandwidth + normCompute + normDrawCalls) / 4.0f;

            m_score.store(composite);
            m_running.store(false);
            m_complete.store(true);
        });
    }

    static constexpr uint32_t FILL_RATE_REPEAT_COUNT = 10000;
    static constexpr uint32_t DRAW_CALL_COUNT = 100000;

    float Benchmark::runFillRatePass() {
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

        VkImage renderTarget;
        vkCreateImage(m_device, &info, nullptr, &renderTarget);

        VkMemoryRequirements memoryRequirements{};
        vkGetImageMemoryRequirements(m_device, renderTarget, &memoryRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memoryRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(
            memoryRequirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );

        VkDeviceMemory renderTargetMemory;
        vkAllocateMemory(m_device, &allocInfo, nullptr, &renderTargetMemory);
        vkBindImageMemory(m_device, renderTarget, renderTargetMemory, 0);

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = renderTarget;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        VkImageView renderTargetView;
        vkCreateImageView(m_device, &viewInfo, nullptr, &renderTargetView);

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

        VkRenderPass renderPass;
        vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &renderPass);

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = &renderTargetView;
        framebufferInfo.width = 1920;
        framebufferInfo.height = 1080;
        framebufferInfo.layers = 1;

        VkFramebuffer framebuffer;
        vkCreateFramebuffer(m_device, &framebufferInfo, nullptr, &framebuffer);

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
        VkRect2D scissor{{0, 0}, {1920, 1080}};

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.pViewports = &viewport;
        viewportState.scissorCount = 1;
        viewportState.pScissors = &scissor;

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

        VkPipelineLayout pipelineLayout;
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &pipelineLayout);

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
        pipelineInfo.layout = pipelineLayout;
        pipelineInfo.renderPass = renderPass;
        pipelineInfo.subpass = 0;

        VkPipeline pipeline;
        vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);

        // destroy shaders
        vkDestroyShaderModule(m_device, fragmentShader, nullptr);
        vkDestroyShaderModule(m_device, vertexShader, nullptr);

        // timestamp query pool
        VkQueryPoolCreateInfo queryPoolInfo{};
        queryPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        queryPoolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
        queryPoolInfo.queryCount = 2; // start and end timestamp

        VkQueryPool queryPool;
        vkCreateQueryPool(m_device, &queryPoolInfo, nullptr, &queryPool);

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
        vkCmdResetQueryPool(cmd, queryPool, 0, 2); // start and end timestamp

        VkClearValue clearValue{};
        clearValue.color = {{0.0f, 0.0f, 0.0f, 1.0f}};

        VkRenderPassBeginInfo passBeginInfo{};
        passBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        passBeginInfo.renderPass = renderPass;
        passBeginInfo.framebuffer = framebuffer;
        passBeginInfo.renderArea = scissor;
        passBeginInfo.clearValueCount = 1;
        passBeginInfo.pClearValues = &clearValue;

        vkCmdBeginRenderPass(cmd, &passBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, queryPool, 0);
        vkCmdDraw(cmd, 3, FILL_RATE_REPEAT_COUNT, 0, 0);
        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queryPool, 1);

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

        // read back timestamps
        uint64_t timestamps[2];
        vkGetQueryPoolResults(
            m_device, queryPool, 0, 2,
            sizeof(timestamps), timestamps,
            sizeof(uint64_t),
            VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT
        );

        float timestampPeriod = m_deviceProps.limits.timestampPeriod;

        float gpuTimeMs = (timestamps[1] - timestamps[0]) * timestampPeriod / 1e6f;
        float pixelsPerMs = (1920.0f * 1080.0f * FILL_RATE_REPEAT_COUNT) / gpuTimeMs;

        // cleanup
        vkDestroyFence(m_device, fence, nullptr);
        vkFreeCommandBuffers(m_device, m_commandPool, 1, &cmd);
        vkDestroyQueryPool(m_device, queryPool, nullptr);
        vkDestroyPipeline(m_device, pipeline, nullptr);
        vkDestroyPipelineLayout(m_device, pipelineLayout, nullptr);
        vkDestroyFramebuffer(m_device, framebuffer, nullptr);
        vkDestroyRenderPass(m_device, renderPass, nullptr);
        vkDestroyImageView(m_device, renderTargetView, nullptr);
        vkFreeMemory(m_device, renderTargetMemory, nullptr);
        vkDestroyImage(m_device, renderTarget, nullptr);

        return pixelsPerMs;
    }

    static constexpr int BANDWIDTH_BUFFER_SIZE = 256 * 1024 * 1024;

    float Benchmark::runBandwidthPass() {
        // buffer
        VkBufferCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        info.size = BANDWIDTH_BUFFER_SIZE;
        info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VkBuffer buffer;
        vkCreateBuffer(m_device, &info, nullptr, &buffer);

        VkMemoryRequirements memoryRequirements{};
        vkGetBufferMemoryRequirements(m_device, buffer, &memoryRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memoryRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(
            memoryRequirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );

        VkDeviceMemory renderTargetMemory;
        vkAllocateMemory(m_device, &allocInfo, nullptr, &renderTargetMemory);
        vkBindBufferMemory(m_device, buffer, renderTargetMemory, 0);

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

        VkDescriptorSetLayout descriptorSetLayout;
        vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &descriptorSetLayout);

        // pool
        VkDescriptorPoolSize poolSize;
        poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        poolSize.descriptorCount = 1;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.maxSets = 1;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;

        VkDescriptorPool descriptorPool;
        vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &descriptorPool);

        VkDescriptorSetAllocateInfo setAllocInfo{};
        setAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        setAllocInfo.descriptorPool = descriptorPool;
        setAllocInfo.descriptorSetCount = 1;
        setAllocInfo.pSetLayouts = &descriptorSetLayout;

        VkDescriptorSet descriptorSet;
        vkAllocateDescriptorSets(m_device, &setAllocInfo, &descriptorSet);

        // update pool to point to buffer
        VkDescriptorBufferInfo bufferInfo;
        bufferInfo.buffer = buffer;
        bufferInfo.offset = 0;
        bufferInfo.range = VK_WHOLE_SIZE;

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = descriptorSet;
        write.dstBinding = 0;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        write.pBufferInfo = &bufferInfo;

        vkUpdateDescriptorSets(m_device, 1, &write, 0, nullptr);

        // compute pipeline
        VkShaderModule computeShader = Veil::ShaderLoader::loadShaderModule(m_device, "shaders/bandwidth_comp.spv");

        VkPipelineShaderStageCreateInfo stageInfo{};
        stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        stageInfo.module = computeShader;
        stageInfo.pName = "main";

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;

        VkPipelineLayout pipelineLayout;
        vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &pipelineLayout);

        VkComputePipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineInfo.stage = stageInfo;
        pipelineInfo.layout = pipelineLayout;

        VkPipeline pipeline;
        vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);

        vkDestroyShaderModule(m_device, computeShader, nullptr);

        uint32_t workgroups = (BANDWIDTH_BUFFER_SIZE / sizeof(float) / 2) / 64;
        // divide by 2 because the shader uses half for read, half for write, divide by 64 for local gpu group size

        // timestamp query pool
        VkQueryPoolCreateInfo queryPoolInfo{};
        queryPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        queryPoolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
        queryPoolInfo.queryCount = 2; // start and end timestamp

        VkQueryPool queryPool;
        vkCreateQueryPool(m_device, &queryPoolInfo, nullptr, &queryPool);

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
        vkCmdResetQueryPool(cmd, queryPool, 0, 2); // start and end timestamp

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);

        vkCmdBindDescriptorSets(
            cmd,
            VK_PIPELINE_BIND_POINT_COMPUTE,
            pipelineLayout,
            0, // first set index
            1,
            &descriptorSet,
            0,
            nullptr // no dynamic offsets
        );

        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, queryPool, 0);
        vkCmdDispatch(cmd, workgroups, 1, 1); // 1D workload over the buffer
        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queryPool, 1);

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

        // read back timestamps
        uint64_t timestamps[2];
        vkGetQueryPoolResults(
            m_device, queryPool, 0, 2,
            sizeof(timestamps), timestamps,
            sizeof(uint64_t),
            VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT
        );

        float timestampPeriod = m_deviceProps.limits.timestampPeriod;

        float gpuTimeMs = (timestamps[1] - timestamps[0]) * timestampPeriod / 1e6f;

        int bytesTransferred = BANDWIDTH_BUFFER_SIZE; // read + write is full buffer size
        float gbPerSec = (bytesTransferred / 1e9f) / (gpuTimeMs / 1000.0f);

        vkDestroyFence(m_device, fence, nullptr);
        vkFreeCommandBuffers(m_device, m_commandPool, 1, &cmd);
        vkDestroyQueryPool(m_device, queryPool, nullptr);
        vkDestroyPipeline(m_device, pipeline, nullptr);
        vkDestroyPipelineLayout(m_device, pipelineLayout, nullptr);
        vkDestroyDescriptorPool(m_device, descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(m_device, descriptorSetLayout, nullptr);
        vkFreeMemory(m_device, renderTargetMemory, nullptr);
        vkDestroyBuffer(m_device, buffer, nullptr);

        return gbPerSec;
    }

    float Benchmark::runComputePass() {
        // buffer
        VkBufferCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        info.size = 1024 * sizeof(float);
        info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VkBuffer buffer;
        vkCreateBuffer(m_device, &info, nullptr, &buffer);

        VkMemoryRequirements memoryRequirements{};
        vkGetBufferMemoryRequirements(m_device, buffer, &memoryRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memoryRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(
            memoryRequirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );

        VkDeviceMemory renderTargetMemory;
        vkAllocateMemory(m_device, &allocInfo, nullptr, &renderTargetMemory);
        vkBindBufferMemory(m_device, buffer, renderTargetMemory, 0);

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

        VkDescriptorSetLayout descriptorSetLayout;
        vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &descriptorSetLayout);

        // pool
        VkDescriptorPoolSize poolSize;
        poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        poolSize.descriptorCount = 1;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.maxSets = 1;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;

        VkDescriptorPool descriptorPool;
        vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &descriptorPool);

        VkDescriptorSetAllocateInfo setAllocInfo{};
        setAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        setAllocInfo.descriptorPool = descriptorPool;
        setAllocInfo.descriptorSetCount = 1;
        setAllocInfo.pSetLayouts = &descriptorSetLayout;

        VkDescriptorSet descriptorSet;
        vkAllocateDescriptorSets(m_device, &setAllocInfo, &descriptorSet);

        // update pool to point to buffer
        VkDescriptorBufferInfo bufferInfo;
        bufferInfo.buffer = buffer;
        bufferInfo.offset = 0;
        bufferInfo.range = VK_WHOLE_SIZE;

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = descriptorSet;
        write.dstBinding = 0;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        write.pBufferInfo = &bufferInfo;

        vkUpdateDescriptorSets(m_device, 1, &write, 0, nullptr);

        // compute pipeline
        VkShaderModule computeShader = Veil::ShaderLoader::loadShaderModule(m_device, "shaders/compute_comp.spv");

        VkPipelineShaderStageCreateInfo stageInfo{};
        stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        stageInfo.module = computeShader;
        stageInfo.pName = "main";

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;

        VkPipelineLayout pipelineLayout;
        vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &pipelineLayout);

        VkComputePipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineInfo.stage = stageInfo;
        pipelineInfo.layout = pipelineLayout;

        VkPipeline pipeline;
        vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);

        vkDestroyShaderModule(m_device, computeShader, nullptr);

        uint32_t workgroups = 1024 / 64; // divide by 64 for local gpu group size

        // timestamp query pool
        VkQueryPoolCreateInfo queryPoolInfo{};
        queryPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        queryPoolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
        queryPoolInfo.queryCount = 2; // start and end timestamp

        VkQueryPool queryPool;
        vkCreateQueryPool(m_device, &queryPoolInfo, nullptr, &queryPool);

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
        vkCmdResetQueryPool(cmd, queryPool, 0, 2); // start and end timestamp

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);

        vkCmdBindDescriptorSets(
            cmd,
            VK_PIPELINE_BIND_POINT_COMPUTE,
            pipelineLayout,
            0, // first set index
            1,
            &descriptorSet,
            0,
            nullptr // no dynamic offsets
        );

        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, queryPool, 0);
        vkCmdDispatch(cmd, workgroups, 1, 1); // 1D workload over the buffer
        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queryPool, 1);

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

        // read back timestamps
        uint64_t timestamps[2];
        vkGetQueryPoolResults(
            m_device, queryPool, 0, 2,
            sizeof(timestamps), timestamps,
            sizeof(uint64_t),
            VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT
        );

        float timestampPeriod = m_deviceProps.limits.timestampPeriod;

        float gpuTimeMs = (timestamps[1] - timestamps[0]) * timestampPeriod / 1e6f;

        float flopsPerInvocation = 1000 * 2; // 1000 iterations, 2 ops per FMA
        float totalFlops = flopsPerInvocation * workgroups * 64;
        float gflops = (totalFlops / 1e9f) / (gpuTimeMs / 1000.0f);

        vkDestroyFence(m_device, fence, nullptr);
        vkFreeCommandBuffers(m_device, m_commandPool, 1, &cmd);
        vkDestroyQueryPool(m_device, queryPool, nullptr);
        vkDestroyPipeline(m_device, pipeline, nullptr);
        vkDestroyPipelineLayout(m_device, pipelineLayout, nullptr);
        vkDestroyDescriptorPool(m_device, descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(m_device, descriptorSetLayout, nullptr);
        vkFreeMemory(m_device, renderTargetMemory, nullptr);
        vkDestroyBuffer(m_device, buffer, nullptr);

        return gflops;
    }

    float Benchmark::runDrawCallOverheadPass() {
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

        VkImage renderTarget;
        vkCreateImage(m_device, &info, nullptr, &renderTarget);

        VkMemoryRequirements memoryRequirements{};
        vkGetImageMemoryRequirements(m_device, renderTarget, &memoryRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memoryRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(
            memoryRequirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );

        VkDeviceMemory renderTargetMemory;
        vkAllocateMemory(m_device, &allocInfo, nullptr, &renderTargetMemory);
        vkBindImageMemory(m_device, renderTarget, renderTargetMemory, 0);

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = renderTarget;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        VkImageView renderTargetView;
        vkCreateImageView(m_device, &viewInfo, nullptr, &renderTargetView);

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

        VkRenderPass renderPass;
        vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &renderPass);

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = &renderTargetView;
        framebufferInfo.width = 1920;
        framebufferInfo.height = 1080;
        framebufferInfo.layers = 1;

        VkFramebuffer framebuffer;
        vkCreateFramebuffer(m_device, &framebufferInfo, nullptr, &framebuffer);

        // shaders
        // can use the same shaders since we dont care what is being drawn
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
        VkRect2D scissor{{0, 0}, {1920, 1080}};

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.pViewports = &viewport;
        viewportState.scissorCount = 1;
        viewportState.pScissors = &scissor;

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

        VkPipelineLayout pipelineLayout;
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &pipelineLayout);

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
        pipelineInfo.layout = pipelineLayout;
        pipelineInfo.renderPass = renderPass;
        pipelineInfo.subpass = 0;

        VkPipeline pipeline;
        vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);

        // destroy shaders
        vkDestroyShaderModule(m_device, fragmentShader, nullptr);
        vkDestroyShaderModule(m_device, vertexShader, nullptr);

        // timestamp query pool
        VkQueryPoolCreateInfo queryPoolInfo{};
        queryPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        queryPoolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
        queryPoolInfo.queryCount = 2; // start and end timestamp

        VkQueryPool queryPool;
        vkCreateQueryPool(m_device, &queryPoolInfo, nullptr, &queryPool);

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
        vkCmdResetQueryPool(cmd, queryPool, 0, 2); // start and end timestamp

        VkClearValue clearValue{};
        clearValue.color = {{0.0f, 0.0f, 0.0f, 1.0f}};

        VkRenderPassBeginInfo passBeginInfo{};
        passBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        passBeginInfo.renderPass = renderPass;
        passBeginInfo.framebuffer = framebuffer;
        passBeginInfo.renderArea = scissor;
        passBeginInfo.clearValueCount = 1;
        passBeginInfo.pClearValues = &clearValue;

        vkCmdBeginRenderPass(cmd, &passBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, queryPool, 0);
        for (uint32_t i = 0; i < DRAW_CALL_COUNT; i++) {
            vkCmdDraw(cmd, 3, 1, 0, 0);
        }
        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queryPool, 1);

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

        // read back timestamps
        uint64_t timestamps[2];
        vkGetQueryPoolResults(
            m_device, queryPool, 0, 2,
            sizeof(timestamps), timestamps,
            sizeof(uint64_t),
            VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT
        );

        float timestampPeriod = m_deviceProps.limits.timestampPeriod;

        float gpuTimeMs = (timestamps[1] - timestamps[0]) * timestampPeriod / 1e6f;
        float drawCallsPerMs = DRAW_CALL_COUNT / gpuTimeMs;

        // cleanup
        vkDestroyFence(m_device, fence, nullptr);
        vkFreeCommandBuffers(m_device, m_commandPool, 1, &cmd);
        vkDestroyQueryPool(m_device, queryPool, nullptr);
        vkDestroyPipeline(m_device, pipeline, nullptr);
        vkDestroyPipelineLayout(m_device, pipelineLayout, nullptr);
        vkDestroyFramebuffer(m_device, framebuffer, nullptr);
        vkDestroyRenderPass(m_device, renderPass, nullptr);
        vkDestroyImageView(m_device, renderTargetView, nullptr);
        vkFreeMemory(m_device, renderTargetMemory, nullptr);
        vkDestroyImage(m_device, renderTarget, nullptr);

        return drawCallsPerMs;
    }
    
    float Benchmark::measureScore() {
        float fillRate  = runFillRatePass();
        float bandwidth = runBandwidthPass();
        float compute   = runComputePass();
        float drawCalls = runDrawCallOverheadPass();

        float normFillRate  = fillRate  / 1000000.0f;
        float normBandwidth = bandwidth / 1000.0f;
        float normCompute   = compute   / 100.0f;
        float normDrawCalls = drawCalls / 1000.0f;

        return (normFillRate + normBandwidth + normCompute + normDrawCalls) / 4.0f;
    }

    uint32_t Benchmark::findMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties) {
        VkPhysicalDeviceMemoryProperties memProps;
        vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProps);
        for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
            if ((typeBits & (1 << i)) &&
                (memProps.memoryTypes[i].propertyFlags & properties) == properties)
                return i;
        }
        throw std::runtime_error("Failed to find suitable memory type");
    }
}
