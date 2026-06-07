#include <pch.h>
#include <stdexcept>
#include <utility>
#include <vector>

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

void RenderGraph::Compile() {
  std::vector<std::vector<size_t>> dependencies(m_Passes.size());
  std::vector<std::vector<size_t>> dependents(m_Passes.size());

  std::unordered_map<std::string, size_t> resourceWriters;

  for (size_t i = 0; i < m_Passes.size(); ++i) {
    const auto &pass = m_Passes[i];

    for (const auto &input : pass.Inputs) {
      auto it = resourceWriters.find(input);
      if (it != resourceWriters.end()) {
        dependencies[i].push_back(it->second);
        dependents[it->second].push_back(i);
      }
    }

    for (const auto &output : pass.Outputs) {
      resourceWriters[output] = i;
    }
  }

  std::vector<bool> visited(m_Passes.size(), false);
  std::vector<bool> inStack(m_Passes.size(), false);

  std::function<void(size_t)> visit = [&](size_t node) {
    if (inStack[node]) {
      throw std::runtime_error("Cycle detected in RenderGrpah!");
    }

    if (visited[node]) {
      return;
    }

    inStack[node] = true;

    for (auto dependent : dependents[node]) {
      visit(dependent);
    }

    inStack[node] = false;
    visited[node] = true;
    m_ExecutionOrder.push_back(node);
  };

  for (size_t i = 0; i < m_Passes.size(); ++i) {
    if (!visited[i]) {
      visit(i);
    }
  }
}
} // namespace Inferno
