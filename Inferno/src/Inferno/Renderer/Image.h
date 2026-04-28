#pragma once

#include "Inferno/Renderer/RenderingContext.h"
#include <vulkan/vulkan_core.h>
namespace Inferno {
struct ImageSpec {
  uint32_t Height = 0;
  uint32_t Width = 0;
  VkFormat Format = VK_FORMAT_R8G8B8A8_SRGB;
  VkImageTiling Tiling = VK_IMAGE_TILING_OPTIMAL;
  VkImageUsageFlags Usage =
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  VkMemoryPropertyFlags Properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
};

class Image {
public:
  Image() = default;

  ~Image();

  Image(const Image &) = delete;
  Image &operator=(const Image &) = delete;

  Image(Image &&other) noexcept;

  Image &operator=(Image &&other) noexcept;

  VkImage Get() const { return m_Image; }
  VkDeviceMemory GetMemory() const { return m_Memory; }

  static Image Create(const RenderingContext *context,
                           const ImageSpec &spec);

private:
  Image(VkDevice device, VkImage image, VkDeviceMemory memory, VkFormat format)
      : m_Device(device), m_Image(image), m_Memory(memory), m_Format(format) {}

  void CleanUp();

private:
  VkDevice m_Device = VK_NULL_HANDLE;
  VkImage m_Image = VK_NULL_HANDLE;
  VkDeviceMemory m_Memory = VK_NULL_HANDLE;
  VkFormat m_Format = VK_FORMAT_R8G8B8A8_SRGB;
};
} // namespace Inferno
