#pragma once

#include <cstdint>
#include <vulkan/vulkan_core.h>

#include "Inferno/Resource/Resource.h"

namespace Inferno {
class Texture : public Resource {
public:
  explicit Texture(const std::string &id, VkDevice device)
      : Resource(id), m_Device(device) {}
  ~Texture() = default;

  Texture(Texture &) = delete;
  Texture &operator=(Texture &) = delete;

  Texture(Texture &&other);
  Texture &operator=(Texture &&other);

  VkImage GetImage() const { return m_Image; }
  VkImageView GetImageView() const { return m_ImageView; }
  VkSampler GetSampler() const { return m_Sampler; }

protected:
  bool DoLoad() override;
  bool DoUnLoad() override;

private:
  void CleanUp();

  unsigned char *LoadImageData(const std::string &filePath, int *width,
                               int *height, int *channels);
  void FreeImage(unsigned char *data);
  void CreateVulkanImage(unsigned char *data, uint32_t width, uint32_t height,
                         uint32_t channels);

private:
  VkDevice m_Device = VK_NULL_HANDLE;

  VkImage m_Image = VK_NULL_HANDLE;
  VkDeviceMemory m_Memory = VK_NULL_HANDLE;
  VkDeviceSize m_Offset = 0;
  VkImageView m_ImageView = VK_NULL_HANDLE;
  VkSampler m_Sampler = VK_NULL_HANDLE;

  uint32_t m_Width = 0;
  uint32_t m_Height = 0;
  uint32_t m_Channels = 0;
};
} // namespace Inferno
