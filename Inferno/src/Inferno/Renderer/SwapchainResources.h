#pragma once

#include <vulkan/vulkan_core.h>

#include "Inferno/Renderer/Image.h"
#include "Inferno/Renderer/RenderingContext.h"

namespace Inferno {
struct SwapchainResources {
  Image DepthImage;
  VkImageView DepthImageView = VK_NULL_HANDLE;
  std::vector<VkFramebuffer> SwapChainFrameBuffers;

  void static Create(const RenderingContext* context, SwapchainResources& resources);
  void static Destroy(const RenderingContext* context, SwapchainResources& resources);
};

} // namespace Inferno
