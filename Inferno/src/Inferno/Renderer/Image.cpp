#include <pch.h>
#include <stdexcept>
#include <vulkan/vulkan_core.h>

#include "Image.h"
#include "VulkanUtils.h"

namespace Inferno {
Image::Image(const DeviceContext *context, const ImageSpec &spec)
    : m_Context(context) {
  // Create Image
  VkImageCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  createInfo.imageType = VK_IMAGE_TYPE_2D;
  createInfo.extent.width = spec.Width;
  createInfo.extent.height = spec.Height;
  createInfo.extent.depth = 1;
  createInfo.mipLevels = 1;
  createInfo.arrayLayers = 1;
  createInfo.format = spec.Format;
  createInfo.tiling = spec.Tiling;
  createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  createInfo.usage = spec.Usage;
  createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  createInfo.flags = 0;

  if (vkCreateImage(m_Context->Device, &createInfo, nullptr, &m_Image) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed To Create Image");
  }

  // Allocate Memory
  VkMemoryRequirements memRequirements{};
  vkGetImageMemoryRequirements(m_Context->Device, m_Image, &memRequirements);

  VkMemoryAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.allocationSize = memRequirements.size;
  allocInfo.memoryTypeIndex = VulkanUtils::FindMemoryType(
      m_Context->PhysicalDevice, memRequirements.memoryTypeBits,
      spec.Properties);

  if (vkAllocateMemory(m_Context->Device, &allocInfo, nullptr, &m_Memory) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed To Allocate Memory For Image");
  }

  // Bind Image With Memory
  if (vkBindImageMemory(m_Context->Device, m_Image, m_Memory, m_Offset) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed To Bind Image Memory");
  }

  // Specs
  m_Width = spec.Width;
  m_Height = spec.Height;
  m_Format = spec.Format;

  // Create Image View
  VkImageViewCreateInfo viewInfo{};
  viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image = m_Image;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewInfo.format = m_Format;
  viewInfo.pNext = nullptr;
  viewInfo.flags = 0;

  viewInfo.subresourceRange.aspectMask = spec.Aspect;
  viewInfo.subresourceRange.baseMipLevel = 0;
  viewInfo.subresourceRange.levelCount = 1;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount = 1;

  if (vkCreateImageView(m_Context->Device, &viewInfo, nullptr, &m_ImageView) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed To Create Image View");
  }
}

Image::~Image() { CleanUp(); }

Image::Image(Image &&other)
    : m_Context(other.m_Context), m_Image(other.m_Image),
      m_Memory(other.m_Memory), m_Offset(other.m_Offset),
      m_ImageView(other.m_ImageView), m_Width(other.m_Width),
      m_Height(other.m_Height), m_MipLevels(other.m_MipLevels),
      m_Format(other.m_Format) {
  other.m_Context = nullptr;

  other.m_Image = VK_NULL_HANDLE;
  other.m_Memory = VK_NULL_HANDLE;

  other.m_Offset = 0;
  other.m_ImageView = VK_NULL_HANDLE;
  other.m_Width = 0;
  other.m_Height = 0;
  other.m_MipLevels = 1;
  other.m_Format = VK_FORMAT_UNDEFINED;
}

Image &Image::operator=(Image &&other) {
  if (this == &other) {
    return *this;
  }
  CleanUp();

  m_Context = other.m_Context;
  m_Image = other.m_Image;
  m_Memory = other.m_Memory;
  m_Offset = other.m_Offset;
  m_ImageView = other.m_ImageView;
  m_Width = other.m_Width;
  m_Height = other.m_Height;
  m_MipLevels = other.m_MipLevels;
  m_Format = other.m_Format;

  other.m_Context = nullptr;
  other.m_Image = VK_NULL_HANDLE;
  other.m_Memory = VK_NULL_HANDLE;
  other.m_Offset = 0;
  other.m_ImageView = VK_NULL_HANDLE;
  other.m_Width = 0;
  other.m_Height = 0;
  other.m_MipLevels = 1;
  other.m_Format = VK_FORMAT_UNDEFINED;

  return *this;
}

void Image::CleanUp() {
  if (m_ImageView != VK_NULL_HANDLE)
    vkDestroyImageView(m_Context->Device, m_ImageView, nullptr);

  if (m_Image != VK_NULL_HANDLE)
    vkDestroyImage(m_Context->Device, m_Image, nullptr);

  if (m_Memory != VK_NULL_HANDLE)
    vkFreeMemory(m_Context->Device, m_Memory, nullptr);
}
} // namespace Inferno
