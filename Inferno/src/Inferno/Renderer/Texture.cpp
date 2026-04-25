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
  tex->LoadFromFile(context, filePath, context->GetCommandPool(),
                    context->GetGraphicsQueue());
  tex->CreateImage(context);
  return tex;
}

Texture::Texture(TextureType type, VkDevice device,
                 VkPhysicalDevice physicalDevice)
    : m_Type(type), m_Device(device), m_PhysicalDevice(physicalDevice) {}

Texture::~Texture() {}

void Texture::LoadFromFile(const RenderingContext *context,
                           const std::string &path, VkCommandPool commandPool,
                           VkQueue graphicsQueue) {
  int width, height, channels;
  stbi_uc *pixels =
      stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);

  m_Width = static_cast<uint32_t>(width);
  m_Height = static_cast<uint32_t>(height);
  m_Channels = static_cast<uint32_t>(channels);

  VkDeviceSize imageSize = width * height * 4;

  m_ImageSize = imageSize;

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
}

void Texture::CreateImage(const RenderingContext *context) {
  // Add Type Creation Switch
  VkImageCreateInfo imageInfo{};
  imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageInfo.imageType = VK_IMAGE_TYPE_2D;
  imageInfo.extent.width = m_Width;
  imageInfo.extent.height = m_Height;
  imageInfo.extent.depth = 1;
  imageInfo.mipLevels = 1;
  imageInfo.arrayLayers = 1;
  imageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
  imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
  imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageInfo.usage =
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  imageInfo.flags = 0;

  if (vkCreateImage(context->GetDevice(), &imageInfo, nullptr, &m_Image) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed To Create Texture Image");
  }

  VkMemoryRequirements memRequirements{};
  vkGetImageMemoryRequirements(context->GetDevice(), m_Image, &memRequirements);

  VkMemoryAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.allocationSize = memRequirements.size;
  allocInfo.memoryTypeIndex = VulkanUtils::FindMemoryType(
      context->GetPhysicalDevice(), memRequirements.memoryTypeBits,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  if (vkAllocateMemory(context->GetDevice(), &allocInfo, nullptr,
                       &m_TextureImageMemory) != VK_SUCCESS) {
    throw std::runtime_error("Failed To Allocate Memory For Texture Image!");
  }

  vkBindImageMemory(context->GetDevice(), m_Image, m_TextureImageMemory, 0);
}

} // namespace Inferno
