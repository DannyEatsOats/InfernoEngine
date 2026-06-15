#include <pch.h>
#include <ranges>
#include <stdexcept>
#include <utility>
#include <vector>
#include <vulkan/vulkan_core.h>

#include "Inferno/Renderer/Image.h"
#include "RenderGraph.h"

namespace Inferno {
static VkImageAspectFlags GetAspectMaskFromFormat(VkFormat format) {
  switch (format) {
  case VK_FORMAT_D16_UNORM:
  case VK_FORMAT_X8_D24_UNORM_PACK32:
  case VK_FORMAT_D32_SFLOAT:
    return VK_IMAGE_ASPECT_DEPTH_BIT;
  case VK_FORMAT_S8_UINT:
    return VK_IMAGE_ASPECT_STENCIL_BIT;
  case VK_FORMAT_D16_UNORM_S8_UINT:
  case VK_FORMAT_D24_UNORM_S8_UINT:
  case VK_FORMAT_D32_SFLOAT_S8_UINT:
    return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
  default:
    return VK_IMAGE_ASPECT_COLOR_BIT;
  }
}

RenderGraph::~RenderGraph() {
  if (!m_CommandBuffers.empty()) {
    vkFreeCommandBuffers(m_Context->Device, m_Context->CommandPool,
                         static_cast<uint32_t>(m_CommandBuffers.size()),
                         m_CommandBuffers.data());
  }

  for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    vkDestroySemaphore(m_Context->Device, m_ImagesAvailableSemaphores[i],
                       nullptr);
    vkDestroyFence(m_Context->Device, m_InFlightFences[i], nullptr);
  }

  for (VkSemaphore semaphore : m_RenderFinishedSemaphores) {
    vkDestroySemaphore(m_Context->Device, semaphore, nullptr);
  }
}

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
  resource.IsExternal = false;

  m_Resources.emplace(name, std::move(resource));
}

void RenderGraph::ImportSwapchainResources(
    const std::string &name, const std::vector<VkImage> &swapchainImages,
    const std::vector<VkImageView> &swapchainImageViews, VkFormat format,
    VkExtent2D extent, VkImageUsageFlags usage, VkImageLayout finalLayout) {
  Resource resource{};
  resource.Name = name;
  resource.Format = format;
  resource.Extent = extent;
  resource.Usage = usage;
  resource.InitialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  resource.FinalLayout = finalLayout;
  resource.ExternalImages = swapchainImages;
  resource.ExternalImageViews = swapchainImageViews;
  resource.IsExternal = true;

  m_Resources.emplace(name, std::move(resource));
}

