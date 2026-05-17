#include <ktx.h>
#include <pch.h>
#include <stdexcept>
#include <vulkan/vulkan_core.h>

#include "Inferno/Resource/Resource.h"
#include "Texture.h"
#include "ktxvulkan.h"

namespace Inferno {
Texture::Texture(Texture &&other)
    : Resource(std::move(other)), m_Context(other.m_Context),
      m_Image(other.m_Image), m_Memory(other.m_Memory),
      m_Offset(other.m_Offset), m_ImageView(other.m_ImageView),
      m_Sampler(other.m_Sampler), m_Width(other.m_Width),
      m_Height(other.m_Height), m_MipLevels(other.m_MipLevels),
      m_Format(other.m_Format) {
  other.m_Context = VK_NULL_HANDLE;

  other.m_Image = VK_NULL_HANDLE;
  other.m_Memory = VK_NULL_HANDLE;
  other.m_Offset = 0;
  other.m_ImageView = VK_NULL_HANDLE;
  other.m_Sampler = VK_NULL_HANDLE;
  other.m_Width = 0;
  other.m_Height = 0;
  other.m_MipLevels = 0;
  other.m_Format = VK_FORMAT_UNDEFINED;
}

Texture &Texture::operator=(Texture &&other) {
  if (this == &other) {
    return *this;
  }

  CleanUp();

  m_Context = other.m_Context;

  m_Image = other.m_Image;
  m_Memory = other.m_Memory;
  m_Offset = other.m_Offset;
  m_ImageView = other.m_ImageView;
  m_Sampler = other.m_Sampler;

  m_Width = other.m_Width;
  m_Height = other.m_Height;
  m_MipLevels = other.m_MipLevels;
  m_Format = other.m_Format;

  other.m_Context = VK_NULL_HANDLE;

  other.m_Image = VK_NULL_HANDLE;
  other.m_Memory = VK_NULL_HANDLE;
  other.m_Offset = 0;
  other.m_ImageView = VK_NULL_HANDLE;
  other.m_Sampler = VK_NULL_HANDLE;
  other.m_Width = 0;
  other.m_Height = 0;
  other.m_MipLevels = 0;
  other.m_Format = VK_FORMAT_UNDEFINED;

  return *this;
}

bool Texture::DoLoad() {
  // std::string filePath = "textures/" + GetID() + ".ktx";
  std::string filePath = "textures/" + GetID() + ".ktx";

  LoadFromKTX2(filePath);

  // CreateVulkanImage(data, m_Width, m_Height, m_Channels);
  // FreeImage(data);

  return true;
}

void Texture::LoadFromKTX2(const std::string &filePath) {
  ktxTexture2 *kTexture = nullptr;

  KTX_error_code result = ktxTexture2_CreateFromNamedFile(
      filePath.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &kTexture);

  if (result != KTX_SUCCESS || !kTexture)
    throw std::runtime_error("Failed to load KTX2: " + filePath);

  m_Width = kTexture->baseWidth;
  m_Height = kTexture->baseHeight;
  m_MipLevels = kTexture->numLevels;
  m_Format = static_cast<VkFormat>(kTexture->vkFormat);

  // ---- Vulkan upload (modern KTX path) ----
  ktxVulkanDeviceInfo deviceInfo;
  ktxVulkanTexture texture;

  ktxVulkanDeviceInfo_Construct(&deviceInfo, m_Context->PhysicalDevice,
                                m_Context->Device, m_Context->GraphicsQueue,
                                m_Context->CommandPool, nullptr);

  KTX_error_code uploadResult = ktxTexture_VkUploadEx(
      reinterpret_cast<ktxTexture *>(kTexture), &deviceInfo, &texture,
      VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  if (uploadResult != KTX_SUCCESS) {
    ktxTexture_Destroy(reinterpret_cast<ktxTexture *>(kTexture));
    throw std::runtime_error("KTX Vulkan upload failed: " + filePath);
  }

  VkImageViewCreateInfo viewInfo{};
  viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image = texture.image;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewInfo.format = static_cast<VkFormat>(kTexture->vkFormat);

  viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  viewInfo.subresourceRange.baseMipLevel = 0;
  viewInfo.subresourceRange.levelCount = kTexture->numLevels;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount = 1;

  VkImageView imageView;
  vkCreateImageView(m_Context->Device, &viewInfo, nullptr, &imageView);

  // Store GPU objects
  m_Image = texture.image;
  m_Memory = texture.deviceMemory;
  m_ImageView = imageView;

  // Cleanup CPU KTX immediately (IMPORTANT)
  ktxTexture_Destroy(reinterpret_cast<ktxTexture *>(kTexture));
}

bool Texture::DoUnLoad() {
  if (!IsLoaded()) {
    return false;
  }

  CleanUp();

  return true;
}

void Texture::CleanUp() {
  vkDestroySampler(m_Context->Device, m_Sampler, nullptr);
  vkDestroyImageView(m_Context->Device, m_ImageView, nullptr);
  vkDestroyImage(m_Context->Device, m_Image, nullptr);
  vkFreeMemory(m_Context->Device, m_Memory, nullptr);
}

} // namespace Inferno
