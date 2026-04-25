#pragma once

#include "Inferno/Renderer/RenderingContext.h"
#include "Inferno/Window.h"
#include <optional>
#include <vulkan/vulkan_core.h>

struct GLFWwindow;

namespace Inferno {
namespace VulkanUtils {
struct QueueFamilyIndices {
  std::optional<uint32_t> graphicsFamily;
  std::optional<uint32_t> presentFamily;

  bool IsComplete() const {
    return graphicsFamily.has_value() && presentFamily.has_value();
  }
};

struct SwapChainSupportDetails {
  VkSurfaceCapabilitiesKHR capabilities;
  std::vector<VkSurfaceFormatKHR> formats;
  std::vector<VkPresentModeKHR> presentModes;
};

static uint32_t FindMemoryType(VkPhysicalDevice physicalDevice,
                               uint32_t typeFilter,
                               VkMemoryPropertyFlags properties);

static QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device,
                                            VkSurfaceKHR surface);

static SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device,
                                                     VkSurfaceKHR surface);

static VkSurfaceFormatKHR ChooseSwapSurfaceFormat(
    const std::vector<VkSurfaceFormatKHR> &availableFormats);

static VkPresentModeKHR ChooseSwapPresentMode(
    const std::vector<VkPresentModeKHR> &availablePresentationModes);

static VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities,
                                   Window *window);

// Image Creation

struct ImageSpec {
  uint32_t Height = 0;
  uint32_t Width = 0;
  VkFormat Format = VK_FORMAT_R8G8B8A8_SRGB;
  VkImageTiling Tiling = VK_IMAGE_TILING_OPTIMAL;
  VkImageUsageFlags Usage =
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  VkMemoryPropertyFlags Properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
};

struct AllocatedImage {
  VkImage Image = VK_NULL_HANDLE;
  VkDeviceMemory Memory = VK_NULL_HANDLE;
};

AllocatedImage CreateImage(const RenderingContext *context, const ImageSpec &spec);

}; // namespace VulkanUtils
} // namespace Inferno