void RenderGraph::UpdateSwapchainResources(
    const std::string &name, const std::vector<VkImage> &swapchainImages,
    const std::vector<VkImageView> &swapchainImageViews, VkExtent2D newExtent) {

  auto it = m_Resources.find(name);
  if (it != m_Resources.end() && it->second.IsExternal) {
    it->second.ExternalImages = swapchainImages;
    it->second.ExternalImageViews = swapchainImageViews;
    it->second.Extent = newExtent;

    for (VkSemaphore semaphore : m_RenderFinishedSemaphores) {
      vkDestroySemaphore(m_Context->Device, semaphore, nullptr);
    }
    m_RenderFinishedSemaphores.clear();

    m_RenderFinishedSemaphores.resize(swapchainImages.size());
    VkSemaphoreCreateInfo semaphoreInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    for (size_t i = 0; i < swapchainImages.size(); ++i) {
      if (vkCreateSemaphore(m_Context->Device, &semaphoreInfo, nullptr,
                            &m_RenderFinishedSemaphores[i]) != VK_SUCCESS) {
        throw std::runtime_error(
            "Failed to recreate RenderGraph render finished semaphores!");
      }
    }
  }
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

VkImageView RenderGraph::GetImageView(const std::string &name,
                                      uint32_t frameIndex) {
  auto it = m_Resources.find(name);
  if (it == m_Resources.end()) {
    throw std::runtime_error("RenderGraph resource not found: " + name);
  }

  const auto &resource = it->second;
  if (resource.IsExternal) {
    return resource.ExternalImageViews[m_ActiveImageIndex];
  } else {
    return resource.FrameImages[frameIndex].GetView();
  }
}

void RenderGraph::Compile() {
  // Topological Sorting ==================================================
  m_ExecutionOrder.clear();
  std::vector<std::vector<size_t>> dependencies(m_Passes.size());
  std::unordered_map<std::string, size_t> resourceWriters;

  for (size_t i = 0; i < m_Passes.size(); ++i) {
    const auto &pass = m_Passes[i];

    for (const auto &input : pass.Inputs) {
      auto it = resourceWriters.find(input);
      if (it != resourceWriters.end()) {
        dependencies[i].push_back(it->second);
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
      throw std::runtime_error("Cycle detected in RenderGraph!");
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
  // ==============================================================

  m_CommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
  m_ImagesAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
  m_InFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

  uint32_t swapchainImageCount = MAX_FRAMES_IN_FLIGHT;
  for (const auto &[name, resource] : m_Resources) {
    if (resource.IsExternal) {
      swapchainImageCount =
          static_cast<uint32_t>(resource.ExternalImages.size());
      break;
    }
  }
  m_RenderFinishedSemaphores.resize(swapchainImageCount);

  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.commandPool = m_Context->CommandPool;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;

  if (vkAllocateCommandBuffers(m_Context->Device, &allocInfo,
                               m_CommandBuffers.data()) != VK_SUCCESS) {
    throw std::runtime_error("Failed to allocate RenderGraph command buffers");
  }

  VkSemaphoreCreateInfo semaphoreInfo{};
  semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  VkFenceCreateInfo fenceInfo{};
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    if (vkCreateSemaphore(m_Context->Device, &semaphoreInfo, nullptr,
                          &m_ImagesAvailableSemaphores[i]) != VK_SUCCESS ||
        vkCreateFence(m_Context->Device, &fenceInfo, nullptr,
                      &m_InFlightFences[i]) != VK_SUCCESS) {
      throw std::runtime_error(
          "Failed to create RenderGraph synchronization objects!");
    }
  }

  for (uint32_t i = 0; i < swapchainImageCount; ++i) {
    if (vkCreateSemaphore(m_Context->Device, &semaphoreInfo, nullptr,
                          &m_RenderFinishedSemaphores[i]) != VK_SUCCESS) {
      throw std::runtime_error(
          "Failed to create RenderGraph render finished semaphores!");
    }
  }

  for (auto &[name, resource] : m_Resources) {
    if (resource.IsExternal) {
      continue;
    }

    ImageSpec imageSpec{};
    imageSpec.Format = resource.Format;
    imageSpec.Width = resource.Extent.width;
    imageSpec.Height = resource.Extent.height;
    imageSpec.Tiling = VK_IMAGE_TILING_OPTIMAL;
    imageSpec.Usage = resource.Usage;
    imageSpec.Aspect = GetAspectMaskFromFormat(resource.Format);

    resource.FrameImages.reserve(MAX_FRAMES_IN_FLIGHT);
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
      resource.FrameImages.emplace_back(m_Context, imageSpec);
    }
  }
}

void RenderGraph::Execute(uint32_t imageIndex) {
  m_ActiveImageIndex = imageIndex;
  VkCommandBuffer commandBuffer = m_CommandBuffers[m_CurrentFrame];

  std::unordered_map<std::string, VkImageLayout> currentLayouts;
  for (const auto &[name, resource] : m_Resources) {
    currentLayouts[name] = resource.InitialLayout;
  }

  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
    throw std::runtime_error("Failed to begin RenderGraph command buffer!");
  }

  for (auto passIdx : m_ExecutionOrder) {
    const auto &pass = m_Passes[passIdx];

    for (const auto &input : pass.Inputs) {
      auto &resource = m_Resources[input];
      VkImageLayout oldLayout = currentLayouts[input];
      VkImageLayout newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

      VkImageAspectFlags aspectMask = GetAspectMaskFromFormat(resource.Format);

      VkImageMemoryBarrier barrier{};
      barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
      barrier.oldLayout = oldLayout;
      barrier.newLayout = newLayout;
      barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.image = resource.GetVkImage(m_CurrentFrame, imageIndex);
      barrier.subresourceRange = {aspectMask, 0, VK_REMAINING_MIP_LEVELS, 0,
                                  VK_REMAINING_ARRAY_LAYERS};
      // barrier.subresourceRange = {aspectMask, 0, 1, 0, 1};
      barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
      barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

      vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                           VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                           VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 0, nullptr,
                           1, &barrier);

      currentLayouts[input] = newLayout;
    }

    for (const auto &output : pass.Outputs) {
      auto &resource = m_Resources[output];
      VkImageLayout oldLayout = currentLayouts[output];

      VkImageLayout newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
      VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      VkAccessFlags dstAccess = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
      VkPipelineStageFlags dstStage =
          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

      if (resource.Usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
        newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        dstAccess = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dstStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                   VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
      }

      VkImageMemoryBarrier barrier{};
      barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
      barrier.oldLayout = oldLayout;
      barrier.newLayout = newLayout;
      barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.image = resource.GetVkImage(m_CurrentFrame, imageIndex);
      barrier.subresourceRange = {aspectMask, 0, 1, 0, 1};
      barrier.srcAccessMask =
          VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
      barrier.dstAccessMask = dstAccess;

      vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                           dstStage, VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 0,
                           nullptr, 1, &barrier);

      currentLayouts[output] = newLayout;
    }

    pass.ExecuteFunc(commandBuffer);

    for (const auto &output : pass.Outputs) {
      auto &resource = m_Resources[output];
      VkImageLayout oldLayout = currentLayouts[output];
      VkImageLayout newLayout = resource.FinalLayout;

      VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      VkPipelineStageFlags srcStage =
          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
      VkAccessFlags srcAccess = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

      if (resource.Usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
        aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        srcStage = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        srcAccess = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
      }

      VkImageMemoryBarrier barrier{};
      barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
      barrier.oldLayout = oldLayout;
      barrier.newLayout = newLayout;
      barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.image = resource.GetVkImage(m_CurrentFrame, imageIndex);
      barrier.subresourceRange = {aspectMask, 0, 1, 0, 1};
      barrier.srcAccessMask = srcAccess;
      barrier.dstAccessMask = (newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
                                  ? 0
                                  : VK_ACCESS_MEMORY_READ_BIT;

      vkCmdPipelineBarrier(
          commandBuffer, srcStage, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
          VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 0, nullptr, 1, &barrier);

      currentLayouts[output] = newLayout;
    }
  }

  if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
    throw std::runtime_error("Failed to end RenderGraph command buffer");
  }
}

