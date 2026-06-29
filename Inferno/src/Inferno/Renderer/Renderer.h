#pragma once

#include "Inferno/Renderer/CullingSystem.h"
#include "Inferno/Renderer/DeviceContext.h"
#include "Inferno/Resource/ResourceManager.h"
#include <array>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace Inferno {

struct GridPushConstants {
  glm::mat4 View;
  glm::mat4 Proj;
};

class Renderer {
public:
  Renderer(DeviceContext *context) : m_Context(context) {}
  ~Renderer() = default;

  Renderer(const Renderer &) = delete;
  Renderer(Renderer &&) = delete;
  Renderer &operator=(const Renderer &) = delete;
  Renderer &operator=(Renderer &&) = delete;

  void StartUp(ResourceManager *resourceManager);
  void ShutDown();

  void Render(const std::vector<Entity *> &entities);

  void SignalResize() { m_Resized = true; }

private:
  void CreateForwardPipeline();
  void CreateGridPipeline();

  void AllocateCommandBuffer();
  void CreateSyncObjects();

  void TransitionImageLayout(VkImage image, VkImageAspectFlags aspect,
                             VkImageLayout oldLayout, VkImageLayout newLayout,
                             VkAccessFlags2 srcAccessMask,
                             VkAccessFlags2 dstAccessMask,
                             VkPipelineStageFlags2 srcStageMask,
                             VkPipelineStageFlags2 dstStageMask);

  void RecordForwardPass();

  void Resize();

private:
  DeviceContext *m_Context = nullptr;

  static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

  Scope<CullingSystem> m_CullingSystem = nullptr;
  ResourceManager *m_ResourceManager = nullptr;

  VkPipeline m_ForwardPipeline = VK_NULL_HANDLE;
  VkPipelineLayout m_ForwardLayout = VK_NULL_HANDLE;

  VkPipeline m_GridPipeline = VK_NULL_HANDLE;
  VkPipelineLayout m_GridLayout = VK_NULL_HANDLE;

  std::array<VkCommandBuffer, MAX_FRAMES_IN_FLIGHT> m_CommandBuffers;
  std::array<Image, MAX_FRAMES_IN_FLIGHT> m_DepthImages;

  VkDescriptorPool m_TextureDescriptorPool = VK_NULL_HANDLE;
  VkDescriptorSetLayout m_TextureDescriptorSetLayout = VK_NULL_HANDLE;

  std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> m_PresentCompleteSemaphores;
  std::vector<VkSemaphore> m_RenderFinishedSemaphores;
  std::array<VkFence, MAX_FRAMES_IN_FLIGHT> m_DrawFences;
  uint32_t m_FrameIndex = 0;
  uint32_t m_ImageIndex = 0;

  bool m_Resized = false;

  std::vector<Entity *> m_VisibleEntites;
};
} // namespace Inferno
