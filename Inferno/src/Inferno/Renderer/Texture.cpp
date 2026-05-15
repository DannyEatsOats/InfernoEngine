#include <pch.h>
#include <vulkan/vulkan_core.h>

#include "Inferno/Resource/Resource.h"
#include "Texture.h"

namespace Inferno {
Texture::Texture(Texture &&other)
    : Resource(std::move(other)), m_Device(other.m_Device),
      m_Image(other.m_Image), m_Memory(other.m_Memory),
      m_Offset(other.m_Offset), m_ImageView(other.m_ImageView),
      m_Sampler(other.m_Sampler), m_Width(other.m_Width),
      m_Height(other.m_Height), m_Channels(other.m_Channels) {
  other.m_Device = VK_NULL_HANDLE;

  other.m_Image = VK_NULL_HANDLE;
  other.m_Memory = VK_NULL_HANDLE;
  other.m_Offset = 0;
  other.m_ImageView = VK_NULL_HANDLE;
  other.m_Sampler = VK_NULL_HANDLE;
}

Texture &Texture::operator=(Texture &&other) {
  if (this == &other) {
    return *this;
  }

  CleanUp();

  m_Device = other.m_Device;

  m_Image = other.m_Image;
  m_Memory = other.m_Memory;
  m_Offset = other.m_Offset;
  m_ImageView = other.m_ImageView;
  m_Sampler = other.m_Sampler;

  m_Width = other.m_Width;
  m_Height = other.m_Height;
  m_Channels = other.m_Channels;

  other.m_Device = VK_NULL_HANDLE;

  other.m_Image = VK_NULL_HANDLE;
  other.m_Memory = VK_NULL_HANDLE;
  other.m_Offset = 0;
  other.m_ImageView = VK_NULL_HANDLE;
  other.m_Sampler = VK_NULL_HANDLE;

  return *this;
}

bool Texture::DoLoad() {
  std::string filePath = "textures/" + GetID() + ".ktx";

  int width = 0, height = 0, channels = 0;
  unsigned char *data = LoadImageData(filePath, &width, &height, &channels);
  if (!data) {
    return false;
  }

  m_Width = static_cast<uint32_t>(width);
  m_Height = static_cast<uint32_t>(height);
  m_Channels = static_cast<uint32_t>(channels);

  CreateVulkanImage(data, m_Width, m_Height, m_Channels);
  FreeImage(data);

  return true;
}

bool Texture::DoUnLoad() {
  if (!IsLoaded()) {
    return false;
  }

  CleanUp();

  return true;
}

void Texture::CleanUp() {
  vkDestroySampler(m_Device, m_Sampler, nullptr);
  vkDestroyImageView(m_Device, m_ImageView, nullptr);
  vkDestroyImage(m_Device, m_Image, nullptr);
  vkFreeMemory(m_Device, m_Memory, nullptr);
}

} // namespace Inferno
