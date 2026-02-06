#include "RenderingContext.h"
#include "Inferno/Renderer/Shader.h"
#include "pch.h"

#include "GLFW/glfw3.h"
#include "Inferno/Log.h"
#include <algorithm>
#include <cstdint>
#include <limits>
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
RenderingContext::CreateContext(GLFWwindow *window) {
  if (!s_Context)
    s_Context = std::make_shared<RenderingContext>(window);
  return s_Context;
}

std::shared_ptr<RenderingContext>& RenderingContext::GetContext() {
  if (!s_Context)
    throw std::runtime_error("Rendering Context is NullPtr");
  return s_Context;
}

RenderingContext::RenderingContext(GLFWwindow *window) : m_Window(window) {}

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

  for (auto imageView : m_SwapChainImageViews) {
    vkDestroyImageView(m_Device, imageView, nullptr);
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
  if (glfwCreateWindowSurface(m_Instance, m_Window, nullptr, &m_Surface) !=
      VK_SUCCESS) {
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
  QueueFamilyIndices indices = FindQueueFamilies(m_PhysicalDevice);

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
                   &m_GrapicsQueue);
  vkGetDeviceQueue(m_Device, indices.presentFamily.value(), 0, &m_PresentQueue);
}

void RenderingContext::CreateSwapChain() {
  SwapChainSupportDetails swapChainDetails =
      QuerySwapChainSupport(m_PhysicalDevice);

  VkSurfaceFormatKHR surfaceFormat =
      ChooseSwapSurfaceFormat(swapChainDetails.formats);
  VkPresentModeKHR presentMode =
      ChooseSwapPresentMode(swapChainDetails.presentModes);
  VkExtent2D extent = ChooseSwapExtent(swapChainDetails.capabilities);

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

  QueueFamilyIndices indices = FindQueueFamilies(m_PhysicalDevice);
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

  if (vkCreateSwapchainKHR(m_Device, &createInfo, nullptr, &m_SwapChain)) {
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
  vkDestroySwapchainKHR(m_Device, m_SwapChain, nullptr);
}

void RenderingContext::CreateImageViews() {
  m_SwapChainImageViews.resize(m_SwapChainImages.size());

  for (size_t i = 0; i < m_SwapChainImages.size(); ++i) {
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

QueueFamilyIndices
RenderingContext::FindQueueFamilies(VkPhysicalDevice device) {
  QueueFamilyIndices indices;

  uint32_t queueFamilyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

  std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount,
                                           queueFamilies.data());

  int i = 0;
  for (const auto &queueFamily : queueFamilies) {
    if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      indices.graphicsFamily = i;
    }

    VkBool32 presentSupported = false;
    vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_Surface,
                                         &presentSupported);

    if (presentSupported) {
      indices.presentFamily = i;
    }

    if (indices.IsComplete()) {
      break;
    }

    i++;
  }

  return indices;
}

SwapChainSupportDetails
RenderingContext::QuerySwapChainSupport(VkPhysicalDevice device) {
  SwapChainSupportDetails details;

  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_Surface,
                                            &details.capabilities);

  uint32_t formatCount = 0;

  vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_Surface, &formatCount,
                                       nullptr);

  if (formatCount != 0) {
    details.formats.resize(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_Surface, &formatCount,
                                         details.formats.data());
  }

  uint32_t presentModeCount = 0;
  vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_Surface,
                                            &presentModeCount, nullptr);

  if (presentModeCount != 0) {
    details.presentModes.resize(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        device, m_Surface, &presentModeCount, details.presentModes.data());
  }

  return details;
}

VkSurfaceFormatKHR RenderingContext::ChooseSwapSurfaceFormat(
    const std::vector<VkSurfaceFormatKHR> &availableFormats) {

  for (const auto &availableFormat : availableFormats) {
    if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
        availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      return availableFormat;
    }
  }
  return availableFormats[0];
}

VkPresentModeKHR RenderingContext::ChooseSwapPresentMode(
    const std::vector<VkPresentModeKHR> &availablePresentationModes) {

  for (const auto &availablePresentMode : availablePresentationModes) {
    if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
      return availablePresentMode;
    }
  }

  return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D RenderingContext::ChooseSwapExtent(
    const VkSurfaceCapabilitiesKHR &capabilities) {
  if (capabilities.currentExtent.width !=
      std::numeric_limits<uint32_t>::max()) {
    return capabilities.currentExtent;
  } else {
    int width, height;
    glfwGetFramebufferSize(m_Window, &width, &height);

    VkExtent2D actualExtent = {
        .width = static_cast<uint32_t>(width),
        .height = static_cast<uint32_t>(height),
    };

    actualExtent.width =
        std::clamp(actualExtent.width, capabilities.minImageExtent.width,
                   capabilities.maxImageExtent.width);
    actualExtent.height =
        std::clamp(actualExtent.height, capabilities.minImageExtent.height,
                   capabilities.maxImageExtent.height);

    return actualExtent;
  }
}

bool RenderingContext::IsDeviceSuitable(VkPhysicalDevice device) {
  QueueFamilyIndices indices = FindQueueFamilies(device);
  bool extensionsSupported = CheckDeviceExtensionSupport(device);

  bool swapChainAdequate = false;
  if (extensionsSupported) {
    SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(device);
    swapChainAdequate = !swapChainSupport.formats.empty() &&
                        !swapChainSupport.presentModes.empty();
  }

  return indices.IsComplete() && extensionsSupported && swapChainAdequate;
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

} // namespace Inferno
