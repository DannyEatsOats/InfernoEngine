#pragma once

#include "Inferno/ECS/Entity.h"
#include "Inferno/Renderer/Buffer.h"
#include "Inferno/Renderer/DeviceContext.h"
#include "Inferno/Renderer/Pipeline.h"
#include "Inferno/Resource/ResourceManager.h"
#include "Inferno/Tools/GUISystem.h"
#include <array>
#include <cstdint>
#include <optional>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace Inferno {
class EditorSystem;
// ==========================

struct RenderCamera {
  glm::mat4 View;
  glm::mat4 Proj;
};

enum class RenderFeature : uint32_t {
  NONE = 0,
  SCENE = 1 << 0,
  GRID = 1 << 1,
  GIZMOS = 1 << 2,
  SELECTION_OUTLINE = 1 << 3,
  IMGUI = 1 << 4,
};

constexpr RenderFeature operator|(RenderFeature lhs, RenderFeature rhs) {
  return static_cast<RenderFeature>(static_cast<uint32_t>(lhs) |
                                    static_cast<uint32_t>(rhs));
}

constexpr bool HasRenderFeature(RenderFeature features,
                                RenderFeature feature) {
  return (static_cast<uint32_t>(features) &
          static_cast<uint32_t>(feature)) != 0;
}

struct RenderView {
  RenderCamera Camera;
  RenderFeature Features = RenderFeature::SCENE;
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

  void StartUp(ResourceManager *resourceManager, EditorSystem *editorSystem,
               GUISystem *guiSystem);
  void ShutDown();

  void Render(const std::vector<Entity *> &entities, const RenderView &view);

  void SignalResize() { m_Resized = true; }

  std::optional<uint32_t> PickEntity(int32_t mouseX, int32_t mouseY) const;

  // TODO: Refactor this
  void EDITOR_Update(const std::vector<Entity*>& entities);
  void GAME_Update();

private:
  void CreateForwardPipeline();
  void CreateGridPipeline();
  void CreateOutlinePipeline();
  void CreateGizmoPipeline();

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

  FrameData &BeginFrame();
  void EndFrame(FrameData &frame);

  void RecordForwardPass(FrameData &frame,
                         const std::vector<Entity *> &entities,
                         const RenderView &view);
  void RecordOutlinePass(FrameData &frame);

  void UpdateOutlineDescriptorSets();

  void Resize();

  // NOTE: TEMPORARY CODE, MOVE THIS TO AN EDITOR/DEBUG RENDERER!!!!!!!!!!!!!!!
  void UpdateFrustumGizmo(const glm::mat4 &view, const glm::mat4 &proj);
  void RecordGizmoPass(FrameData &frame, const std::vector<Entity *> &entities);
  std::array<Scope<VertexBuffer<GizmoVertex>>, 2> m_GizmoVertexBuffers;
  // NOTE: TEMPORARY CODE END

private:
  static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

  // References
  DeviceContext *m_Context = nullptr;
  ResourceManager *m_ResourceManager = nullptr;
  EditorSystem *m_EditorSystem = nullptr;
  GUISystem *m_GUISystem = nullptr;

  // Pipelines
  Pipeline m_ForwardPipeline{};
  Pipeline m_GridPipeline{};
  Pipeline m_OutlinePipeline{};
  Pipeline m_GizmoPipeline{};

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
