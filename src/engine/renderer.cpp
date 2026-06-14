#include "renderer.h"
#include "vulkan_core.h"
#include "texture.h"
#include <array>
#include <glm/gtc/matrix_transform.hpp>

Renderer::Renderer(VulkanCore* core, const std::vector<char>& vertShader, const std::vector<char>& fragShader)
    : m_core(core) {
    createDescriptorSetLayouts();
    createPipeline(vertShader, fragShader);
    createVertexBuffer();
    createIndexBuffer();
    createUniformBuffers();
    createDescriptorPool();
    createUboDescriptorSets();
}

Renderer::~Renderer() {
    auto dev = m_core->device();
    vkDestroyPipeline(dev, m_pipeline, nullptr);
    vkDestroyPipelineLayout(dev, m_pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(dev, m_uboSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(dev, m_textureSetLayout, nullptr);
    vkDestroyDescriptorPool(dev, m_descriptorPool, nullptr);

    for (size_t i = 0; i < m_uniformBuffers.size(); i++) {
        vkUnmapMemory(dev, m_uniformBuffersMemory[i]);
        vkDestroyBuffer(dev, m_uniformBuffers[i], nullptr);
        vkFreeMemory(dev, m_uniformBuffersMemory[i], nullptr);
    }
    vkDestroyBuffer(dev, m_indexBuffer, nullptr);
    vkFreeMemory(dev, m_indexBufferMemory, nullptr);
    vkDestroyBuffer(dev, m_vertexBuffer, nullptr);
    vkFreeMemory(dev, m_vertexBufferMemory, nullptr);
}

void Renderer::createDescriptorSetLayouts() {
    auto dev = m_core->device();

    VkDescriptorSetLayoutBinding uboBinding{};
    uboBinding.binding = 0;
    uboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboBinding.descriptorCount = 1;
    uboBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo uboInfo{};
    uboInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    uboInfo.bindingCount = 1;
    uboInfo.pBindings = &uboBinding;
    if (vkCreateDescriptorSetLayout(dev, &uboInfo, nullptr, &m_uboSetLayout) != VK_SUCCESS)
        throw std::runtime_error("failed to create UBO descriptor set layout");

    VkDescriptorSetLayoutBinding texBinding{};
    texBinding.binding = 0;
    texBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    texBinding.descriptorCount = 1;
    texBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo texInfo{};
    texInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    texInfo.bindingCount = 1;
    texInfo.pBindings = &texBinding;
    if (vkCreateDescriptorSetLayout(dev, &texInfo, nullptr, &m_textureSetLayout) != VK_SUCCESS)
        throw std::runtime_error("failed to create texture descriptor set layout");
}

void Renderer::createPipeline(const std::vector<char>& vert, const std::vector<char>& frag) {
    auto dev = m_core->device();
    auto vertModule = m_core->createShaderModule(vert);
    auto fragModule = m_core->createShaderModule(frag);

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertModule;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fragModule;
    stages[1].pName = "main";

    VkVertexInputBindingDescription bindingDesc{};
    bindingDesc.binding = 0;
    bindingDesc.stride = sizeof(Vertex);
    bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attribs[2]{};
    attribs[0].binding = 0;
    attribs[0].location = 0;
    attribs[0].format = VK_FORMAT_R32G32_SFLOAT;
    attribs[0].offset = offsetof(Vertex, position);
    attribs[1].binding = 0;
    attribs[1].location = 1;
    attribs[1].format = VK_FORMAT_R32G32_SFLOAT;
    attribs[1].offset = offsetof(Vertex, texCoord);

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &bindingDesc;
    vertexInput.vertexAttributeDescriptionCount = 2;
    vertexInput.pVertexAttributeDescriptions = attribs;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkViewport viewport{};
    viewport.x = 0; viewport.y = 0;
    viewport.width = (float)m_core->swapchainExtent().width;
    viewport.height = (float)m_core->swapchainExtent().height;
    viewport.minDepth = 0; viewport.maxDepth = 1;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = m_core->swapchainExtent();

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState blend{};
    blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                           VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blend.blendEnable = VK_TRUE;
    blend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend.colorBlendOp = VK_BLEND_OP_ADD;
    blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    blend.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &blend;

    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushRange.offset = 0;
    pushRange.size = sizeof(PushConstants);

    std::array<VkDescriptorSetLayout, 2> layouts = {m_uboSetLayout, m_textureSetLayout};
    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 2;
    layoutInfo.pSetLayouts = layouts.data();
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushRange;
    if (vkCreatePipelineLayout(dev, &layoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS)
        throw std::runtime_error("failed to create pipeline layout");

    VkGraphicsPipelineCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    info.stageCount = 2;
    info.pStages = stages;
    info.pVertexInputState = &vertexInput;
    info.pInputAssemblyState = &inputAssembly;
    info.pViewportState = &viewportState;
    info.pRasterizationState = &rasterizer;
    info.pMultisampleState = &multisampling;
    info.pColorBlendState = &colorBlending;
    info.layout = m_pipelineLayout;
    info.renderPass = m_core->renderPass();

    if (vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &info, nullptr, &m_pipeline) != VK_SUCCESS)
        throw std::runtime_error("failed to create pipeline");

    vkDestroyShaderModule(dev, fragModule, nullptr);
    vkDestroyShaderModule(dev, vertModule, nullptr);
}

void Renderer::createVertexBuffer() {
    auto dev = m_core->device();
    std::vector<Vertex> vertices = {
        {{-0.5f, -0.5f}, {0.0f, 0.0f}},
        {{ 0.5f, -0.5f}, {1.0f, 0.0f}},
        {{ 0.5f,  0.5f}, {1.0f, 1.0f}},
        {{-0.5f,  0.5f}, {0.0f, 1.0f}},
    };
    VkDeviceSize size = sizeof(Vertex) * vertices.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    VkBufferCreateInfo bufInfo{};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size = size;
    bufInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vkCreateBuffer(dev, &bufInfo, nullptr, &stagingBuffer);

    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(dev, stagingBuffer, &memReq);
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = m_core->findMemoryType(memReq.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    vkAllocateMemory(dev, &allocInfo, nullptr, &stagingMemory);
    vkBindBufferMemory(dev, stagingBuffer, stagingMemory, 0);

    void* data;
    vkMapMemory(dev, stagingMemory, 0, size, 0, &data);
    memcpy(data, vertices.data(), (size_t)size);
    vkUnmapMemory(dev, stagingMemory);

    bufInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    vkCreateBuffer(dev, &bufInfo, nullptr, &m_vertexBuffer);
    vkGetBufferMemoryRequirements(dev, m_vertexBuffer, &memReq);
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = m_core->findMemoryType(memReq.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vkAllocateMemory(dev, &allocInfo, nullptr, &m_vertexBufferMemory);
    vkBindBufferMemory(dev, m_vertexBuffer, m_vertexBufferMemory, 0);

    auto cmd = m_core->beginSingleTimeCommands();
    VkBufferCopy copy{};
    copy.size = size;
    vkCmdCopyBuffer(cmd, stagingBuffer, m_vertexBuffer, 1, &copy);
    m_core->endSingleTimeCommands(cmd);

    vkDestroyBuffer(dev, stagingBuffer, nullptr);
    vkFreeMemory(dev, stagingMemory, nullptr);
}

void Renderer::createIndexBuffer() {
    auto dev = m_core->device();
    std::vector<uint16_t> indices = {0, 1, 2, 2, 3, 0};
    VkDeviceSize size = sizeof(uint16_t) * indices.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    VkBufferCreateInfo bufInfo{};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size = size;
    bufInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    vkCreateBuffer(dev, &bufInfo, nullptr, &stagingBuffer);

    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(dev, stagingBuffer, &memReq);
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = m_core->findMemoryType(memReq.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    vkAllocateMemory(dev, &allocInfo, nullptr, &stagingMemory);
    vkBindBufferMemory(dev, stagingBuffer, stagingMemory, 0);

    void* data;
    vkMapMemory(dev, stagingMemory, 0, size, 0, &data);
    memcpy(data, indices.data(), (size_t)size);
    vkUnmapMemory(dev, stagingMemory);

    bufInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    vkCreateBuffer(dev, &bufInfo, nullptr, &m_indexBuffer);
    vkGetBufferMemoryRequirements(dev, m_indexBuffer, &memReq);
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = m_core->findMemoryType(memReq.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vkAllocateMemory(dev, &allocInfo, nullptr, &m_indexBufferMemory);
    vkBindBufferMemory(dev, m_indexBuffer, m_indexBufferMemory, 0);

    auto cmd = m_core->beginSingleTimeCommands();
    VkBufferCopy copy{};
    copy.size = size;
    vkCmdCopyBuffer(cmd, stagingBuffer, m_indexBuffer, 1, &copy);
    m_core->endSingleTimeCommands(cmd);

    vkDestroyBuffer(dev, stagingBuffer, nullptr);
    vkFreeMemory(dev, stagingMemory, nullptr);
}

void Renderer::createUniformBuffers() {
    auto dev = m_core->device();
    VkDeviceSize size = sizeof(UniformBufferObject);
    m_uniformBuffers.resize(VulkanCore::MAX_FRAMES_IN_FLIGHT);
    m_uniformBuffersMemory.resize(VulkanCore::MAX_FRAMES_IN_FLIGHT);
    m_uniformBuffersMapped.resize(VulkanCore::MAX_FRAMES_IN_FLIGHT);

    VkBufferCreateInfo bufInfo{};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size = size;
    bufInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    for (size_t i = 0; i < VulkanCore::MAX_FRAMES_IN_FLIGHT; i++) {
        vkCreateBuffer(dev, &bufInfo, nullptr, &m_uniformBuffers[i]);
        VkMemoryRequirements memReq;
        vkGetBufferMemoryRequirements(dev, m_uniformBuffers[i], &memReq);
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReq.size;
        allocInfo.memoryTypeIndex = m_core->findMemoryType(memReq.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        vkAllocateMemory(dev, &allocInfo, nullptr, &m_uniformBuffersMemory[i]);
        vkBindBufferMemory(dev, m_uniformBuffers[i], m_uniformBuffersMemory[i], 0);
        vkMapMemory(dev, m_uniformBuffersMemory[i], 0, size, 0, &m_uniformBuffersMapped[i]);
    }
}

void Renderer::createDescriptorPool() {
    auto dev = m_core->device();
    std::array<VkDescriptorPoolSize, 2> sizes{};
    sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    sizes[0].descriptorCount = VulkanCore::MAX_FRAMES_IN_FLIGHT;
    sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    sizes[1].descriptorCount = 10;

    VkDescriptorPoolCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    info.poolSizeCount = 2;
    info.pPoolSizes = sizes.data();
    info.maxSets = VulkanCore::MAX_FRAMES_IN_FLIGHT + 10;
    if (vkCreateDescriptorPool(dev, &info, nullptr, &m_descriptorPool) != VK_SUCCESS)
        throw std::runtime_error("failed to create descriptor pool");
}

void Renderer::createUboDescriptorSets() {
    auto dev = m_core->device();
    m_uboDescriptorSets.resize(VulkanCore::MAX_FRAMES_IN_FLIGHT);
    std::vector<VkDescriptorSetLayout> layouts(VulkanCore::MAX_FRAMES_IN_FLIGHT, m_uboSetLayout);

    VkDescriptorSetAllocateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    info.descriptorPool = m_descriptorPool;
    info.descriptorSetCount = VulkanCore::MAX_FRAMES_IN_FLIGHT;
    info.pSetLayouts = layouts.data();
    if (vkAllocateDescriptorSets(dev, &info, m_uboDescriptorSets.data()) != VK_SUCCESS)
        throw std::runtime_error("failed to allocate UBO descriptor sets");

    for (size_t i = 0; i < VulkanCore::MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo bufInfo{};
        bufInfo.buffer = m_uniformBuffers[i];
        bufInfo.range = sizeof(UniformBufferObject);

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = m_uboDescriptorSets[i];
        write.dstBinding = 0;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.pBufferInfo = &bufInfo;
        vkUpdateDescriptorSets(dev, 1, &write, 0, nullptr);
    }
}

void Renderer::beginFrame(VkCommandBuffer cmd) {
    m_currentExtent = m_core->swapchainExtent();
    VkViewport viewport{};
    viewport.x = 0; viewport.y = 0;
    viewport.width = (float)m_currentExtent.width;
    viewport.height = (float)m_currentExtent.height;
    viewport.minDepth = 0; viewport.maxDepth = 1;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = m_currentExtent;
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    updateUbo();
}

void Renderer::drawSprite(VkCommandBuffer cmd, const glm::vec2& pos, const glm::vec2& scale,
                          float rotation, Texture* texture, const glm::vec2& tiling) {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, &m_vertexBuffer, offsets);
    vkCmdBindIndexBuffer(cmd, m_indexBuffer, 0, VK_INDEX_TYPE_UINT16);

    std::array<VkDescriptorSet, 2> descSets = {
        m_uboDescriptorSets[m_core->currentFrame()],
        texture->descriptorSet()
    };
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout,
                            0, 2, descSets.data(), 0, nullptr);

    PushConstants pc{};
    pc.position = pos;
    pc.scale = scale;
    pc.tiling = tiling;
    pc.rotation = rotation;
    vkCmdPushConstants(cmd, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT,
                       0, sizeof(PushConstants), &pc);

    vkCmdDrawIndexed(cmd, 6, 1, 0, 0, 0);
}

void Renderer::endFrame() {
}

void Renderer::updateUbo() {
    auto& extent = m_currentExtent;
    UniformBufferObject ubo{};
    ubo.projection = glm::ortho(0.0f, (float)extent.width, (float)extent.height, 0.0f, -1.0f, 1.0f);
    memcpy(m_uniformBuffersMapped[m_core->currentFrame()], &ubo, sizeof(ubo));
}
