#pragma once

#include "Inferno/Renderer/DeviceContext.h"
#include "Inferno/Renderer/Image.h"
#include <optional>
#include <vulkan/vulkan_core.h>
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
  static QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device,
                                              VkSurfaceKHR surface);

  struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
  };
  static SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device,
                                                       VkSurfaceKHR surface);

  static VkSurfaceFormatKHR ChooseSwapSurfaceFormat(
      const std::vector<VkSurfaceFormatKHR> &availableFormats);

  static VkPresentModeKHR ChooseSwapPresentMode(
      const std::vector<VkPresentModeKHR> &availablePresentationModes);

  static VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities,
                              GLFWwindow *window);

  static VkPhysicalDeviceProperties
  GetPhysicalDeviceProps(VkPhysicalDevice device);

  static uint32_t FindMemoryType(VkPhysicalDevice physicalDevice,
                                 uint32_t typeFilter,
                                 VkMemoryPropertyFlags properties);

  static void CopyBuffer(const DeviceContext *context, VkBuffer srcBuffer,
                         VkBuffer dstBuffer, VkDeviceSize size);

  static VkCommandBuffer BeginSingleTimeCommands(const DeviceContext *context);
  static void EndSingleTimeCommands(const DeviceContext *context,
                                    VkCommandBuffer commandBuffer);

  static void TransitionImageLayout(const DeviceContext *context, const Image& image, VkImageLayout oldLayout,
                                    VkImageLayout newLayout);

  static void CopyBufferToImage(const DeviceContext *context, VkBuffer buffer,
                                VkImage image, uint32_t width, uint32_t height);
};
} // namespace Inferno
