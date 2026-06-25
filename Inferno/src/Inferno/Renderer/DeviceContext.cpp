#include "DeviceContext.h"
#include "Inferno/Core/Log.h"
#include "vulkan/vulkan_core.h"
#include <Inferno/Renderer/VulkanUtils.h>
#include <cstdint>
#include <cstring>
#include <pch.h>
#include <utility>
#include <vulkan/vulkan_core.h>

const std::vector<const char *> validationLayers = {
    "VK_LAYER_KHRONOS_validation"};

const std::vector<const char *> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME};

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
bool CheckDeviceExtensionSupport(VkPhysicalDevice device) {
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

VkSampleCountFlagBits GetMaxUsabelSampleCount(VkPhysicalDevice device) {
  VkPhysicalDeviceProperties physicalDeviceProps;
  vkGetPhysicalDeviceProperties(device, &physicalDeviceProps);

  VkSampleCountFlags counts =
      physicalDeviceProps.limits.framebufferColorSampleCounts &
      physicalDeviceProps.limits.framebufferDepthSampleCounts;

  if (counts & VK_SAMPLE_COUNT_64_BIT) {
    return VK_SAMPLE_COUNT_64_BIT;
  }
  if (counts & VK_SAMPLE_COUNT_32_BIT) {
    return VK_SAMPLE_COUNT_32_BIT;
  }
  if (counts & VK_SAMPLE_COUNT_32_BIT) {
    return VK_SAMPLE_COUNT_32_BIT;
  }
  if (counts & VK_SAMPLE_COUNT_16_BIT) {
    return VK_SAMPLE_COUNT_16_BIT;
  }
  if (counts & VK_SAMPLE_COUNT_8_BIT) {
    return VK_SAMPLE_COUNT_8_BIT;
  }
  if (counts & VK_SAMPLE_COUNT_4_BIT) {
    return VK_SAMPLE_COUNT_4_BIT;
  }
  if (counts & VK_SAMPLE_COUNT_2_BIT) {
    return VK_SAMPLE_COUNT_2_BIT;
  }

  return VK_SAMPLE_COUNT_1_BIT;
}

bool IsDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface) {
  VulkanUtils::QueueFamilyIndices indices =
      VulkanUtils::FindQueueFamilies(device, surface);
  bool extensionsSupported = CheckDeviceExtensionSupport(device);

  bool swapChainAdequate = false;
  if (extensionsSupported) {
    VulkanUtils::SwapChainSupportDetails swapChainSupport =
        VulkanUtils::QuerySwapChainSupport(device, surface);
    swapChainAdequate = !swapChainSupport.formats.empty() &&
                        !swapChainSupport.presentModes.empty();
  }

  VkPhysicalDeviceFeatures supportedFeatures;
  vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

  VkFormatProperties formatProperties;
  vkGetPhysicalDeviceFormatProperties(device, VK_FORMAT_R8G8B8A8_SRGB,
                                      &formatProperties);

  bool formatSupportsBlitting =
      (formatProperties.optimalTilingFeatures &
       VK_FORMAT_FEATURE_BLIT_SRC_BIT) &&
      (formatProperties.optimalTilingFeatures &
       VK_FORMAT_FEATURE_BLIT_DST_BIT) &&
      (formatProperties.optimalTilingFeatures &
       VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT);

  return indices.IsComplete() && extensionsSupported && swapChainAdequate &&
         supportedFeatures.samplerAnisotropy && formatSupportsBlitting;
}

void DeviceContext::StartUp(void *windowHandle) {
  WindowHandle = windowHandle;

  CreateInstance();
  CreateSurface();
  PickPhysicalDevice();
  CreateLogicalDevice();
  CreateSwapchain();
  CreateSwapchainImageViews();
  CreateCommandPool();
}

void DeviceContext::ShutDown() {
  if (Device != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(Device);
  }

  if (CommandPool != VK_NULL_HANDLE) {
    vkDestroyCommandPool(Device, CommandPool, nullptr);
    CommandPool = VK_NULL_HANDLE;
  }

  CleanupSwapchain();

  if (Device)
    vkDestroyDevice(Device, nullptr);

  if (Surface)
    vkDestroySurfaceKHR(Instance, Surface, nullptr);

  if (Instance)
    vkDestroyInstance(Instance, nullptr);

  Device = VK_NULL_HANDLE;
  Surface = VK_NULL_HANDLE;
  Instance = VK_NULL_HANDLE;
}

