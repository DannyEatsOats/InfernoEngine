#include "Image.h"
#include "VulkanUtils.h"
#include <pch.h>
#include <vulkan/vulkan_core.h>

namespace Inferno {
Image::Image(Image &&other) noexcept
    : m_Device(other.m_Device), m_Image(other.m_Image),
      m_Memory(other.m_Memory), m_Format(other.m_Format) {

  other.m_Image = VK_NULL_HANDLE;
  other.m_Memory = VK_NULL_HANDLE;
}

Image::~Image() { CleanUp(); }

Image &Image::operator=(Image &&other) noexcept {
  if (this != &other) {
    CleanUp();

    m_Device = other.m_Device;
    m_Image = other.m_Image;
    m_Memory = other.m_Memory;
    m_Format = other.m_Format;

    other.m_Device = VK_NULL_HANDLE;
    other.m_Image = VK_NULL_HANDLE;
    other.m_Memory = VK_NULL_HANDLE;
    other.m_Format = {};
  }
  return *this;
}

void Image::CleanUp() {
  if (m_Device == VK_NULL_HANDLE)
    return;
  if (m_Image) {
    vkDestroyImage(m_Device, m_Image, nullptr);
    m_Image = VK_NULL_HANDLE;
  }
  if (m_Memory) {
    vkFreeMemory(m_Device, m_Memory, nullptr);
    m_Memory = VK_NULL_HANDLE;
  }
  m_Device = VK_NULL_HANDLE;
}

Image Image::Create(const RenderingContext *context, const ImageSpec &spec) {
  VkImage image;
  VkDeviceMemory memory;

  VkImageCreateInfo imageInfo{};
  imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageInfo.imageType = VK_IMAGE_TYPE_2D;
  imageInfo.extent.width = spec.Width;
  imageInfo.extent.height = spec.Height;
  imageInfo.extent.depth = 1;
  imageInfo.mipLevels = 1;
  imageInfo.arrayLayers = 1;
  imageInfo.format = spec.Format;
  imageInfo.tiling = spec.Tiling;
  imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageInfo.usage = spec.Usage;
  imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  imageInfo.flags = 0;

  if (vkCreateImage(context->GetDevice(), &imageInfo, nullptr, &image) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed To Create Texture Image");
  }

  VkMemoryRequirements memRequirements{};
  vkGetImageMemoryRequirements(context->GetDevice(), image, &memRequirements);

  VkMemoryAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.allocationSize = memRequirements.size;
  allocInfo.memoryTypeIndex = VulkanUtils::FindMemoryType(
      context->GetPhysicalDevice(), memRequirements.memoryTypeBits,
      spec.Properties);

  if (vkAllocateMemory(context->GetDevice(), &allocInfo, nullptr, &memory) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed To Allocate Memory For Texture Image!");
  }

  if (vkBindImageMemory(context->GetDevice(), image, memory, 0) != VK_SUCCESS) {
    throw std::runtime_error("Failed To Bind Image Memory");
  }

  return Image(context->GetDevice(), image, memory, spec.Format);
}
} // namespace Inferno
