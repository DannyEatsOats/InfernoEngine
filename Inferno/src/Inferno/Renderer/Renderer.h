#pragma once

#include "Inferno/ECS/Entity.h"
#include "Inferno/Renderer/Buffer.h"
#include "Inferno/Renderer/DeviceContext.h"
#include "Inferno/Renderer/Pipeline.h"
#include "Inferno/Renderer/RenderWorld.h"
#include "Inferno/Renderer/Vertices.h"
#include "Inferno/Resource/ResourceManager.h"
#include "Inferno/Tools/GUISystem.h"
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace Inferno {
// ==========================

struct RenderCamera {
  glm::mat4 View;
  glm::mat4 Proj;
  float NearPlane = 0.1f;
  float FarPlane = 100.0f;
};

enum class RenderFeature : uint32_t {
  NONE = 0,
  SCENE = 1 << 0,
  GRID = 1 << 1,
  DEBUG_LINES = 1 << 2,
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

enum class GBufferDebugView : uint32_t {
  LIT = 0,
  ALBEDO = 1,
  NORMALS = 2,
  METALLIC = 3,
  ROUGHNESS = 4,
  DEPTH = 5,
  ENTITY_ID = 6,
};

struct RenderView {
  RenderCamera Camera;
  RenderFeature Features = RenderFeature::SCENE;
  uint32_t SelectedEntity = Entity::NULL_ENTITY;
  std::span<const DebugLineVertex> DebugLines{};
  GBufferDebugView GBufferView = GBufferDebugView::LIT;
  float Exposure = 1.0f;
};

struct GBuffer {
  static constexpr VkFormat AlbedoMetallicFormat =
      VK_FORMAT_R8G8B8A8_UNORM;
  static constexpr VkFormat NormalRoughnessFormat =
      VK_FORMAT_R16G16B16A16_SFLOAT;
  static constexpr VkFormat DepthFormat = VK_FORMAT_D32_SFLOAT;
  static constexpr VkFormat EntityIDFormat = VK_FORMAT_R32_UINT;

  Image AlbedoMetallic;
  Image NormalRoughness;
  Image Depth;
  Image EntityID;
};

struct HDRSceneColor {
  static constexpr VkFormat Format = VK_FORMAT_R16G16B16A16_SFLOAT;

  Image Color;
};

struct FrameData {
  VkCommandBuffer CommandBuffer = VK_NULL_HANDLE;
  GBuffer GeometryBuffer;
  HDRSceneColor SceneColor;
  Scope<StorageBuffer> PointLightBuffer;
  Scope<StorageBuffer> SpotLightBuffer;
  Scope<Buffer> TilePointLightCounts;
  Scope<Buffer> TilePointLightIndices;
  Scope<Buffer> TileSpotLightCounts;
  Scope<Buffer> TileSpotLightIndices;
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

  void StartUp(ResourceManager *resourceManager, GUISystem *guiSystem);
  void ShutDown();

  void Render(const RenderWorld &renderWorld, const RenderView &view);

  void SignalResize() { m_Resized = true; }

  std::optional<uint32_t> PickEntity(int32_t mouseX, int32_t mouseY) const;

private:
  void CreateGBufferImages();
  void CreateHDRSceneColorImages();
  void CreateLightBuffers();
  void CreateTiledLightCullingBuffers();
  void CreateTextureDescriptorResources();
  void CreateGBufferPipeline();
  void CreateGBufferDescriptorResources();
  void CreateGBufferDebugPipeline();
  void CreateDeferredLightingPipeline();
  void CreateTiledLightCullingPipeline();
  void CreateToneMappingDescriptorResources();
  void CreateToneMappingPipeline();
  void CreateForwardPipeline();
  void CreateGridPipeline();
  void CreateOutlinePipeline();
  void CreateDebugLinePipeline();

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

  void RecordGBufferPass(FrameData &frame, const RenderWorld &renderWorld);
  void RecordGBufferDebugPass(FrameData &frame, const RenderView &view);
  void RecordDeferredLightingPass(FrameData &frame,
                                  const RenderWorld &renderWorld,
                                  const RenderView &view);
  void RecordTiledLightCullingPass(FrameData &frame,
                                   const RenderWorld &renderWorld,
                                   const RenderView &view);
  void RecordToneMappingPass(FrameData &frame, const RenderView &view);
  void RecordGridPass(FrameData &frame, const RenderView &view);
  void RecordForwardPass(FrameData &frame, const RenderWorld &renderWorld);
  void RecordOutlinePass(FrameData &frame, const RenderView &view);

  void UpdateGBufferDescriptorSets();
  void UpdateToneMappingDescriptorSets();
  void UpdateOutlineDescriptorSets();
  void UpdateLightBuffers(const RenderWorld &renderWorld);

  void Resize();

  void UpdateDebugLineBuffer(std::span<const DebugLineVertex> debugLines);
  void RecordDebugLinePass(FrameData &frame, const RenderView &view);
  std::array<Scope<VertexBuffer<DebugLineVertex>>, 2>
      m_DebugLineVertexBuffers;

private:
  static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;
  static constexpr uint32_t MAX_POINT_LIGHTS = 1024;
  static constexpr uint32_t MAX_SPOT_LIGHTS = 256;
  static constexpr uint32_t LIGHT_TILE_SIZE = 16;
  static constexpr uint32_t MAX_POINT_LIGHTS_PER_TILE = 128;
  static constexpr uint32_t MAX_SPOT_LIGHTS_PER_TILE = 64;

  // References
  DeviceContext *m_Context = nullptr;
  ResourceManager *m_ResourceManager = nullptr;
  GUISystem *m_GUISystem = nullptr;

  // Pipelines
  Pipeline m_ForwardPipeline{};
  Pipeline m_GBufferPipeline{};
  Pipeline m_GBufferDebugPipeline{};
  Pipeline m_DeferredLightingPipeline{};
  ComputePipeline m_TiledLightCullingPipeline{};
  Pipeline m_ToneMappingPipeline{};
  Pipeline m_GridPipeline{};
  Pipeline m_OutlinePipeline{};
  Pipeline m_DebugLinePipeline{};

  // Frame Data
  std::array<FrameData, MAX_FRAMES_IN_FLIGHT> m_Frames;

  // Texture Descriptor
  VkDescriptorPool m_TextureDescriptorPool = VK_NULL_HANDLE;
  VkDescriptorSetLayout m_TextureDescriptorSetLayout = VK_NULL_HANDLE;

  // GBuffer Descriptor
  VkDescriptorPool m_GBufferDescriptorPool = VK_NULL_HANDLE;
  VkDescriptorSetLayout m_GBufferDescriptorSetLayout = VK_NULL_HANDLE;
  std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT>
      m_GBufferDescriptorSets{};
  VkSampler m_GBufferSampler = VK_NULL_HANDLE;

  // Tone Mapping Descriptor
  VkDescriptorPool m_ToneMappingDescriptorPool = VK_NULL_HANDLE;
  VkDescriptorSetLayout m_ToneMappingDescriptorSetLayout = VK_NULL_HANDLE;
  std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT>
      m_ToneMappingDescriptorSets{};
  VkSampler m_ToneMappingSampler = VK_NULL_HANDLE;

  // Outline Descriptor
  VkDescriptorPool m_OutlineDescriptorPool = VK_NULL_HANDLE;
  VkDescriptorSetLayout m_OutlineDescriptorSetLayout = VK_NULL_HANDLE;
  std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> m_OutlineDescriptorSets{};
  VkSampler m_OutlineSampler = VK_NULL_HANDLE;

  std::vector<VkSemaphore> m_RenderFinishedSemaphores;
  uint32_t m_FrameIndex = 0;
  uint32_t m_ImageIndex = 0;

  bool m_Resized = false;

  uint32_t m_LightTileCountX = 0;
  uint32_t m_LightTileCountY = 0;

  RenderCamera m_ActiveCamera{};
};
} // namespace Inferno
