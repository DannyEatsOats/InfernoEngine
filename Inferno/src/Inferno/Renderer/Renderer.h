#pragma once

#include "Inferno/Renderer/Buffer.h"
#include "Inferno/Renderer/Image.h"
#include "Inferno/Renderer/RenderingContext.h"
#include "Inferno/Renderer/SwapchainResources.h"
#include "Inferno/Renderer/Texture.h"
#include <cstdint>
#include <memory>
#include <vector>
#include <vulkan/vulkan_core.h>
namespace Inferno {

struct FrameData {
  std::unique_ptr<UniformBuffer> UniBuffer = nullptr;
  VkDescriptorSet DescriptorSet = VK_NULL_HANDLE;
  VkCommandBuffer CommandBuffer = VK_NULL_HANDLE;
  VkSemaphore ImageAvailable = VK_NULL_HANDLE;
  VkSemaphore RenderFinished = VK_NULL_HANDLE;
  VkFence InFlight = VK_NULL_HANDLE;
};

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
  void CreateDepthResources();
  void CreateGraphicsPipeline();
  void CreateFramebuffers();
  void CreateCommandBuffers();
  void CreateSyncObjects();
  void RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
  void RecreateSwapChain();

  void UpdateUniformBuffer(uint32_t currentImage);

private:
  VkRenderPass m_RenderPass = VK_NULL_HANDLE;

  SwapchainResources m_SwapchainResources;

  VkPipelineLayout m_PipelineLayout;
  VkPipeline m_GraphicsPipeline;

  std::vector<VkCommandBuffer> m_CommandBuffers;

  VkDescriptorPool m_DescriptorPool;
  VkDescriptorSetLayout m_DescriptorSetLayout;
  bool m_FrameBufferResized = false;

  uint32_t m_CurrentFrame;

  std::vector<FrameData> m_Frames;

  std::shared_ptr<RenderingContext> m_pContext = nullptr;

  std::shared_ptr<VertexBuffer> m_VertexBuffer = nullptr;
  std::shared_ptr<IndexBuffer> m_IndexBuffer = nullptr;

  std::shared_ptr<Texture> m_Texture = nullptr;

  static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
};
} // namespace Inferno
