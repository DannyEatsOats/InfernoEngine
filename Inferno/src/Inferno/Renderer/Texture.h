#pragma once

#include "Inferno/Renderer/Image.h"
#include "Inferno/Renderer/RenderingContext.h"
#include <vulkan/vulkan_core.h>
namespace Inferno {

enum class TextureType { Texture2D, TextureCube, Texture3D, RenderTarget };

class Texture {
public:
  static std::shared_ptr<Texture> Create2D(const RenderingContext *context,
                                           const std::string &filePath);

  ~Texture();

  uint32_t GetWidth() const { return m_Width; };
  uint32_t GetHeight() const { return m_Height; };
  uint32_t GetChannels() const { return m_Channels; };

  VkImageView GetImageView() const { return m_ImageView; };
  VkSampler GetSampler() const { return m_Sampler; };

  VkDescriptorImageInfo GetDescriptorInfo() const;

private:
  Texture(TextureType type, VkDevice device, VkPhysicalDevice physicalDevice);

  void LoadFromFile(const RenderingContext *context, const std::string &path);

  void CreateImage(const RenderingContext *context);
  void CreateSampler(const RenderingContext *context);

private:
  VkDevice m_Device = VK_NULL_HANDLE;
  VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;

  Image m_Image;
  VkImageView m_ImageView = VK_NULL_HANDLE;
  VkSampler m_Sampler = VK_NULL_HANDLE;

  VkFormat m_Format = VK_FORMAT_R8G8B8A8_SRGB;

  TextureType m_Type;

  uint32_t m_Width = 0;
  uint32_t m_Height = 0;
  uint32_t m_Channels = 0;
};
} // namespace Inferno
