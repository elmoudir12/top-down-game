#pragma once

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <optional>
#include <stdexcept>
#include <cstring>
#include <iostream>
#include <fstream>

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;
    bool isComplete() const { return graphicsFamily.has_value() && presentFamily.has_value(); }
};

struct SwapchainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

class VulkanCore {
public:
    VulkanCore(GLFWwindow* window);
    ~VulkanCore();

    VkDevice device() const { return m_device; }
    VkPhysicalDevice physicalDevice() const { return m_physicalDevice; }
    VkQueue graphicsQueue() const { return m_graphicsQueue; }
    VkQueue presentQueue() const { return m_presentQueue; }
    uint32_t graphicsQueueFamily() const { return m_graphicsQueueFamily.value(); }
    VkRenderPass renderPass() const { return m_renderPass; }
    VkExtent2D swapchainExtent() const { return m_swapchainExtent; }
    VkCommandPool commandPool() const { return m_commandPool; }
    uint32_t currentImageIndex() const { return m_currentImageIndex; }
    uint32_t currentFrame() const { return (uint32_t)m_currentFrame; }

    VkCommandBuffer beginFrame();
    void endFrame();
    void waitIdle() const { vkDeviceWaitIdle(m_device); }

    VkShaderModule createShaderModule(const std::vector<char>& code) const;
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;
    VkCommandBuffer beginSingleTimeCommands() const;
    void endSingleTimeCommands(VkCommandBuffer cmd) const;
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) const;
    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout,
                               VkImageLayout newLayout) const;
    void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
                     VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
                     VkImage& image, VkDeviceMemory& imageMemory) const;
    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) const;

    static void framebufferResizedCallback(GLFWwindow* window, int, int) {
        auto* core = static_cast<VulkanCore*>(glfwGetWindowUserPointer(window));
        if (core) core->m_framebufferResized = true;
    }

    void recreateSwapchain();

    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
    static constexpr const char* APP_NAME = "TopDown";

private:
    void createInstance();
    void setupDebugMessenger();
    void createSurface(GLFWwindow* window);
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createSwapchain();
    void createImageViews();
    void createRenderPass();
    void createCommandPool();
    void createFramebuffers();
    void createSyncObjects();
    void cleanupSwapchain();

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) const;
    SwapchainSupportDetails querySwapchainSupport(VkPhysicalDevice device) const;
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) const;
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& modes) const;
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const;
    bool isDeviceSuitable(VkPhysicalDevice device) const;
    bool checkDeviceExtensionSupport(VkPhysicalDevice device) const;

    GLFWwindow* m_window = nullptr;
    VkInstance m_instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    VkQueue m_presentQueue = VK_NULL_HANDLE;
    std::optional<uint32_t> m_graphicsQueueFamily;
    std::optional<uint32_t> m_presentQueueFamily;

    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> m_swapchainImages;
    std::vector<VkImageView> m_swapchainImageViews;
    std::vector<VkFramebuffer> m_swapchainFramebuffers;
    VkFormat m_swapchainFormat = VK_FORMAT_B8G8R8A8_SRGB;
    VkExtent2D m_swapchainExtent{};

    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> m_commandBuffers;

    std::vector<VkSemaphore> m_imageAvailableSemaphores;
    std::vector<VkSemaphore> m_renderFinishedSemaphores;
    std::vector<VkFence> m_inFlightFences;
    size_t m_currentFrame = 0;
    uint32_t m_currentImageIndex = 0;
    bool m_framebufferResized = false;
};
