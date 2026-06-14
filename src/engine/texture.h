#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>
#include <vector>
#include <string>

class VulkanCore;

class Texture {
public:
    Texture(VulkanCore* core, const uint32_t* pixels, uint32_t width, uint32_t height,
            VkDescriptorSetLayout textureSetLayout, VkDescriptorPool pool);
    Texture(VulkanCore* core, const std::string& filename,
            VkDescriptorSetLayout textureSetLayout, VkDescriptorPool pool,
            VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
    ~Texture();

    uint32_t width() const { return m_width; }
    uint32_t height() const { return m_height; }
    VkImageView imageView() const { return m_imageView; }
    VkSampler sampler() const { return m_sampler; }
    VkDescriptorSet descriptorSet() const { return m_descriptorSet; }

private:
    void createTextureImage(const uint32_t* pixels);
    void createTextureImageView();
    void createTextureSampler();
    void createDescriptorSet(VkDescriptorSetLayout layout, VkDescriptorPool pool);

    VulkanCore* m_core;

    VkImage m_image = VK_NULL_HANDLE;
    VkDeviceMemory m_imageMemory = VK_NULL_HANDLE;
    VkImageView m_imageView = VK_NULL_HANDLE;
    VkSampler m_sampler = VK_NULL_HANDLE;
    VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;

    uint32_t m_width;
    uint32_t m_height;
    VkSamplerAddressMode m_addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
};
