#pragma once

#include <vector>
#include <vulkan/vulkan_core.h>

namespace Inferno {
struct SwapchainConfig {
  VkSwapchainKHR Handle = VK_NULL_HANDLE;
  std::vector<VkImage> Images;
  std::vector<VkImageView> ImageViews;
  VkFormat Format;
  VkExtent2D Extent;
};

struct PhysicalDeviceProperties {
  VkSampleCountFlagBits MSAAsamples = VK_SAMPLE_COUNT_1_BIT;
};

struct DeviceContext {
  VkInstance Instance = VK_NULL_HANDLE;
  VkDevice Device = VK_NULL_HANDLE;
  VkSurfaceKHR Surface = VK_NULL_HANDLE;
  VkPhysicalDevice PhysicalDevice = VK_NULL_HANDLE;
  VkQueue GraphicsQueue = VK_NULL_HANDLE;
  VkQueue PresentQueue = VK_NULL_HANDLE;
  SwapchainConfig Swapchain{};
  PhysicalDeviceProperties PhysicalDeviceProps{};
  VkCommandPool GraphicsCommandPool = VK_NULL_HANDLE;
  VkCommandPool TransientCommandPool = VK_NULL_HANDLE;

  void *WindowHandle = nullptr;

  void StartUp(void *windowHandle);
  void ShutDown();

  std::pair<VkResult, uint32_t>
  AcquireNextImage(VkSemaphore presentCompleteSemaphore);

  void RecreateSwapchain();

private:
  void CreateInstance();
  void CreateSurface();
  void PickPhysicalDevice();
  void CreateLogicalDevice();
  void CreateSwapchain();
  void CreateSwapchainImageViews();
  void CreateCommandPools();

  void CleanupSwapchain();
};
} // namespace Inferno
