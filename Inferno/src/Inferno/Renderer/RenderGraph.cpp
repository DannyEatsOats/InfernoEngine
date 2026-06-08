#include <pch.h>
#include <stdexcept>
#include <utility>
#include <vector>
#include <vulkan/vulkan_core.h>

#include "Inferno/Renderer/Image.h"
#include "RenderGraph.h"

namespace Inferno {
void RenderGraph::AddResource(const std::string &name, VkFormat format,
                              VkExtent2D extent, VkImageUsageFlags usage,
                              VkImageLayout initialLayout,
                              VkImageLayout finalLayout) {
  Resource resource{};
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
  Pass pass{};
  pass.Name = name;
  pass.Inputs = inputs;
  pass.Outputs = outputs;
  pass.ExecuteFunc = executeFunc;

  m_Passes.emplace_back(std::move(pass));
}

void RenderGraph::Compile() {
  m_ExecutionOrder.clear();
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

    for (auto dependency : dependencies[node]) {
      visit(dependency);
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

  // creating sync objects
  for (size_t i = 0; i < m_Passes.size(); ++i) {
    for (auto dependency : dependencies[i]) {
      VkSemaphore semaphore{};
      vkCreateSemaphore(m_Context->Device, {}, nullptr, &semaphore);
      m_Semaphores.emplace_back(semaphore);
      m_SemaphoreSignalWaitPairs.emplace_back(dependency, i);
    }
  }

  for (auto &[name, resource] : m_Resources) {
    ImageSpec imageSpec{};
    imageSpec.Format = resource.Format;
    imageSpec.Width = resource.Extent.width;
    imageSpec.Height = resource.Extent.height;
    imageSpec.Tiling = VK_IMAGE_TILING_OPTIMAL;
    imageSpec.Usage = resource.Usage;

    resource.Image = Image(m_Context, imageSpec);
  }
}

void RenderGraph::Execute(VkCommandBuffer commandBuffer, VkQueue queue) {
  std::vector<VkCommandBuffer> cmdBuffer;
  std::vector<VkSemaphore> waitSemaphores;
  std::vector<VkPipelineStageFlags> waitStages;
  std::vector<VkSemaphore> signalSemaphores;

  for (auto passIdx : m_ExecutionOrder) {
    const auto &pass = m_Passes[passIdx];

    waitSemaphores.clear();
    waitStages.clear();

    for (size_t i = 0; i < m_SemaphoreSignalWaitPairs.size(); ++i) {
      if (m_SemaphoreSignalWaitPairs[i].second == passIdx) {
        waitSemaphores.push_back(m_Semaphores[i]);
        waitStages.push_back(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
      }
    }

    signalSemaphores.clear();
    for (size_t i = 0; i < m_SemaphoreSignalWaitPairs.size(); ++i) {
      if (m_SemaphoreSignalWaitPairs[i].first == passIdx) {
        signalSemaphores.push_back(m_Semaphores[i]);
      }
    }

    vkBeginCommandBuffer(commandBuffer, {});

    for (const auto &input : pass.Inputs) {
      auto &resource = m_Resources[input];

      VkImageMemoryBarrier barrier{};
      barrier.oldLayout = resource.InitialLayout;
      barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.image = resource.Image.GetImage();
      barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
      barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
      barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

      vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                           VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                           VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 0, nullptr,
                           1, &barrier);
    }

    for (const auto &output : pass.Outputs) {
      auto &resource = m_Resources[output];

      VkImageMemoryBarrier barrier{};
      barrier.oldLayout = resource.InitialLayout;
      barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
      barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.image = resource.Image.GetImage();
      barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
      barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
      barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

      vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                           VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                           VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 0, nullptr,
                           1, &barrier);
    }

    pass.ExecuteFunc(commandBuffer);

    for (const auto &output : pass.Outputs) {
      auto &resource = m_Resources[output];

      VkImageMemoryBarrier barrier{};
      barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
      barrier.newLayout = resource.FinalLayout;
      barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.image = resource.Image.GetImage();
      barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
      barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
      barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;

      vkCmdPipelineBarrier(
          commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
          VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0,
          nullptr, 0, nullptr, 1, &barrier);
    }

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.waitSemaphoreCount =
        static_cast<uint32_t>(waitSemaphores.size());
    submitInfo.pWaitSemaphores = waitSemaphores.data();
    submitInfo.pWaitDstStageMask = waitStages.data();
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    submitInfo.signalSemaphoreCount =
        static_cast<uint32_t>(signalSemaphores.size());
    submitInfo.pSignalSemaphores = signalSemaphores.data();

    vkQueueSubmit(queue, 1, &submitInfo, nullptr);
  }
}

} // namespace Inferno
