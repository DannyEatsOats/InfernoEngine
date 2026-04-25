#pragma once

#include "Inferno/Window.h"
#include <cstdint>
#include <memory>
#include <vector>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

namespace Inferno {
class RenderingContext {
public:
  RenderingContext(Window *window);
  ~RenderingContext();

  void Init();
  void ShutDown();

  void OnResize(uint32_t width, uint32_t height);

  const VkPhysicalDevice &GetPhysicalDevice() const { return m_PhysicalDevice; }
  const VkDevice &GetDevice() const { return m_Device; }
  const VkSwapchainKHR &GetSwapChain() const { return m_SwapChain; }
  const VkFormat &GetSwapChainImgFormat() const {
    return m_SwapChainImageFormat;
  }
  const VkExtent2D &GetSwapChainExtent() const { return m_SwapChainExtent; }
  const std::vector<VkImageView> &GetSwapChainImageViews() const {
    return m_SwapChainImageViews;
  }
  const VkQueue &GetGraphicsQueue() const { return m_GraphicsQueue; }
  const VkQueue &GetPresentQueue() const { return m_PresentQueue; }

  const VkCommandPool &GetCommandPool() const { return m_CommandPool; }

  void CreateCommandPool();

  static std::shared_ptr<RenderingContext> &CreateContext(Window *window);
  static std::shared_ptr<RenderingContext> &GetContext();

private:
  void CreateInstance();
  void CreateSurface();
  void PickPhysicalDevice();
  void CreateLogicalDevice();
  void CreateSwapChain();
  void CleanUpSwapChain();
  void CreateImageViews();
  void RecreateSwapChain();

  bool IsDeviceSuitable(VkPhysicalDevice device);
  bool CheckDeviceExtensionSupport(VkPhysicalDevice device);

private:
  Window *m_Window = nullptr;

  VkInstance m_Instance = VK_NULL_HANDLE;
  VkSurfaceKHR m_Surface = VK_NULL_HANDLE;

  VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
  VkDevice m_Device = VK_NULL_HANDLE;

  VkQueue m_GraphicsQueue = VK_NULL_HANDLE;
  VkQueue m_PresentQueue = VK_NULL_HANDLE;

  VkSwapchainKHR m_SwapChain = VK_NULL_HANDLE;
  std::vector<VkImage> m_SwapChainImages;
  VkFormat m_SwapChainImageFormat;
  VkExtent2D m_SwapChainExtent;
  std::vector<VkImageView> m_SwapChainImageViews;

  VkCommandPool m_CommandPool;

  static std::shared_ptr<RenderingContext> s_Context;

  friend class Renderer;
};
} // namespace Inferno
