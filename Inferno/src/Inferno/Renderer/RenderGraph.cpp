#include <pch.h>
#include <utility>

#include "RenderGraph.h"

namespace Inferno {
void RenderGraph::AddResource(const std::string &name, VkFormat format,
                              VkExtent2D extent, VkImageUsageFlags usage,
                              VkImageLayout initialLayout,
                              VkImageLayout finalLayout) {
  Resource resource;
  resource.Name = name;
  resource.Format = format;
  resource.Extent = extent;
  resource.Usage = usage;
  resource.InitialLayout = initialLayout;
  resource.FinalLayout = finalLayout;

  m_Resources.emplace(name, std::move(resource));
}

void RenderGraph::AddPass(const std::string &name,
                          const std::vector<std::string> &inputs,
                          const std::vector<std::string> &outputs,
                          std::function<void(VkCommandBuffer &)> executeFunc) {
  Pass pass;
  pass.Name = name;
  pass.Inputs = inputs;
  pass.Outputs = outputs;
  pass.ExecuteFunc = executeFunc;

  m_Passes.emplace_back(std::move(pass));
}

void RenderGraph::Compile() {}
} // namespace Inferno
