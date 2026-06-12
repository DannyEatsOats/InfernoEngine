#pragma once

#include "Inferno/Renderer/DeviceContext.h"
#include <cstdint>
#include <vulkan/vulkan_core.h>
namespace Inferno {
struct ImageSpec {
  uint32_t Width = 0;
  uint32_t Height = 0;
  uint32_t MipLevels = 1;
  VkFormat Format = VK_FORMAT_R8G8B8A8_SRGB;
  VkImageTiling Tiling = VK_IMAGE_TILING_OPTIMAL;
  VkImageUsageFlags Usage =
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  VkMemoryPropertyFlags Properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
  VkImageAspectFlags Aspect = VK_IMAGE_ASPECT_COLOR_BIT;
};

class Image {
public:
  Image(const DeviceContext *context, const ImageSpec &spec);
  Image() = default;
  ~Image();

  Image(const Image &) = delete;
  Image &operator=(const Image &) = delete;

  Image(Image &&other);
  Image &operator=(Image &&other);

  VkImage GetImage() const { return m_Image; }
  VkImageView GetView() const { return m_ImageView; }

  uint32_t GetWidth() const { return m_Width; }
  uint32_t GetHeight() const { return m_Height; }

  uint32_t GetMipLevels() const { return m_MipLevels; }
  VkFormat GetFormat() const { return m_Format; }

private:
  void CleanUp();

private:
  const DeviceContext *m_Context = nullptr;

  VkImage m_Image = VK_NULL_HANDLE;
  VkDeviceMemory m_Memory = VK_NULL_HANDLE;

  VkDeviceSize m_Offset = 0;
  VkImageView m_ImageView = VK_NULL_HANDLE;

  uint32_t m_Width = 0;
  uint32_t m_Height = 0;
  uint32_t m_MipLevels = 1;
  VkFormat m_Format = VK_FORMAT_UNDEFINED;
};
} // namespace Inferno
