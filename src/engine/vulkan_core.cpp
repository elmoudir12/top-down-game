#include "vulkan_core.h"
#include <set>
#include <algorithm>

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT,
    VkDebugUtilsMessageTypeFlagsEXT,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void*) {
    std::cerr << "[VULKAN] " << pCallbackData->pMessage << std::endl;
    return VK_FALSE;
}

static VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pMessenger) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)
        vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    return func ? func(instance, pCreateInfo, pAllocator, pMessenger) : VK_ERROR_EXTENSION_NOT_PRESENT;
}

static void DestroyDebugUtilsMessengerEXT(VkInstance instance,
    VkDebugUtilsMessengerEXT messenger, const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)
        vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func) func(instance, messenger, pAllocator);
}

static std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) throw std::runtime_error("failed to open: " + filename);
    size_t size = (size_t)file.tellg();
    std::vector<char> buf(size);
    file.seekg(0);
    file.read(buf.data(), (long)size);
    file.close();
    return buf;
}

const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

#ifdef NDEBUG
static const bool enableValidationLayers = false;
#else
static const bool enableValidationLayers = true;
#endif

VulkanCore::VulkanCore(GLFWwindow* window) : m_window(window) {
    glfwSetWindowUserPointer(window, this);
    createInstance();
    setupDebugMessenger();
    createSurface(window);
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapchain();
    createImageViews();
    createRenderPass();
    createCommandPool();
    createFramebuffers();
    createSyncObjects();
}

VulkanCore::~VulkanCore() {
    waitIdle();
    cleanupSwapchain();
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(m_device, m_renderFinishedSemaphores[i], nullptr);
        vkDestroySemaphore(m_device, m_imageAvailableSemaphores[i], nullptr);
        vkDestroyFence(m_device, m_inFlightFences[i], nullptr);
    }
    vkDestroyCommandPool(m_device, m_commandPool, nullptr);
    vkDestroyRenderPass(m_device, m_renderPass, nullptr);
    vkDestroyDevice(m_device, nullptr);
    if (enableValidationLayers)
        DestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
    vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
    vkDestroyInstance(m_instance, nullptr);
}

void VulkanCore::createInstance() {
    if (enableValidationLayers) {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        std::vector<VkLayerProperties> layers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, layers.data());
        bool found = false;
        for (const auto& l : layers)
            if (strcmp(l.layerName, "VK_LAYER_KHRONOS_validation") == 0) { found = true; break; }
        if (!found)
            throw std::runtime_error("validation layers not available");
    }

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = APP_NAME;
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = APP_NAME;
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    uint32_t glfwExtCount = 0;
    const char** glfwExts = glfwGetRequiredInstanceExtensions(&glfwExtCount);
    std::vector<const char*> extensions(glfwExts, glfwExts + glfwExtCount);
    if (enableValidationLayers)
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = (uint32_t)extensions.size();
    createInfo.ppEnabledExtensionNames = extensions.data();
    createInfo.enabledLayerCount = enableValidationLayers ? (uint32_t)validationLayers.size() : 0;
    createInfo.ppEnabledLayerNames = enableValidationLayers ? validationLayers.data() : nullptr;

    if (vkCreateInstance(&createInfo, nullptr, &m_instance) != VK_SUCCESS)
        throw std::runtime_error("failed to create instance");
}

void VulkanCore::setupDebugMessenger() {
    if (!enableValidationLayers) return;
    VkDebugUtilsMessengerCreateInfoEXT info{};
    info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    info.pfnUserCallback = debugCallback;
    if (CreateDebugUtilsMessengerEXT(m_instance, &info, nullptr, &m_debugMessenger) != VK_SUCCESS)
        throw std::runtime_error("failed to set up debug messenger");
}

void VulkanCore::createSurface(GLFWwindow* window) {
    if (glfwCreateWindowSurface(m_instance, window, nullptr, &m_surface) != VK_SUCCESS)
        throw std::runtime_error("failed to create window surface");
}

void VulkanCore::pickPhysicalDevice() {
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(m_instance, &count, nullptr);
    if (count == 0) throw std::runtime_error("no Vulkan-capable GPUs");
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(m_instance, &count, devices.data());
    for (const auto& d : devices) {
        if (isDeviceSuitable(d)) { m_physicalDevice = d; break; }
    }
    if (m_physicalDevice == VK_NULL_HANDLE)
        throw std::runtime_error("no suitable GPU");
}