bool RenderGraph::RenderFrame(VkSwapchainKHR swapchain, VkQueue graphicsQueue,
                              VkQueue presentQueue) {
  vkWaitForFences(m_Context->Device, 1, &m_InFlightFences[m_CurrentFrame],
                  VK_TRUE, UINT64_MAX);

  uint32_t imageIndex;
  VkResult result = vkAcquireNextImageKHR(
      m_Context->Device, swapchain, UINT64_MAX,
      m_ImagesAvailableSemaphores[m_CurrentFrame], VK_NULL_HANDLE, &imageIndex);

  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
    return false;
  } else if (result != VK_SUCCESS) {
    throw std::runtime_error("Failed to acquire swapchain image!");
  }

  vkResetFences(m_Context->Device, 1, &m_InFlightFences[m_CurrentFrame]);

  Execute(imageIndex);

  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

  VkSemaphore waitSemaphores[] = {m_ImagesAvailableSemaphores[m_CurrentFrame]};
  VkPipelineStageFlags waitStages[] = {
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pWaitSemaphores = waitSemaphores;
  submitInfo.pWaitDstStageMask = waitStages;

  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &m_CommandBuffers[m_CurrentFrame];

  VkSemaphore signalSemaphores[] = {m_RenderFinishedSemaphores[imageIndex]};
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores = signalSemaphores;

  if (vkQueueSubmit(graphicsQueue, 1, &submitInfo,
                    m_InFlightFences[m_CurrentFrame]) != VK_SUCCESS) {
    throw std::runtime_error(
        "Failed to submit RenderGraph to execution queue!");
  }

  VkPresentInfoKHR presentInfo{};
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores = signalSemaphores;

  VkSwapchainKHR swapchains[] = {swapchain};
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = swapchains;
  presentInfo.pImageIndices = &imageIndex;

  result = vkQueuePresentKHR(presentQueue, &presentInfo);
  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
    return false;
  } else if (result != VK_SUCCESS) {
    throw std::runtime_error("Failed to present rendered swapchain image!");
  }

  m_CurrentFrame = (m_CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
  return true;
}

void RenderGraph::Resize(VkExtent2D newExtent) {
  for (auto &[name, resource] : m_Resources) {
    if (resource.IsExternal) {
      continue;
    }

    resource.Extent = newExtent;

    resource.FrameImages.clear();

    ImageSpec imageSpec{};
    imageSpec.Format = resource.Format;
    imageSpec.Width = resource.Extent.width;
    imageSpec.Height = resource.Extent.height;
    imageSpec.Tiling = VK_IMAGE_TILING_OPTIMAL;
    imageSpec.Usage = resource.Usage;
    imageSpec.Aspect = GetAspectMaskFromFormat(resource.Format);

    resource.FrameImages.reserve(MAX_FRAMES_IN_FLIGHT);
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
      resource.FrameImages.emplace_back(m_Context, imageSpec);
    }
  }
}
} // namespace Inferno
