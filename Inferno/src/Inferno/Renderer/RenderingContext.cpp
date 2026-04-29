#include "RenderingContext.h"
#include "pch.h"

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"
#include "Inferno/Log.h"
#include "Inferno/Renderer/VulkanUtils.h"
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <vector>
#include <vulkan/vulkan_core.h>

const std::vector<const char *> validationLayers = {
    "VK_LAYER_KHRONOS_validation"};

const std::vector<const char *> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME};

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

bool checkValidationlayerSupport() {
  uint32_t layerCount = 0;
  vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

  std::vector<VkLayerProperties> availableLayers(layerCount);
  vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

  for (const auto &layerName : validationLayers) {
    bool layerFound = false;

    for (const auto &layerProperties : availableLayers) {
      if (strcmp(layerName, layerProperties.layerName) == 0) {
        layerFound = true;
        break;
      }
    }

    if (!layerFound) {
      return false;
    }
  }

  return true;
}

namespace Inferno {
std::shared_ptr<RenderingContext> RenderingContext::s_Context = nullptr;

std::shared_ptr<RenderingContext> &
RenderingContext::CreateContext(Window *window) {
  if (!s_Context)
    s_Context = std::make_shared<RenderingContext>(window);
  return s_Context;
}

std::shared_ptr<RenderingContext> &RenderingContext::GetContext() {
  if (!s_Context)
    throw std::runtime_error("Rendering Context is NullPtr");
  return s_Context;
}

RenderingContext::RenderingContext(Window *window) : m_Window(window) {}

RenderingContext::~RenderingContext() {}

void RenderingContext::Init() {
  CreateInstance();
  CreateSurface();
  PickPhysicalDevice();
  CreateLogicalDevice();
  CreateSwapChain();
  CreateImageViews();

  INFERNO_LOG_INFO("Vulkan Initialized");
}

void RenderingContext::ShutDown() {
  if (m_Device != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(m_Device);
  }

  CleanUpSwapChain();

  if (m_Device)
    vkDestroyDevice(m_Device, nullptr);

  if (m_Surface)
    vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);

  if (m_Instance)
    vkDestroyInstance(m_Instance, nullptr);

  m_Device = VK_NULL_HANDLE;
  m_Surface = VK_NULL_HANDLE;
  m_Instance = VK_NULL_HANDLE;
}

void RenderingContext::CreateInstance() {
  VkApplicationInfo appInfo = {
      .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .pApplicationName = "Inferno Engine",
      .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
      .pEngineName = "No Engine",
      .engineVersion = VK_MAKE_VERSION(1, 0, 0),
      .apiVersion = VK_API_VERSION_1_4,
  };

  uint32_t glfwExtCount = 0;
  const char **glfwExtensions =
      glfwGetRequiredInstanceExtensions(&glfwExtCount);

  if (enableValidationLayers && !checkValidationlayerSupport()) {
    throw std::runtime_error("Validation layers requested, but not available");
  }

  VkInstanceCreateInfo createInfo = {
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .pApplicationInfo = &appInfo,
      .enabledLayerCount = 0,
      .ppEnabledLayerNames = nullptr,
      .enabledExtensionCount = glfwExtCount,
      .ppEnabledExtensionNames = glfwExtensions,
  };

  if (enableValidationLayers) {
    createInfo.enabledLayerCount =
        static_cast<uint32_t>(validationLayers.size());
    createInfo.ppEnabledLayerNames = validationLayers.data();
  }

  if (vkCreateInstance(&createInfo, nullptr, &m_Instance) != VK_SUCCESS) {
    INFERNO_LOG_ERROR("Failed to create VkInstance");
    throw std::runtime_error("Failed to create VkInstance");
  }
}

void RenderingContext::CreateSurface() {
  if (glfwCreateWindowSurface(m_Instance, m_Window->GetNativeWindow(), nullptr,
                              &m_Surface) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create Vulkan Surface");
  }
}

