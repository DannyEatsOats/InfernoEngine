#include <pch.h>

#include "Inferno/Renderer/Buffer.h"
#include "Inferno/Renderer/VulkanUtils.h"
#include "Texture.h"
#include <cstring>
#include <memory>

#include <stb_image.h>
#include <stdexcept>
#include <vulkan/vulkan_core.h>

namespace Inferno {

std::shared_ptr<Texture> Texture::Create2D(const RenderingContext *context,
                                           const std::string &filePath) {
  auto tex = std::shared_ptr<Texture>(
      new Texture(TextureType::Texture2D, context->GetDevice(),
                  context->GetPhysicalDevice()));
  tex->LoadFromFile(context, filePath);

  return tex;
}

Texture::Texture(TextureType type, VkDevice device,
                 VkPhysicalDevice physicalDevice)
    : m_Type(type), m_Device(device), m_PhysicalDevice(physicalDevice) {}

void Texture::LoadFromFile(const RenderingContext *context,
                           const std::string &path) {
  // Loading
  int width, height, channels;
  stbi_uc *pixels =
      stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);

  m_Width = static_cast<uint32_t>(width);
  m_Height = static_cast<uint32_t>(height);
  m_Channels = static_cast<uint32_t>(channels);

  VkDeviceSize imageSize = m_Width * m_Height * 4;

  if (!pixels) {
    throw std::runtime_error("Failed To Load Texture: " + path);
  }

  Buffer stagingBuffer(context, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  void *data;
  vkMapMemory(context->GetDevice(), stagingBuffer.GetMemory(), 0,
              stagingBuffer.GetSize(), 0, &data);
  memcpy(data, pixels, static_cast<size_t>(imageSize));
  vkUnmapMemory(context->GetDevice(), stagingBuffer.GetMemory());

  stbi_image_free(pixels);

  // Image Creation
  CreateImage(context);
  VulkanUtils::TransitionImageLayout(context, m_Image.Get(), m_Format,
                                     VK_IMAGE_LAYOUT_UNDEFINED,
                                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  VulkanUtils::CopyBufferToImage(context, stagingBuffer.Get(), m_Image.Get(),
                                 m_Width, m_Height);
  VulkanUtils::TransitionImageLayout(context, m_Image.Get(), m_Format,
                                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void Texture::CreateImage(const RenderingContext *context) {
  ImageSpec spec{};

  switch (m_Type) {
  case TextureType::Texture2D: {
    spec.Width = m_Width;
    spec.Height = m_Height;
    spec.Format = VK_FORMAT_R8G8B8A8_SRGB;
    spec.Tiling = VK_IMAGE_TILING_OPTIMAL;
    spec.Usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    spec.Properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    break;
  }
  default:
    throw std::runtime_error("Unsupported Texture Type");
  }

  m_Image = Image::Create(context, spec);
}
} // namespace Inferno
