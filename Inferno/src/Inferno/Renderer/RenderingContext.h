#pragma once

#include <memory>
#include <optional>
#include <vector>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

struct GLFWwindow;

struct QueueFamilyIndices {
  std::optional<uint32_t> graphicsFamily;
  std::optional<uint32_t> presentFamily;

  bool IsComplete() {
    return graphicsFamily.has_value() && presentFamily.has_value();
  }
};

struct SwapChainSupportDetails {
  VkSurfaceCapabilitiesKHR capabilities;
  std::vector<VkSurfaceFormatKHR> formats;
  std::vector<VkPresentModeKHR> presentModes;
};

namespace Inferno {
class RenderingContext {
public:
  RenderingContext(GLFWwindow *window);
  ~RenderingContext();

  void Init();
  void ShutDown();

  void OnResize(uint32_t width, uint32_t height);

  static std::shared_ptr<RenderingContext> &CreateContext(GLFWwindow *window);
  static std::shared_ptr<RenderingContext> &GetContext();

private:
  void CreateInstance();
  void CreateSurface();
  void PickPhysicalDevice();
  void CreateLogicalDevice();
  void CreateSwapChain();
  void CleanUpSwapChain();
  void CreateImageViews();

  QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);
  SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device);
  VkSurfaceFormatKHR ChooseSwapSurfaceFormat(
      const std::vector<VkSurfaceFormatKHR> &availableFormats);
  VkPresentModeKHR ChooseSwapPresentMode(
      const std::vector<VkPresentModeKHR> &availablePresentationModes);
  VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities);
  bool IsDeviceSuitable(VkPhysicalDevice device);
  bool CheckDeviceExtensionSupport(VkPhysicalDevice device);

private:
  GLFWwindow *m_Window = nullptr;

  VkInstance m_Instance = VK_NULL_HANDLE;
  VkSurfaceKHR m_Surface = VK_NULL_HANDLE;

  VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
  VkDevice m_Device = VK_NULL_HANDLE;

  VkQueue m_GrapicsQueue = VK_NULL_HANDLE;
  VkQueue m_PresentQueue = VK_NULL_HANDLE;

  VkSwapchainKHR m_Swapchain = VK_NULL_HANDLE;
  std::vector<VkImage> m_SwapChainImages;
  VkFormat m_SwapChainImageFormat;
  VkExtent2D m_SwapChainExtent;
  std::vector<VkImageView> m_SwapChainImageViews;

  static std::shared_ptr<RenderingContext> s_Context;

  friend class Renderer;
};
} // namespace Inferno
