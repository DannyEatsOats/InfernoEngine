#pragma once

#include "Inferno/Renderer/Buffer.h"
#include "Inferno/Renderer/RenderingContext.h"
#include <cstdint>
#include <memory>
#include <vector>
#include <vulkan/vulkan_core.h>
namespace Inferno {
class Renderer {
public:
  Renderer() = default;
  ~Renderer() = default;

  void Init();
  void ShutDown();

  void DrawFrame();

  void OnWindowResize(uint32_t width, uint32_t height);

private:
  void CreateRenderPass();
  void CreateDescriptorSetLayout();
  void CreateDescriptorPool();
  void CreateDescriptorSets();
  void CreateGraphicsPipeline();
  void CreateFramebuffers();
  void CreateCommandBuffers();
  void CreateSyncObjects();
  void RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
  void RecreateSwapChain();

  void UpdateUniformBuffer(uint32_t currentImage);

private:
  std::vector<VkFramebuffer> m_SwapChainFrameBuffers;

  VkRenderPass m_RenderPass;
  VkPipelineLayout m_PipelineLayout;
  VkPipeline m_GraphicsPipeline;

  std::vector<VkCommandBuffer> m_CommandBuffers;

  VkDescriptorPool m_DescriptorPool;
  VkDescriptorSetLayout m_DescriptorSetLayout;
  std::vector<VkDescriptorSet> m_DescriptorSets;

  std::vector<VkSemaphore> m_ImageAvailableSemaphores;
  std::vector<VkSemaphore> m_RenderFinishedSemaphores;
  std::vector<VkFence> m_InFlightFences;
  bool m_FrameBufferResized = false;

  uint32_t m_CurrentFrame;

  std::shared_ptr<RenderingContext> m_pContext = nullptr;

  std::shared_ptr<VertexBuffer> m_VertexBuffer = nullptr;
  std::shared_ptr<IndexBuffer> m_IndexBuffer = nullptr;
  std::vector<std::shared_ptr<UniformBuffer>> m_UniformBuffers;

  static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
};
} // namespace Inferno
