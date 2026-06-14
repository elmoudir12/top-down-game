#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>
#include <memory>

class VulkanCore;
class Texture;

struct UniformBufferObject {
    glm::mat4 projection;
};

struct Vertex {
    glm::vec2 position;
    glm::vec2 texCoord;
};

struct PushConstants {
    glm::vec2 position;
    glm::vec2 scale;
    glm::vec2 tiling;
    float rotation;
};

class Renderer {
public:
    Renderer(VulkanCore* core, const std::vector<char>& vertShader, const std::vector<char>& fragShader);
    ~Renderer();

    void beginFrame(VkCommandBuffer cmd);
    void drawSprite(VkCommandBuffer cmd, const glm::vec2& pos, const glm::vec2& scale,
                    float rotation, Texture* texture, const glm::vec2& tiling = {1, 1});
    void endFrame();

    VkDescriptorSetLayout textureSetLayout() const { return m_textureSetLayout; }
    VkDescriptorPool descriptorPool() const { return m_descriptorPool; }

private:
    void createDescriptorSetLayouts();
    void createPipeline(const std::vector<char>& vert, const std::vector<char>& frag);
    void createVertexBuffer();
    void createIndexBuffer();
    void createUniformBuffers();
    void createDescriptorPool();
    void createUboDescriptorSets();
    void updateUbo();

    VulkanCore* m_core;

    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;

    VkBuffer m_vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_vertexBufferMemory = VK_NULL_HANDLE;
    VkBuffer m_indexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_indexBufferMemory = VK_NULL_HANDLE;

    VkDescriptorSetLayout m_uboSetLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_textureSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;

    std::vector<VkBuffer> m_uniformBuffers;
    std::vector<VkDeviceMemory> m_uniformBuffersMemory;
    std::vector<void*> m_uniformBuffersMapped;
    std::vector<VkDescriptorSet> m_uboDescriptorSets;

    VkExtent2D m_currentExtent{};
};