std::pair<VkResult, uint32_t>
DeviceContext::AcquireNextImage(VkSemaphore presentCompleteSemaphore) {
  uint32_t imageIndex;
  VkResult result =
      vkAcquireNextImageKHR(Device, Swapchain.Handle, UINT64_MAX,
                            presentCompleteSemaphore, nullptr, &imageIndex);
  return std::make_pair(result, imageIndex);
}

void DeviceContext::CreateInstance() {
  VkApplicationInfo appInfo{};
  appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appInfo.pApplicationName = "Inferno Engine";
  appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.pEngineName = "No Engine";
  appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.apiVersion = VK_API_VERSION_1_4;

  uint32_t glfwExtCount = 0;
  const char **glfwExtensions =
      glfwGetRequiredInstanceExtensions(&glfwExtCount);

  if (enableValidationLayers && !checkValidationlayerSupport()) {
    throw std::runtime_error("Validation layers requested, but not available");
  }

  VkInstanceCreateInfo instanceInfo{};
  instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instanceInfo.pApplicationInfo = &appInfo;
  instanceInfo.enabledLayerCount = 0;
  instanceInfo.ppEnabledLayerNames = nullptr;
  instanceInfo.enabledExtensionCount = glfwExtCount;
  instanceInfo.ppEnabledExtensionNames = glfwExtensions;

  if (enableValidationLayers) {
    instanceInfo.enabledLayerCount =
        static_cast<uint32_t>(validationLayers.size());
    instanceInfo.ppEnabledLayerNames = validationLayers.data();
  }

  if (vkCreateInstance(&instanceInfo, nullptr, &Instance) != VK_SUCCESS) {
    INFERNO_LOG_ERROR("Failed to create VkInstance");
    throw std::runtime_error("Failed to create VkInstance");
  }
}

void DeviceContext::CreateSurface() {
  if (glfwCreateWindowSurface(Instance, static_cast<GLFWwindow *>(WindowHandle),
                              nullptr, &Surface) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create Vulkan Surface");
  }
}

void DeviceContext::PickPhysicalDevice() {
  uint32_t deviceCount = 0;
  vkEnumeratePhysicalDevices(Instance, &deviceCount, nullptr);
  if (deviceCount == 0) {
    throw std::runtime_error("No Vulkan GPUs found");
  }

  std::vector<VkPhysicalDevice> devices(deviceCount);

  vkEnumeratePhysicalDevices(Instance, &deviceCount, devices.data());

  for (const auto &device : devices) {
    if (IsDeviceSuitable(device, Surface)) {
      PhysicalDevice = device;
      PhysicalDeviceProps.MSAAsamples = GetMaxUsabelSampleCount(device);
      break;
    }
  }

  if (PhysicalDevice == VK_NULL_HANDLE) {
    throw std::runtime_error("Failed to find suitable GPU");
  }
}

void DeviceContext::CreateLogicalDevice() {
  VulkanUtils::QueueFamilyIndices deviceIndices =
      VulkanUtils::FindQueueFamilies(PhysicalDevice, Surface);

  std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
  std::set<uint32_t> uniqueQueueFamilies = {
      deviceIndices.graphicsFamily.value(),
      deviceIndices.presentFamily.value()};

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

  VkPhysicalDeviceVulkan11Features features11{
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
      .shaderDrawParameters = VK_TRUE,
  };

  VkPhysicalDeviceVulkan13Features features13{};
  features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
  features13.pNext = &features11;
  features13.dynamicRendering = VK_TRUE;

  VkDeviceCreateInfo deviceInfo = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .pNext = &features13,
      .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
      .pQueueCreateInfos = queueCreateInfos.data(),
      .enabledLayerCount = 0,
      .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
      .ppEnabledExtensionNames = deviceExtensions.data(),
      .pEnabledFeatures = &deviceFeatures,
  };

  if (enableValidationLayers) {
    deviceInfo.enabledLayerCount =
        static_cast<uint32_t>(validationLayers.size());
    deviceInfo.ppEnabledLayerNames = validationLayers.data();
  }

  if (vkCreateDevice(PhysicalDevice, &deviceInfo, nullptr, &Device) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed to create logical deivce");
  }

  vkGetDeviceQueue(Device, deviceIndices.graphicsFamily.value(), 0,
                   &GraphicsQueue);
  vkGetDeviceQueue(Device, deviceIndices.presentFamily.value(), 0,
                   &PresentQueue);
}

