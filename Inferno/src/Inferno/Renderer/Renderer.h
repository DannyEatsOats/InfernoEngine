#pragma once

#include "Inferno/ECS/Entity.h"
#include "Inferno/Renderer/DeviceContext.h"
#include "Inferno/Renderer/Pipeline.h"
#include "Inferno/Resource/ResourceManager.h"
#include "glm/ext/vector_float3.hpp"
#include <array>
#include <cstdint>
#include <optional>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace Inferno {
class EditorSystem;

// Push Constants
struct MeshPushConstants {
  glm::mat4 Mvp;
  glm::mat4 Model;
  uint32_t EntityID;
};

struct OutlinePushConstants {
  glm::vec3 Color;
  uint32_t EntityID;
  uint32_t ThicknessPX;
};

struct GridPushConstants {
  glm::mat4 View;
  glm::mat4 Proj;
  glm::mat4 ViewInv;
  glm::mat4 ProjInv;
};
// ==========================

struct RenderCamera {
  glm::mat4 View;
  glm::mat4 Proj;
};

struct FrameData {
  VkCommandBuffer CommandBuffer = VK_NULL_HANDLE;
  Image DepthImage;
  Image EntityPickingImage;
  VkSemaphore PresentCompleteSemaphore = VK_NULL_HANDLE;
  VkFence DrawFence = VK_NULL_HANDLE;
};

class Renderer {
public:
  Renderer(DeviceContext *context) : m_Context(context) {}
  ~Renderer() = default;

  Renderer(const Renderer &) = delete;
  Renderer(Renderer &&) = delete;
  Renderer &operator=(const Renderer &) = delete;
  Renderer &operator=(Renderer &&) = delete;

  void StartUp(ResourceManager *resourceManager, EditorSystem *editorSystem);
  void ShutDown();

  void Render(const std::vector<Entity *> &entities);

  void SignalResize() { m_Resized = true; }

  void SetActiveCamera(RenderCamera camera) { m_ActiveCamera = camera; }

  std::optional<uint32_t> PickEntity(int32_t mouseX, int32_t mouseY) const;

private:
  void CreateForwardPipeline();
  void CreateGridPipeline();
  void CreateOutlinePipeline();

  void CreateOutlineDescriptorResources();

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
  void RecordOutlinePass(VkCommandBuffer cmd, FrameData &frame);

  void UpdateOutlineDescriptorSets();

  void Resize();

private:
  static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

  // References
  DeviceContext *m_Context = nullptr;
  ResourceManager *m_ResourceManager = nullptr;
  EditorSystem *m_EditorSystem = nullptr;

  // Pipelines
  Pipeline m_ForwardPipeline{};
  Pipeline m_GridPipeline{};
  Pipeline m_OutlinePipeline{};

  // Frame Data
  std::array<FrameData, MAX_FRAMES_IN_FLIGHT> m_Frames;

  // Texture Descriptor
  VkDescriptorPool m_TextureDescriptorPool = VK_NULL_HANDLE;
  VkDescriptorSetLayout m_TextureDescriptorSetLayout = VK_NULL_HANDLE;

  // Outline Descriptor
  VkDescriptorPool m_OutlineDescriptorPool = VK_NULL_HANDLE;
  VkDescriptorSetLayout m_OutlineDescriptorSetLayout = VK_NULL_HANDLE;
  std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> m_OutlineDescriptorSets{};
  VkSampler m_OutlineSampler = VK_NULL_HANDLE;

  std::vector<VkSemaphore> m_RenderFinishedSemaphores;
  uint32_t m_FrameIndex = 0;
  uint32_t m_ImageIndex = 0;

  bool m_Resized = false;

  RenderCamera m_ActiveCamera{};
};
} // namespace Inferno