void RenderingContext::PickPhysicalDevice() {
  uint32_t deviceCount = 0;
  vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr);
  if (deviceCount == 0) {
    throw std::runtime_error("No Vulkan GPUs found");
  }

  std::vector<VkPhysicalDevice> devices(deviceCount);

  vkEnumeratePhysicalDevices(m_Instance, &deviceCount, devices.data());

  for (const auto &device : devices) {
    if (IsDeviceSuitable(device)) {
      m_PhysicalDevice = device;
      break;
    }
  }

  if (m_PhysicalDevice == VK_NULL_HANDLE) {
    throw std::runtime_error("Failed to find suitable GPU");
  }
}

void RenderingContext::CreateLogicalDevice() {
  VulkanUtils::QueueFamilyIndices indices =
      VulkanUtils::FindQueueFamilies(m_PhysicalDevice, m_Surface);

  std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
  std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(),
                                            indices.presentFamily.value()};

  float queuePriority = 1.0f;
  for (uint32_t queueFamily : uniqueQueueFamilies) {
    VkDeviceQueueCreateInfo queueCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = queueFamily,
        .queueCount = 1,
        .pQueuePriorities = &queuePriority,
    };
    queueCreateInfos.push_back(queueCreateInfo);
  }

  VkPhysicalDeviceFeatures deviceFeatures{};
  deviceFeatures.samplerAnisotropy = VK_TRUE;

  VkDeviceCreateInfo createInfo = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
      .pQueueCreateInfos = queueCreateInfos.data(),
      .enabledLayerCount = 0,
      .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
      .ppEnabledExtensionNames = deviceExtensions.data(),
      .pEnabledFeatures = &deviceFeatures,
  };

  if (enableValidationLayers) {
    createInfo.enabledLayerCount =
        static_cast<uint32_t>(validationLayers.size());
    createInfo.ppEnabledLayerNames = validationLayers.data();
  }

  if (vkCreateDevice(m_PhysicalDevice, &createInfo, nullptr, &m_Device) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed to create logical deivce");
  }

  vkGetDeviceQueue(m_Device, indices.graphicsFamily.value(), 0,
                   &m_GraphicsQueue);
  vkGetDeviceQueue(m_Device, indices.presentFamily.value(), 0, &m_PresentQueue);
}

void RenderingContext::CreateSwapChain() {
  VulkanUtils::SwapChainSupportDetails swapChainDetails =
      VulkanUtils::QuerySwapChainSupport(m_PhysicalDevice, m_Surface);

  VkSurfaceFormatKHR surfaceFormat =
      VulkanUtils::ChooseSwapSurfaceFormat(swapChainDetails.formats);
  VkPresentModeKHR presentMode =
      VulkanUtils::ChooseSwapPresentMode(swapChainDetails.presentModes);
  VkExtent2D extent =
      VulkanUtils::ChooseSwapExtent(swapChainDetails.capabilities, m_Window);

  uint32_t imageCount = swapChainDetails.capabilities.minImageCount + 1;

  if (swapChainDetails.capabilities.maxImageCount > 0 &&
      imageCount > swapChainDetails.capabilities.maxImageCount) {
    imageCount = swapChainDetails.capabilities.maxImageCount;
  }

  VkSwapchainCreateInfoKHR createInfo{
      .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      .surface = m_Surface,
      .minImageCount = imageCount,
      .imageFormat = surfaceFormat.format,
      .imageColorSpace = surfaceFormat.colorSpace,
      .imageExtent = extent,
      .imageArrayLayers = 1,
      .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
  };

  VulkanUtils::QueueFamilyIndices indices =
      VulkanUtils::FindQueueFamilies(m_PhysicalDevice, m_Surface);
  uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(),
                                   indices.presentFamily.value()};

  if (indices.graphicsFamily != indices.presentFamily) {
    createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    createInfo.queueFamilyIndexCount = 2;
    createInfo.pQueueFamilyIndices = queueFamilyIndices;
  } else {
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.queueFamilyIndexCount = 0;
    createInfo.pQueueFamilyIndices = nullptr;
  }

  createInfo.preTransform = swapChainDetails.capabilities.currentTransform;
  createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  createInfo.presentMode = presentMode;
  createInfo.clipped = VK_TRUE;
  createInfo.oldSwapchain = VK_NULL_HANDLE;

  INFERNO_LOG_ERROR("Width {}, Height {}", createInfo.imageExtent.width,
                    createInfo.imageExtent.height);

  if (vkCreateSwapchainKHR(m_Device, &createInfo, nullptr, &m_SwapChain) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed to Create SwapChain");
  }

  vkGetSwapchainImagesKHR(m_Device, m_SwapChain, &imageCount, nullptr);
  m_SwapChainImages.resize(imageCount);
  vkGetSwapchainImagesKHR(m_Device, m_SwapChain, &imageCount,
                          m_SwapChainImages.data());

  m_SwapChainImageFormat = surfaceFormat.format;
  m_SwapChainExtent = extent;
}