void DeviceContext::CreateSwapchain() {
  VulkanUtils::SwapChainSupportDetails swapChainDetails =
      VulkanUtils::QuerySwapChainSupport(PhysicalDevice, Surface);

  VkSurfaceFormatKHR surfaceFormat =
      VulkanUtils::ChooseSwapSurfaceFormat(swapChainDetails.formats);
  VkPresentModeKHR presentMode =
      VulkanUtils::ChooseSwapPresentMode(swapChainDetails.presentModes);
  VkExtent2D extent = VulkanUtils::ChooseSwapExtent(
      swapChainDetails.capabilities, static_cast<GLFWwindow *>(WindowHandle));

  uint32_t imageCount = swapChainDetails.capabilities.minImageCount + 1;

  if (swapChainDetails.capabilities.maxImageCount > 0 &&
      imageCount > swapChainDetails.capabilities.maxImageCount) {
    imageCount = swapChainDetails.capabilities.maxImageCount;
  }

  VkSwapchainCreateInfoKHR swapchainInfo{
      .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      .surface = Surface,
      .minImageCount = imageCount,
      .imageFormat = surfaceFormat.format,
      .imageColorSpace = surfaceFormat.colorSpace,
      .imageExtent = extent,
      .imageArrayLayers = 1,
      .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
  };

  VulkanUtils::QueueFamilyIndices swapchainIndices =
      VulkanUtils::FindQueueFamilies(PhysicalDevice, Surface);
  uint32_t queueFamilyIndices[] = {swapchainIndices.graphicsFamily.value(),
                                   swapchainIndices.presentFamily.value()};

  if (swapchainIndices.graphicsFamily != swapchainIndices.presentFamily) {
    swapchainInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    swapchainInfo.queueFamilyIndexCount = 2;
    swapchainInfo.pQueueFamilyIndices = queueFamilyIndices;
  } else {
    swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchainInfo.queueFamilyIndexCount = 0;
    swapchainInfo.pQueueFamilyIndices = nullptr;
  }

  swapchainInfo.preTransform = swapChainDetails.capabilities.currentTransform;
  swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  swapchainInfo.presentMode = presentMode;
  swapchainInfo.clipped = VK_TRUE;
  swapchainInfo.oldSwapchain = VK_NULL_HANDLE;

  INFERNO_LOG_ERROR("Width {}, Height {}", swapchainInfo.imageExtent.width,
                    swapchainInfo.imageExtent.height);

  if (vkCreateSwapchainKHR(Device, &swapchainInfo, nullptr,
                           &Swapchain.Handle) != VK_SUCCESS) {
    throw std::runtime_error("Failed to Create SwapChain");
  }

  vkGetSwapchainImagesKHR(Device, Swapchain.Handle, &imageCount, nullptr);
  Swapchain.Images.resize(imageCount);
  vkGetSwapchainImagesKHR(Device, Swapchain.Handle, &imageCount,
                          Swapchain.Images.data());

  Swapchain.Format = surfaceFormat.format;
  Swapchain.Extent = extent;
}

void DeviceContext::CreateSwapchainImageViews() {
  Swapchain.ImageViews.resize(Swapchain.Images.size());

  for (size_t i = 0; i < Swapchain.Images.size(); ++i) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = Swapchain.Images[i];
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = Swapchain.Format;

    viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(Device, &viewInfo, nullptr,
                          &Swapchain.ImageViews[i]) != VK_SUCCESS) {
      throw std::runtime_error("Failed to create swapchain image views");
    }
  }
}

void DeviceContext::CreateCommandPool() {
  VulkanUtils::QueueFamilyIndices commandPoolIndices =
      VulkanUtils::FindQueueFamilies(PhysicalDevice, Surface);

  VkCommandPoolCreateInfo poolCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = commandPoolIndices.graphicsFamily.value(),
  };

  if (vkCreateCommandPool(Device, &poolCreateInfo, nullptr, &CommandPool) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed to create command pool!");
  }
}

void DeviceContext::CleanupSwapchain() {
  for (auto imageView : Swapchain.ImageViews) {
    vkDestroyImageView(Device, imageView, nullptr);
  }

  vkDestroySwapchainKHR(Device, Swapchain.Handle, nullptr);
  Swapchain.Images.resize(0);
  Swapchain.ImageViews.resize(0);
}

void DeviceContext::RecreateSwapchain() {
  vkDeviceWaitIdle(Device);

  CleanupSwapchain();
  CreateSwapchain();
  CreateSwapchainImageViews();
}

} // namespace Inferno
