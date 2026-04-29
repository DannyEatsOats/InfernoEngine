#pragma once

#include "Inferno/Renderer/Image.h"
#include "Inferno/Renderer/RenderingContext.h"
#include "Inferno/Window.h"
#include <optional>
#include <vulkan/vulkan_core.h>

struct GLFWwindow;

namespace Inferno {
class VulkanUtils {
public:
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

  static VkExtent2D
  ChooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities,
                   Window *window);

  static VkPhysicalDeviceProperties
  GetPhysicalDeviceProps(VkPhysicalDevice device);

  // Command Buffers

  static VkCommandBuffer
  BeginSingleTimeCommands(const RenderingContext *context);

  static void EndSingleTimeCommands(const RenderingContext *context,
                                    VkCommandBuffer commandBuffer);

  // Buffer Stuff
  static void CopyBuffer(const RenderingContext *context, VkBuffer srcBuffer,
                         VkBuffer dstBuffer, VkDeviceSize size);

  static void CopyBufferToImage(const RenderingContext *context,
                                VkBuffer buffer, VkImage image, uint32_t width,
                                uint32_t height);

  static void TransitionImageLayout(const RenderingContext *context,
                                    VkImage image, VkFormat format,
                                    VkImageLayout oldLayout,
                                    VkImageLayout newLayout);

  // Image View

  static VkImageView CreateImageView(const RenderingContext *context,
                                     Image &image);
};
} // namespace Inferno
