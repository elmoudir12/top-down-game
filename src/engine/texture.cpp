#include "texture.h"
#include "vulkan_core.h"
#include <cstring>
#include <iostream>
#include <fstream>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

Texture::Texture(VulkanCore* core, const uint32_t* pixels, uint32_t width, uint32_t height,
                 VkDescriptorSetLayout textureSetLayout, VkDescriptorPool pool)
    : m_core(core), m_width(width), m_height(height) {
    createTextureImage(pixels);
    createTextureImageView();
    createTextureSampler();
    createDescriptorSet(textureSetLayout, pool);
}

Texture::Texture(VulkanCore* core, const std::string& filename,
                 VkDescriptorSetLayout textureSetLayout, VkDescriptorPool pool,
                 VkSamplerAddressMode addressMode)
    : m_core(core), m_addressMode(addressMode) {
    int w, h, channels;
    unsigned char* data = stbi_load(filename.c_str(), &w, &h, &channels, STBI_rgb_alpha);
    if (!data) {
        std::string err = stbi_failure_reason();
        throw std::runtime_error("failed to load texture: " + filename + " (" + err + ")");
    }
    m_width = (uint32_t)w;
    m_height = (uint32_t)h;

    std::cout << "Loaded " << filename << ": " << w << "x" << h
              << " ch=" << channels << std::endl;

    std::vector<uint32_t> pixels(w * h);
    for (int i = 0; i < w * h; i++)
        pixels[i] = ((uint32_t)data[i*4+3] << 24) | ((uint32_t)data[i*4+2] << 16) |
                     ((uint32_t)data[i*4+1] << 8) | (uint32_t)data[i*4+0];

    // Debug: print first row of RGBA values
    std::cout << "  First 8 pixels:";
    for (int x = 0; x < std::min(w, 8); x++)
        std::cout << " " << std::hex << pixels[x];
    std::cout << std::dec << std::endl;

    stbi_image_free(data);

    createTextureImage(pixels.data());
    createTextureImageView();
    createTextureSampler();
    createDescriptorSet(textureSetLayout, pool);
}

Texture::~Texture() {
    auto dev = m_core->device();
    vkDestroySampler(dev, m_sampler, nullptr);
    vkDestroyImageView(dev, m_imageView, nullptr);
    vkDestroyImage(dev, m_image, nullptr);
    vkFreeMemory(dev, m_imageMemory, nullptr);
}

void Texture::createTextureImage(const uint32_t* pixels) {
    auto dev = m_core->device();
    VkDeviceSize size = (VkDeviceSize)m_width * m_height * 4;

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
    memcpy(data, pixels, (size_t)size);
    vkUnmapMemory(dev, stagingMemory);

    m_core->createImage(m_width, m_height, VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        m_image, m_imageMemory);

    m_core->transitionImageLayout(m_image, VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    m_core->copyBufferToImage(stagingBuffer, m_image, m_width, m_height);
    m_core->transitionImageLayout(m_image, VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(dev, stagingBuffer, nullptr);
    vkFreeMemory(dev, stagingMemory, nullptr);
}

void Texture::createTextureImageView() {
    m_imageView = m_core->createImageView(m_image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
}

void Texture::createTextureSampler() {
    auto dev = m_core->device();
    VkSamplerCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    info.magFilter = VK_FILTER_NEAREST;
    info.minFilter = VK_FILTER_NEAREST;
    info.addressModeU = m_addressMode;
    info.addressModeV = m_addressMode;
    info.addressModeW = m_addressMode;
    info.anisotropyEnable = VK_FALSE;
    info.compareOp = VK_COMPARE_OP_ALWAYS;
    info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    if (vkCreateSampler(dev, &info, nullptr, &m_sampler) != VK_SUCCESS)
        throw std::runtime_error("failed to create sampler");
}

void Texture::createDescriptorSet(VkDescriptorSetLayout layout, VkDescriptorPool pool) {
    auto dev = m_core->device();
    VkDescriptorSetAllocateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    info.descriptorPool = pool;
    info.descriptorSetCount = 1;
    info.pSetLayouts = &layout;
    if (vkAllocateDescriptorSets(dev, &info, &m_descriptorSet) != VK_SUCCESS)
        throw std::runtime_error("failed to allocate texture descriptor set");

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = m_imageView;
    imageInfo.sampler = m_sampler;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_descriptorSet;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &imageInfo;
    vkUpdateDescriptorSets(dev, 1, &write, 0, nullptr);
}