void VulkanCore::createLogicalDevice() {
    auto indices = findQueueFamilies(m_physicalDevice);
    std::vector<VkDeviceQueueCreateInfo> queueInfos;
    std::set<uint32_t> uniqueQueues = {indices.graphicsFamily.value(), indices.presentFamily.value()};
    float priority = 1.0f;
    for (uint32_t q : uniqueQueues) {
        VkDeviceQueueCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        info.queueFamilyIndex = q;
        info.queueCount = 1;
        info.pQueuePriorities = &priority;
        queueInfos.push_back(info);
    }

    VkPhysicalDeviceFeatures features{};
    features.samplerAnisotropy = VK_TRUE;

    VkDeviceCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    info.queueCreateInfoCount = (uint32_t)queueInfos.size();
    info.pQueueCreateInfos = queueInfos.data();
    info.pEnabledFeatures = &features;
    info.enabledExtensionCount = (uint32_t)deviceExtensions.size();
    info.ppEnabledExtensionNames = deviceExtensions.data();
    if (enableValidationLayers) {
        info.enabledLayerCount = (uint32_t)validationLayers.size();
        info.ppEnabledLayerNames = validationLayers.data();
    }

    if (vkCreateDevice(m_physicalDevice, &info, nullptr, &m_device) != VK_SUCCESS)
        throw std::runtime_error("failed to create logical device");

    vkGetDeviceQueue(m_device, indices.graphicsFamily.value(), 0, &m_graphicsQueue);
    vkGetDeviceQueue(m_device, indices.presentFamily.value(), 0, &m_presentQueue);
    m_graphicsQueueFamily = indices.graphicsFamily;
    m_presentQueueFamily = indices.presentFamily;
}

void VulkanCore::createSwapchain() {
    auto support = querySwapchainSupport(m_physicalDevice);
    auto format = chooseSwapSurfaceFormat(support.formats);
    auto mode = chooseSwapPresentMode(support.presentModes);
    auto extent = chooseSwapExtent(support.capabilities);

    uint32_t imageCount = support.capabilities.minImageCount + 1;
    if (support.capabilities.maxImageCount > 0 && imageCount > support.capabilities.maxImageCount)
        imageCount = support.capabilities.maxImageCount;

    VkSwapchainCreateInfoKHR info{};
    info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    info.surface = m_surface;
    info.minImageCount = imageCount;
    info.imageFormat = format.format;
    info.imageColorSpace = format.colorSpace;
    info.imageExtent = extent;
    info.imageArrayLayers = 1;
    info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    auto indices = findQueueFamilies(m_physicalDevice);
    uint32_t qfi[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};
    if (indices.graphicsFamily != indices.presentFamily) {
        info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        info.queueFamilyIndexCount = 2;
        info.pQueueFamilyIndices = qfi;
    } else {
        info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    info.preTransform = support.capabilities.currentTransform;
    info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    info.presentMode = mode;
    info.clipped = VK_TRUE;

    if (vkCreateSwapchainKHR(m_device, &info, nullptr, &m_swapchain) != VK_SUCCESS)
        throw std::runtime_error("failed to create swapchain");

    vkGetSwapchainImagesKHR(m_device, m_swapchain, &imageCount, nullptr);
    m_swapchainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(m_device, m_swapchain, &imageCount, m_swapchainImages.data());
    m_swapchainFormat = format.format;
    m_swapchainExtent = extent;
}

void VulkanCore::createImageViews() {
    m_swapchainImageViews.resize(m_swapchainImages.size());
    for (size_t i = 0; i < m_swapchainImages.size(); i++)
        m_swapchainImageViews[i] = createImageView(m_swapchainImages[i], m_swapchainFormat, VK_IMAGE_ASPECT_COLOR_BIT);
}

void VulkanCore::createRenderPass() {
    VkAttachmentDescription color{};
    color.format = m_swapchainFormat;
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;

    VkSubpassDependency dep{};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.srcAccessMask = 0;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info.attachmentCount = 1;
    info.pAttachments = &color;
    info.subpassCount = 1;
    info.pSubpasses = &subpass;
    info.dependencyCount = 1;
    info.pDependencies = &dep;

    if (vkCreateRenderPass(m_device, &info, nullptr, &m_renderPass) != VK_SUCCESS)
        throw std::runtime_error("failed to create render pass");
}

void VulkanCore::createCommandPool() {
    VkCommandPoolCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    info.queueFamilyIndex = m_graphicsQueueFamily.value();
    if (vkCreateCommandPool(m_device, &info, nullptr, &m_commandPool) != VK_SUCCESS)
        throw std::runtime_error("failed to create command pool");

    m_commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t)m_commandBuffers.size();
    if (vkAllocateCommandBuffers(m_device, &allocInfo, m_commandBuffers.data()) != VK_SUCCESS)
        throw std::runtime_error("failed to allocate command buffers");
}

