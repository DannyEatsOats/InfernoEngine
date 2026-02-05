#pragma once

#include "Inferno/Renderer/RenderingContext.h"
#include <memory>
#include <vulkan/vulkan_core.h>
namespace Inferno {
class Renderer {
public:
  Renderer() = default;
  ~Renderer() = default;

  void Init();
  void ShutDown();

  void DrawFrame();

private:
  void CreateRenderPass();
  void CreateGraphicsPipeline();
  void CreateFramebuffers();
  void CreateCommandPool();
  void CreateCommandBuffers();
  void CreateSyncObjects();
  void RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);

private:
  std::vector<VkFramebuffer> m_SwapChainFrameBuffers;

  VkRenderPass m_RenderPass;
  VkPipelineLayout m_PipelineLayout;
  VkPipeline m_GraphicsPipeline;

  VkCommandPool m_CommandPool;
  std::vector<VkCommandBuffer> m_CommandBuffers;

  std::vector<VkSemaphore> m_ImageAvailableSemaphores;
  std::vector<VkSemaphore> m_RenderFinishedSemaphores;
  std::vector<VkFence> m_InFlightFences;

  uint32_t m_CurrentFrame;

  std::shared_ptr<RenderingContext> m_Context;

  static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
};
} // namespace Inferno