void RenderingContext::CleanUpSwapChain() {
  for (auto imageView : m_SwapChainImageViews) {
    vkDestroyImageView(m_Device, imageView, nullptr);
  }

  vkDestroySwapchainKHR(m_Device, m_SwapChain, nullptr);
}

void RenderingContext::CreateImageViews() {
  m_SwapChainImageViews.resize(m_SwapChainImages.size());

  for (size_t i = 0; i < m_SwapChainImages.size(); ++i) {
    // m_SwapChainImageViews[i] = VulkanUtils::CreateImageView(this, Image
    // &image)
    VkImageViewCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = m_SwapChainImages[i],
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = m_SwapChainImageFormat,
    };
    createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    createInfo.subresourceRange.baseMipLevel = 0;
    createInfo.subresourceRange.levelCount = 1;
    createInfo.subresourceRange.baseArrayLayer = 0;
    createInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(m_Device, &createInfo, nullptr,
                          &m_SwapChainImageViews[i]) != VK_SUCCESS) {
      throw std::runtime_error("Failed to create image views");
    }
  }
}

void RenderingContext::RecreateSwapChain() {
  CleanUpSwapChain();
  CreateSwapChain();
  CreateImageViews();
}

bool RenderingContext::IsDeviceSuitable(VkPhysicalDevice device) {
  VulkanUtils::QueueFamilyIndices indices =
      VulkanUtils::FindQueueFamilies(device, m_Surface);
  bool extensionsSupported = CheckDeviceExtensionSupport(device);

  bool swapChainAdequate = false;
  if (extensionsSupported) {
    VulkanUtils::SwapChainSupportDetails swapChainSupport =
        VulkanUtils::QuerySwapChainSupport(device, m_Surface);
    swapChainAdequate = !swapChainSupport.formats.empty() &&
                        !swapChainSupport.presentModes.empty();
  }

  VkPhysicalDeviceFeatures supportedFeatures;
  vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

  return indices.IsComplete() && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy;
}

bool RenderingContext::CheckDeviceExtensionSupport(VkPhysicalDevice device) {
  uint32_t extensionCount;
  vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount,
                                       nullptr);

  std::vector<VkExtensionProperties> availableExtensions(extensionCount);
  vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount,
                                       availableExtensions.data());

  std::set<std::string> requiredExtensions(deviceExtensions.begin(),
                                           deviceExtensions.end());

  for (const auto &extension : availableExtensions) {
    requiredExtensions.erase(extension.extensionName);
  }

  return requiredExtensions.empty();
}

void RenderingContext::CreateCommandPool() {
  VulkanUtils::QueueFamilyIndices queueFamilyIndices =
      VulkanUtils::FindQueueFamilies(GetPhysicalDevice(), m_Surface);

  VkCommandPoolCreateInfo poolCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = queueFamilyIndices.graphicsFamily.value(),
  };

  if (vkCreateCommandPool(GetDevice(), &poolCreateInfo, nullptr,
                          &m_CommandPool) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create command pool!");
  }
}

} // namespace Inferno