void VulkanCore::createFramebuffers() {
    m_swapchainFramebuffers.resize(m_swapchainImageViews.size());
    for (size_t i = 0; i < m_swapchainImageViews.size(); i++) {
        VkImageView attachments[] = {m_swapchainImageViews[i]};
        VkFramebufferCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        info.renderPass = m_renderPass;
        info.attachmentCount = 1;
        info.pAttachments = attachments;
        info.width = m_swapchainExtent.width;
        info.height = m_swapchainExtent.height;
        info.layers = 1;
        if (vkCreateFramebuffer(m_device, &info, nullptr, &m_swapchainFramebuffers[i]) != VK_SUCCESS)
            throw std::runtime_error("failed to create framebuffer");
    }
}

void VulkanCore::createSyncObjects() {
    m_imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(m_device, &semInfo, nullptr, &m_imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(m_device, &semInfo, nullptr, &m_renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(m_device, &fenceInfo, nullptr, &m_inFlightFences[i]) != VK_SUCCESS)
            throw std::runtime_error("failed to create sync objects");
    }
}

VkCommandBuffer VulkanCore::beginFrame() {
    vkWaitForFences(m_device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);

    VkResult result = vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX,
        m_imageAvailableSemaphores[m_currentFrame], VK_NULL_HANDLE, &m_currentImageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain();
        return VK_NULL_HANDLE;
    }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        throw std::runtime_error("failed to acquire swapchain image");

    vkResetFences(m_device, 1, &m_inFlightFences[m_currentFrame]);

    vkResetCommandBuffer(m_commandBuffers[m_currentFrame], 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(m_commandBuffers[m_currentFrame], &beginInfo);

    VkClearValue clearColor = {{{0.1f, 0.12f, 0.15f, 1.0f}}};
    VkRenderPassBeginInfo rpInfo{};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpInfo.renderPass = m_renderPass;
    rpInfo.framebuffer = m_swapchainFramebuffers[m_currentImageIndex];
    rpInfo.renderArea.offset = {0, 0};
    rpInfo.renderArea.extent = m_swapchainExtent;
    rpInfo.clearValueCount = 1;
    rpInfo.pClearValues = &clearColor;
    vkCmdBeginRenderPass(m_commandBuffers[m_currentFrame], &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

    return m_commandBuffers[m_currentFrame];
}

void VulkanCore::endFrame() {
    vkCmdEndRenderPass(m_commandBuffers[m_currentFrame]);

    if (vkEndCommandBuffer(m_commandBuffers[m_currentFrame]) != VK_SUCCESS)
        throw std::runtime_error("failed to record command buffer");

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    VkSemaphore waitSems[] = {m_imageAvailableSemaphores[m_currentFrame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSems;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_commandBuffers[m_currentFrame];
    VkSemaphore signalSems[] = {m_renderFinishedSemaphores[m_currentFrame]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSems;
    vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_inFlightFences[m_currentFrame]);

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSems;
    VkSwapchainKHR swapchains[] = {m_swapchain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &m_currentImageIndex;
    VkResult result = vkQueuePresentKHR(m_presentQueue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_framebufferResized) {
        m_framebufferResized = false;
        recreateSwapchain();
    }

    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void VulkanCore::recreateSwapchain() {
    int w, h;
    glfwGetWindowSize(m_window, &w, &h);
    while (w == 0 || h == 0) {
        glfwGetWindowSize(m_window, &w, &h);
        glfwWaitEvents();
    }
    waitIdle();
    cleanupSwapchain();
    createSwapchain();
    createImageViews();
    createFramebuffers();
}

void VulkanCore::cleanupSwapchain() {
    for (auto fb : m_swapchainFramebuffers)
        vkDestroyFramebuffer(m_device, fb, nullptr);
    for (auto iv : m_swapchainImageViews)
        vkDestroyImageView(m_device, iv, nullptr);
    vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
}

VkShaderModule VulkanCore::createShaderModule(const std::vector<char>& code) const {
    VkShaderModuleCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = code.size();
    info.pCode = reinterpret_cast<const uint32_t*>(code.data());
    VkShaderModule module;
    if (vkCreateShaderModule(m_device, &info, nullptr, &module) != VK_SUCCESS)
        throw std::runtime_error("failed to create shader module");
    return module;
}

uint32_t VulkanCore::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags props) const {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++)
        if ((typeFilter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & props) == props)
            return i;
    throw std::runtime_error("failed to find suitable memory type");
}

VkCommandBuffer VulkanCore::beginSingleTimeCommands() const {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = m_commandPool;
    allocInfo.commandBufferCount = 1;
    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(m_device, &allocInfo, &cmd);
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);
    return cmd;
}

void VulkanCore::endSingleTimeCommands(VkCommandBuffer cmd) const {
    vkEndCommandBuffer(cmd);
    VkSubmitInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    info.commandBufferCount = 1;
    info.pCommandBuffers = &cmd;
    vkQueueSubmit(m_graphicsQueue, 1, &info, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_graphicsQueue);
    vkFreeCommandBuffers(m_device, m_commandPool, 1, &cmd);
}

void VulkanCore::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) const {
    auto cmd = beginSingleTimeCommands();
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};
    vkCmdCopyBufferToImage(cmd, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    endSingleTimeCommands(cmd);
}

void VulkanCore::transitionImageLayout(VkImage image, VkFormat, VkImageLayout oldLayout, VkImageLayout newLayout) const {
    auto cmd = beginSingleTimeCommands();
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags srcStage, dstStage;
    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        throw std::runtime_error("unsupported layout transition");
    }

    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    endSingleTimeCommands(cmd);
}

