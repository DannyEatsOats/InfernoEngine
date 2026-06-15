#pragma once

#include "Inferno/Renderer/DeviceContext.h"
#include "Inferno/Renderer/Image.h"
#include <cstdint>
#include <string>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace Inferno {
class RenderGraph {
public:
  static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;
  static inline uint32_t GetMaxFramesInFlight() { return MAX_FRAMES_IN_FLIGHT; }

  // RenderGraph Specific Structs
  struct Resource {
    std::string Name;
    VkFormat Format;
    VkExtent2D Extent;
    VkImageUsageFlags Usage;
    VkImageLayout InitialLayout;
    VkImageLayout FinalLayout;
    VkSampleCountFlagBits Samples = VK_SAMPLE_COUNT_1_BIT;

    std::vector<Image> FrameImages;
    std::vector<VkImage> ExternalImages;
    std::vector<VkImageView> ExternalImageViews;
    bool IsExternal = false;

    VkImage GetVkImage(uint32_t frameIndex, uint32_t imageIndex) const {
      return IsExternal ? ExternalImages[imageIndex]
                        : FrameImages[frameIndex].GetImage();
    }
  };

  struct Pass {
    std::string Name;
    std::vector<std::string> Inputs;
    std::vector<std::string> Outputs;
    std::function<void(VkCommandBuffer &)> ExecuteFunc;
  };
  //=============================================

public:
  explicit RenderGraph(const DeviceContext *context) : m_Context(context) {}
  ~RenderGraph();

  RenderGraph(const RenderGraph &) = delete;
  RenderGraph(RenderGraph &&) = delete;

  RenderGraph &operator=(const RenderGraph &other) = delete;
  RenderGraph &operator=(RenderGraph &&other) = delete;

  const Resource *GetResource(std::string &name) const {
    auto it = m_Resources.find(name);
    return it != m_Resources.end() ? &it->second : nullptr;
  }

  void AddResource(const std::string &name, VkFormat format, VkExtent2D extent,
                   VkImageUsageFlags usage, VkImageLayout initialLayout,
                   VkImageLayout finalLayout);

  void ImportSwapchainResources(
      const std::string &name, const std::vector<VkImage> &swapchainImages,
      const std::vector<VkImageView> &swapchainImageViews, VkFormat format,
      VkExtent2D extent, VkImageUsageFlags usage, VkImageLayout finalLayout);

  void
  UpdateSwapchainResources(const std::string &name,
                           const std::vector<VkImage> &swapchainImages,
                           const std::vector<VkImageView> &swapchainImageViews,
                           VkExtent2D newExtent);

  void AddPass(const std::string &name, const std::vector<std::string> &inputs,
               const std::vector<std::string> &outputs,
               std::function<void(VkCommandBuffer &)> executeFunc);

  VkImageView GetImageView(const std::string &name, uint32_t frameIndex);

  uint32_t GetGetCurrentFrameIndex() const { return m_CurrentFrame; }

  void Compile();
  void Execute(uint32_t imageIndex);

  bool RenderFrame(VkSwapchainKHR swapchain, VkQueue graphicsQueue,
                   VkQueue presentQueue);

  void Resize(VkExtent2D newExtent);

private:
  const DeviceContext *m_Context = nullptr;

  std::unordered_map<std::string, Resource> m_Resources;
  std::vector<Pass> m_Passes;
  std::vector<size_t> m_ExecutionOrder;

  uint32_t m_CurrentFrame = 0;
  uint32_t m_ActiveImageIndex = 0;
  std::vector<VkCommandBuffer> m_CommandBuffers;
  std::vector<VkSemaphore> m_ImagesAvailableSemaphores;
  std::vector<VkSemaphore> m_RenderFinishedSemaphores;
  std::vector<VkFence> m_InFlightFences;
};
} // namespace Inferno
