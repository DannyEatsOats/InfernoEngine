#pragma once

#include "Inferno/Renderer/CullingSystem.h"
#include "Inferno/Renderer/DeviceContext.h"
#include "Inferno/Renderer/RenderGraph.h"
#include "Inferno/Resource/ResourceManager.h"
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

private:
  void SetupDeferredPipeline();
  void CreateTestPipeline();

private:
  DeviceContext *m_Context = nullptr;

  CullingSystem *m_CullingSystem = nullptr;
  RenderGraph *m_RenderGraph = nullptr;

  // TEST
  ResourceManager *m_ResourceManager = nullptr;

  VkPipeline m_Pipeline;
  VkPipelineLayout m_PipelineLayout;

  std::vector<Entity *> m_VisibleEntites;
};
} // namespace Inferno