void VulkanCore::createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
                             VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
                             VkImage& image, VkDeviceMemory& imageMemory) const {
    VkImageCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.imageType = VK_IMAGE_TYPE_2D;
    info.extent.width = width;
    info.extent.height = height;
    info.extent.depth = 1;
    info.mipLevels = 1;
    info.arrayLayers = 1;
    info.format = format;
    info.tiling = tiling;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    info.usage = usage;
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateImage(m_device, &info, nullptr, &image) != VK_SUCCESS)
        throw std::runtime_error("failed to create image");

    VkMemoryRequirements memReq;
    vkGetImageMemoryRequirements(m_device, image, &memReq);
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReq.memoryTypeBits, properties);
    if (vkAllocateMemory(m_device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS)
        throw std::runtime_error("failed to allocate image memory");
    vkBindImageMemory(m_device, image, imageMemory, 0);
}

VkImageView VulkanCore::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) const {
    VkImageViewCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    info.image = image;
    info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    info.format = format;
    info.subresourceRange.aspectMask = aspectFlags;
    info.subresourceRange.baseMipLevel = 0;
    info.subresourceRange.levelCount = 1;
    info.subresourceRange.baseArrayLayer = 0;
    info.subresourceRange.layerCount = 1;
    VkImageView view;
    if (vkCreateImageView(m_device, &info, nullptr, &view) != VK_SUCCESS)
        throw std::runtime_error("failed to create image view");
    return view;
}

QueueFamilyIndices VulkanCore::findQueueFamilies(VkPhysicalDevice device) const {
    QueueFamilyIndices indices;
    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());
    for (uint32_t i = 0; i < count; i++) {
        if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            indices.graphicsFamily = i;
        VkBool32 present = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &present);
        if (present) indices.presentFamily = i;
        if (indices.isComplete()) break;
    }
    return indices;
}

SwapchainSupportDetails VulkanCore::querySwapchainSupport(VkPhysicalDevice device) const {
    SwapchainSupportDetails details;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface, &details.capabilities);
    uint32_t count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &count, nullptr);
    if (count) {
        details.formats.resize(count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &count, details.formats.data());
    }
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &count, nullptr);
    if (count) {
        details.presentModes.resize(count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &count, details.presentModes.data());
    }
    return details;
}

VkSurfaceFormatKHR VulkanCore::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) const {
    for (const auto& f : formats)
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return f;
    return formats[0];
}

VkPresentModeKHR VulkanCore::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& modes) const {
    for (const auto& m : modes)
        if (m == VK_PRESENT_MODE_MAILBOX_KHR) return m;
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanCore::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const {
    if (capabilities.currentExtent.width != UINT32_MAX) return capabilities.currentExtent;
    int w, h;
    glfwGetFramebufferSize(m_window, &w, &h);
    return {std::clamp((uint32_t)w, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
            std::clamp((uint32_t)h, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)};
}

bool VulkanCore::isDeviceSuitable(VkPhysicalDevice device) const {
    auto indices = findQueueFamilies(device);
    bool extOk = checkDeviceExtensionSupport(device);
    bool swapchainOk = false;
    if (extOk) {
        auto support = querySwapchainSupport(device);
        swapchainOk = !support.formats.empty() && !support.presentModes.empty();
    }
    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceFeatures(device, &features);
    return indices.isComplete() && extOk && swapchainOk && features.samplerAnisotropy;
}

bool VulkanCore::checkDeviceExtensionSupport(VkPhysicalDevice device) const {
    uint32_t count;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> exts(count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, exts.data());
    for (const auto& req : deviceExtensions) {
        bool found = false;
        for (const auto& ext : exts)
            if (strcmp(req, ext.extensionName) == 0) { found = true; break; }
        if (!found) return false;
    }
    return true;
}
