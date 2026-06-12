#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <ktx.h>
#include <ktxvulkan.h>
#include <pch.h>
#include <stb_image.h>
#include <stdexcept>
#include <utility>
#include <vulkan/vulkan_core.h>

#include "Inferno/Renderer/Buffer.h"
#include "Inferno/Renderer/Image.h"
#include "Inferno/Renderer/VulkanUtils.h"
#include "Inferno/Resource/Resource.h"
#include "Texture.h"

namespace Inferno {
Texture::Texture(Texture &&other)
    : Resource(std::move(other)), m_Context(other.m_Context),
      m_Image(std::move(other.m_Image)), m_Sampler(other.m_Sampler) {
  other.m_Context = nullptr;
  other.m_Sampler = VK_NULL_HANDLE;
}

Texture &Texture::operator=(Texture &&other) {
  if (this == &other) {
    return *this;
  }

  CleanUp();

  Resource::operator=(std::move(other));
  m_Context = other.m_Context;
  m_Image = std::move(other.m_Image);
  m_Sampler = other.m_Sampler;

  other.m_Context = nullptr;
  other.m_Sampler = VK_NULL_HANDLE;

  return *this;
}

bool Texture::DoLoad() {
  // TODO: Create canonical path
  // std::string filePath = "textures/" + GetID() + ".ktx";
  std::string filePath = "assets/textures/" + GetID() + ".png";

  LoadFromFile(filePath);

  // LoadFromKTX2(filePath);

  // CreateVulkanImage(data, m_Width, m_Height, m_Channels);
  // FreeImage(data);

  return true;
}

/*
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

  // Store GPU objects
  m_Image = texture.image;
  m_Memory = texture.deviceMemory;
  m_ImageView = imageView;

  // Cleanup CPU KTX immediately (IMPORTANT)
  ktxTexture_Destroy(reinterpret_cast<ktxTexture *>(kTexture));
}
*/

void Texture::LoadFromFile(const std::string &filePath) {
  std::filesystem::path exePath =
      std::filesystem::canonical("/proc/self/exe").parent_path();
  std::filesystem::path fullPath = exePath / filePath;

  int width, height, channels;
  stbi_uc *pixels = stbi_load(fullPath.string().c_str(), &width, &height,
                              &channels, STBI_rgb_alpha);

  VkDeviceSize imageSize =
      static_cast<uint32_t>(width) * static_cast<uint32_t>(height) * 4;

  uint32_t mipLevels =
      static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;

  if (!pixels) {
    throw std::runtime_error("Failed to load Texture from file: " +
                             fullPath.string());
  }

  Buffer stagingBuffer(m_Context, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  void *data;
  vkMapMemory(m_Context->Device, stagingBuffer.GetMemory(), 0,
              stagingBuffer.GetSize(), 0, &data);
  memcpy(data, pixels, static_cast<size_t>(imageSize));
  vkUnmapMemory(m_Context->Device, stagingBuffer.GetMemory());

  stbi_image_free(pixels);
  // Create Image
  CreateVulkanImage(stagingBuffer.Get(), static_cast<uint32_t>(width),
                    static_cast<uint32_t>(height), mipLevels);
}

void Texture::CreateVulkanImage(VkBuffer srcBuffer, uint32_t width,
                                uint32_t height, uint32_t mipLevels) {
  ImageSpec spec{};
  spec.Width = static_cast<uint32_t>(width);
  spec.Height = static_cast<uint32_t>(height);
  spec.MipLevels = static_cast<uint32_t>(mipLevels);
  spec.Format = VK_FORMAT_R8G8B8A8_SRGB;
  spec.Tiling = VK_IMAGE_TILING_OPTIMAL;
  spec.Usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
               VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  spec.Properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
  spec.Aspect = VK_IMAGE_ASPECT_COLOR_BIT;

  m_Image = Image(m_Context, spec);

  VulkanUtils::TransitionImageLayout(m_Context, m_Image,
                                     VK_IMAGE_LAYOUT_UNDEFINED,
                                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  VulkanUtils::CopyBufferToImage(m_Context, srcBuffer, m_Image.GetImage(),
                                 m_Image.GetWidth(), m_Image.GetHeight());
  GenerateMipmaps();

  // Create Sampler
  VkSamplerCreateInfo samplerInfo{};
  samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerInfo.magFilter = VK_FILTER_LINEAR;
  samplerInfo.minFilter = VK_FILTER_LINEAR;
  samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  samplerInfo.anisotropyEnable = VK_TRUE;
  samplerInfo.maxAnisotropy =
      VulkanUtils::GetPhysicalDeviceProps(m_Context->PhysicalDevice)
          .limits.maxSamplerAnisotropy;
  samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  samplerInfo.unnormalizedCoordinates = VK_FALSE;
  samplerInfo.compareEnable = VK_FALSE;
  samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
  samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  samplerInfo.mipLodBias = 0.0f;
  samplerInfo.minLod = 0.0f;
  samplerInfo.maxLod = static_cast<float>(m_Image.GetMipLevels());

  if (vkCreateSampler(m_Context->Device, &samplerInfo, nullptr, &m_Sampler) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed to Create Sampler");
  }
}

void Texture::GenerateMipmaps() {
  VkCommandBuffer commandBuffer =
      VulkanUtils::BeginSingleTimeCommands(m_Context);

  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.image = m_Image.GetImage();
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;
  barrier.subresourceRange.levelCount = 1;

  int32_t mipWidth = m_Image.GetWidth();
  int32_t mipHeight = m_Image.GetHeight();

  for (uint32_t i = 1; i < m_Image.GetMipLevels(); ++i) {
    barrier.subresourceRange.baseMipLevel = i - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &barrier);

    VkImageBlit blit{};
    blit.srcOffsets[0] = {0, 0, 0};
    blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
    blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.srcSubresource.mipLevel = i - 1;
    blit.srcSubresource.baseArrayLayer = 0;
    blit.srcSubresource.layerCount = 1;
    blit.dstOffsets[0] = {0, 0, 0};
    blit.dstOffsets[1] = {mipWidth > 1 ? mipWidth / 2 : 1,
                          mipHeight > 1 ? mipHeight / 2 : 1, 1};
    blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.dstSubresource.mipLevel = i;
    blit.dstSubresource.baseArrayLayer = 0;
    blit.dstSubresource.layerCount = 1;

    vkCmdBlitImage(commandBuffer, m_Image.GetImage(),
                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_Image.GetImage(),
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit,
                   VK_FILTER_LINEAR);

    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
                         0, nullptr, 1, &barrier);

    if (mipWidth > 1)
      mipWidth /= 2;
    if (mipHeight > 1)
      mipHeight/= 2;
  }

  barrier.subresourceRange.baseMipLevel = m_Image.GetMipLevels() - 1;
  barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

  vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &barrier);

  VulkanUtils::EndSingleTimeCommands(m_Context, commandBuffer);
}

bool Texture::DoUnLoad() {
  CleanUp();

  return true;
}

void Texture::CleanUp() {
  if (m_Sampler != VK_NULL_HANDLE) {
    vkDestroySampler(m_Context->Device, m_Sampler, nullptr);
    m_Sampler = VK_NULL_HANDLE;
  }
}

} // namespace Inferno
