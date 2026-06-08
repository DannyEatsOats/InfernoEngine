#pragma once

#include "Inferno/Renderer/CullingSystem.h"
#include "Inferno/Renderer/DeviceContext.h"
#include "Inferno/Renderer/RenderGraph.h"
#include "Inferno/Renderer/Shader.h"
#include "Inferno/Resource/ResourceManager.h"
#include <vulkan/vulkan_core.h>

namespace Inferno {
class Renderer {
public:
  Renderer(const DeviceContext *context) : m_Context(context) {}
  ~Renderer() = default;

  Renderer(const Renderer &) = delete;
  Renderer(Renderer &&) = delete;
  Renderer &operator=(const Renderer &) = delete;
  Renderer &operator=(Renderer &&) = delete;

  void StartUp();
  void ShutDown();

  void Render(const std::vector<Entity *> &entities);

private:
  void CreateSurfaceAndSwapchain();
  void SetupDeferredPipeline();
  void CleanUpPresentation();

  void CreateTestPipeline();

private:
  const DeviceContext *m_Context = nullptr;

  VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
  VkSwapchainKHR m_Swapchain = VK_NULL_HANDLE;
  VkQueue m_PresentQueue = VK_NULL_HANDLE;
  std::vector<VkImage> m_SwapchainImages;
  std::vector<VkImageView> m_SwapchainImageViews;
  VkFormat m_SwapchainFormat;
  VkExtent2D m_SwapchainExtent;

  CullingSystem *m_CullingSystem = nullptr;
  RenderGraph *m_RenderGraph = nullptr;

  //TEST
  ResourceManager* m_ResourceManager;
  
  VkPipeline m_Pipeline;
  VkPipelineLayout m_PipelineLayout;

  Shader* testShader;
};
} // namespace Inferno
