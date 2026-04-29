#include <pch.h>

#include "Inferno/Renderer/Buffer.h"
#include "Inferno/Renderer/RenderingContext.h"
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

Texture::~Texture() {
  vkDestroySampler(m_Device, m_Sampler, nullptr);
  vkDestroyImageView(m_Device, m_ImageView, nullptr);
}

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

  m_ImageView = VulkanUtils::CreateImageView(context, m_Image);

  CreateSampler(context);
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

void Texture::CreateSampler(const RenderingContext *context) {
  VkSamplerCreateInfo samplerInfo{};
  samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerInfo.magFilter = VK_FILTER_LINEAR;
  samplerInfo.minFilter = VK_FILTER_LINEAR;
  samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  samplerInfo.anisotropyEnable = VK_TRUE;
  samplerInfo.maxAnisotropy =
      VulkanUtils::GetPhysicalDeviceProps(context->GetPhysicalDevice())
          .limits.maxSamplerAnisotropy;
  samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  samplerInfo.unnormalizedCoordinates = VK_FALSE;
  samplerInfo.compareEnable = VK_FALSE;
  samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
  samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  samplerInfo.mipLodBias = 0.0f;
  samplerInfo.minLod = 0.0f;
  samplerInfo.maxLod = 0.0f;

  // MOVE PHYSICAL DEVICE CHECK TO UTILS AND CONDITIONALLY SET ANISOTROPY

  if (vkCreateSampler(context->GetDevice(), &samplerInfo, nullptr,
                      &m_Sampler) != VK_SUCCESS) {
    throw std::runtime_error("Failed To Create Texture Sampler");
  }
}
} // namespace Inferno
