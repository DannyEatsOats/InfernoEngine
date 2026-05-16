#pragma once

#include "Inferno/Renderer/DeviceContext.h"
#include <vulkan/vulkan_core.h>
namespace Inferno {
class VulkanUtils {
public:
  static uint32_t FindMemoryType(VkPhysicalDevice physicalDevice,
                                 uint32_t typeFilter,
                                 VkMemoryPropertyFlags properties);

  static void CopyBuffer(const DeviceContext *context, VkBuffer srcBuffer,
                         VkBuffer dstBuffer, VkDeviceSize size);

  static VkCommandBuffer BeginSingleTimeCommands(const DeviceContext *context);
  static void EndSingleTimeCommands(const DeviceContext *context,
                                               VkCommandBuffer commandBuffer);
};
} // namespace Inferno
