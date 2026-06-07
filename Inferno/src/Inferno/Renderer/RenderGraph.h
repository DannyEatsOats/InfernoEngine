#pragma once

#include "Inferno/Renderer/DeviceContext.h"
#include "Inferno/Renderer/Image.h"
#include <string>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace Inferno {
class RenderGraph {
public:
  explicit RenderGraph(const DeviceContext *context) : m_Context(context) {}

  RenderGraph(const RenderGraph &) = delete;
  RenderGraph(RenderGraph &&) = delete;

  // TODO: Implement these
  RenderGraph &operator=(const RenderGraph &);
  RenderGraph &operator=(RenderGraph &&);

  void AddResource(const std::string &name, VkFormat format, VkExtent2D extent,
                   VkImageUsageFlags usage, VkImageLayout initialLayout,
                   VkImageLayout finalLayout);

  void AddPass(const std::string &name, const std::vector<std::string> &inputs,
               const std::vector<std::string> &outputs,
               std::function<void(VkCommandBuffer &)> executeFunc);

  void Compile();

private:
  // Internal structs
  struct Resource {
    std::string Name;
    VkFormat Format;
    VkExtent2D Extent;
    VkImageUsageFlags Usage;
    VkImageLayout InitialLayout;
    VkImageLayout FinalLayout;

    Image Image;
  };

  struct Pass {
    std::string Name;
    std::vector<std::string> Inputs;
    std::vector<std::string> Outputs;
    std::function<void(VkCommandBuffer &)> ExecuteFunc;
  };
  //=============================================
private:
  const DeviceContext *m_Context = nullptr;

  std::unordered_map<std::string, Resource> m_Resources;
  std::vector<Pass> m_Passes;
  std::vector<size_t> m_ExecutionOrder;

  std::vector<VkSemaphore> m_Semaphores;
  std::vector<std::pair<size_t, size_t>> m_SemaphoreSignalWaitPairs;
};
} // namespace Inferno
