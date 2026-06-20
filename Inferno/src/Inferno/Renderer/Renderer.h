#pragma once

#include "Inferno/Renderer/CullingSystem.h"
#include "Inferno/Renderer/DeviceContext.h"
#include "Inferno/Renderer/RenderGraph.h"
#include "Inferno/Renderer/Texture.h"
#include "Inferno/Resource/ResourceManager.h"
#include <unordered_map>
#include <vulkan/vulkan_core.h>

namespace Inferno {
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

  void SetLightingDebugMode(int mode) { m_LightinDebugMode = mode; }

private:
  void SetupDeferredRendering();
  void CreateGeometryPipeline();
  VkDescriptorSet GetOrCreateTextureDescriptorSet(const Texture *texture);
  void CreateLightingPipeline();
  void CreateLightingDescriptorSet();
  void UpdateLightingDescriptorSet(uint32_t frameIdx);

  void Resize();

  // TEST
  void CreateTestPipeline();

private:
  DeviceContext *m_Context = nullptr;

  CullingSystem *m_CullingSystem = nullptr;
  RenderGraph *m_RenderGraph = nullptr;
  ResourceManager *m_ResourceManager = nullptr;

  // G-Buffer Geometry Pass
  VkPipeline m_GeometryPipeline = VK_NULL_HANDLE;
  VkPipelineLayout m_GeometryPipelineLayout = VK_NULL_HANDLE;

  VkDescriptorSetLayout m_GeometryDescriptorSetLayout = VK_NULL_HANDLE;
  VkDescriptorPool m_TextureDescriptorPool = VK_NULL_HANDLE;

  // Deffered Shading Lighting Pass
  VkPipeline m_LightingPipeline = VK_NULL_HANDLE;
  VkPipelineLayout m_LightingPipelineLayout = VK_NULL_HANDLE;

  VkDescriptorSetLayout m_LightingDescriptorLayout = VK_NULL_HANDLE;
  VkDescriptorPool m_LightingDescriptorPool = VK_NULL_HANDLE;
  std::vector<VkDescriptorSet> m_LightingDescriptorSets;
  VkSampler m_GBufferSampler = VK_NULL_HANDLE;

  bool m_Resized = false;

  // TEST
  std::vector<Entity *> m_VisibleEntites;
  int m_LightinDebugMode = 0;
};
} // namespace Inferno
