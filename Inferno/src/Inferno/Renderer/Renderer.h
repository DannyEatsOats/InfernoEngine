#pragma once

#include "Inferno/ECS/Entity.h"
#include "Inferno/Renderer/DeviceContext.h"
#include "Inferno/Renderer/Pipeline.h"
#include "Inferno/Resource/ResourceManager.h"
#include <array>
#include <cstdint>
#include <optional>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace Inferno {

struct GridPushConstants {
  glm::mat4 View;
  glm::mat4 Proj;
  glm::mat4 ViewInv;
  glm::mat4 ProjInv;
};

struct RenderCamera {
  glm::mat4 View;
  glm::mat4 Proj;
};

struct FrameData {
  VkCommandBuffer m_CommandBuffers = VK_NULL_HANDLE;
  Image m_DepthImages;
  Image m_EntityPickingImages;
  VkSemaphore m_PresentCompleteSemaphores = VK_NULL_HANDLE;
  VkFence m_DrawFences = VK_NULL_HANDLE;
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

  void SetActiveCamera(RenderCamera camera) { m_ActiveCamera = camera; }

  std::optional<uint32_t> PickEntity(int32_t mouseX, int32_t mouseY) const;

private:
  void CreateForwardPipeline();
  void CreateGridPipeline();

  void AllocateCommandBuffer();
  void CreateSyncObjects();

  void TransitionImageLayout(VkCommandBuffer cmd, VkImage image,
                             VkImageAspectFlags aspect, VkImageLayout oldLayout,
                             VkImageLayout newLayout,
                             VkAccessFlags2 srcAccessMask,
                             VkAccessFlags2 dstAccessMask,
                             VkPipelineStageFlags2 srcStageMask,
                             VkPipelineStageFlags2 dstStageMask) const;

  void RecordForwardPass(const std::vector<Entity *> &entities);

  void Resize();

private:
  static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

  // References
  DeviceContext *m_Context = nullptr;
  ResourceManager *m_ResourceManager = nullptr;

  // Pipelines
  Pipeline m_ForwardPipeline{};
  Pipeline m_GridPipeline{};

  // Frame Data
  std::array<VkCommandBuffer, MAX_FRAMES_IN_FLIGHT> m_CommandBuffers;
  std::array<Image, MAX_FRAMES_IN_FLIGHT> m_DepthImages;
  std::array<Image, MAX_FRAMES_IN_FLIGHT> m_EntityPickingImages;
  std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> m_PresentCompleteSemaphores;
  std::array<VkFence, MAX_FRAMES_IN_FLIGHT> m_DrawFences;
  std::array<FrameData, MAX_FRAMES_IN_FLIGHT> m_Frames;

  // Texture Descriptor
  VkDescriptorPool m_TextureDescriptorPool = VK_NULL_HANDLE;
  VkDescriptorSetLayout m_TextureDescriptorSetLayout = VK_NULL_HANDLE;

  std::vector<VkSemaphore> m_RenderFinishedSemaphores;
  uint32_t m_FrameIndex = 0;
  uint32_t m_ImageIndex = 0;

  bool m_Resized = false;

  RenderCamera m_ActiveCamera{};
};
} // namespace Inferno
