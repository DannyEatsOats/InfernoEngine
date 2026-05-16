#pragma once

#include <vulkan/vulkan_core.h>

namespace Inferno {
struct DeviceContext {
  VkDevice Device = VK_NULL_HANDLE;
  VkPhysicalDevice PhysicalDevice = VK_NULL_HANDLE;
  VkQueue GraphicsQueue = VK_NULL_HANDLE;
  VkCommandPool CommandPool = VK_NULL_HANDLE;
};
} // namespace Inferno
