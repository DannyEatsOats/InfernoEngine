#pragma once

#include "Inferno/Renderer/DeviceContext.h"
#include <vulkan/vulkan_core.h>
namespace Inferno {
class VulkanUtils {
public:
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

  static void TransitionImageLayout(const DeviceContext *context, VkImage image,
                                    VkFormat format, VkImageLayout oldLayout,
                                    VkImageLayout newLayout);

  static void CopyBufferToImage(const DeviceContext *context, VkBuffer buffer,
                                VkImage image, uint32_t width, uint32_t height);
};
} // namespace Inferno
