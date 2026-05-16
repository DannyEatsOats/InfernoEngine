#include <cstddef>
#include <cstring>
#include <pch.h>
#include <stdexcept>
#include <vulkan/vulkan_core.h>

#include "Buffer.h"

#include "Inferno/Renderer/DeviceContext.h"
#include "VulkanUtils.h"

namespace Inferno {
// =================================================
// GPU Buffer
// =================================================
Buffer::Buffer(const DeviceContext *context, VkDeviceSize size,
               VkBufferUsageFlags usage, VkMemoryPropertyFlags properties)
    : m_DeviceContext(context), m_Size(size) {
  VkBufferCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  createInfo.pNext = nullptr;
  createInfo.flags = 0;
  createInfo.size = size;
  createInfo.usage = usage;
  createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  if (vkCreateBuffer(m_DeviceContext->Device, &createInfo, nullptr,
                     &m_Buffer) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create Buffer");
  }

  VkMemoryRequirements memRequirements;
  vkGetBufferMemoryRequirements(m_DeviceContext->Device, m_Buffer,
                                &memRequirements);

  VkMemoryAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.allocationSize = memRequirements.size;
  allocInfo.memoryTypeIndex =
      VulkanUtils::FindMemoryType(m_DeviceContext->PhysicalDevice,
                                  memRequirements.memoryTypeBits, properties);

  if (vkAllocateMemory(m_DeviceContext->Device, &allocInfo, nullptr,
                       &m_Memory) != VK_SUCCESS) {
    throw std::runtime_error("Falied To allocate Memory For Buffer");
  }

  if (vkBindBufferMemory(m_DeviceContext->Device, m_Buffer, m_Memory, 0) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed To Bind Buffer Memory");
  }
}

Buffer::~Buffer() { CleanUp(); }

Buffer::Buffer(Buffer &&other) {
  m_DeviceContext = other.m_DeviceContext;
  m_Buffer = other.m_Buffer;
  m_Memory = other.m_Memory;
  m_Size = other.m_Size;

  other.m_DeviceContext = nullptr;
  other.m_Buffer = VK_NULL_HANDLE;
  other.m_Memory = VK_NULL_HANDLE;
  other.m_Size = 0;
}

Buffer &Buffer::operator=(Buffer &&other) {
  if (this == &other) {
    return *this;
  }

  CleanUp();

  m_DeviceContext = other.m_DeviceContext;
  m_Buffer = other.m_Buffer;
  m_Memory = other.m_Memory;
  m_Size = other.m_Size;

  other.m_DeviceContext = nullptr;
  other.m_Buffer = VK_NULL_HANDLE;
  other.m_Memory = VK_NULL_HANDLE;
  other.m_Size = 0;

  return *this;
}

void Buffer::CleanUp() {
  if (!m_DeviceContext) {
    return;
  }

  if (m_Buffer != VK_NULL_HANDLE) {
    vkDestroyBuffer(m_DeviceContext->Device, m_Buffer, nullptr);
    m_Buffer = VK_NULL_HANDLE;
  }

  if (m_Memory != VK_NULL_HANDLE) {
    vkFreeMemory(m_DeviceContext->Device, m_Memory, nullptr);
    m_Memory = VK_NULL_HANDLE;
  }
}

// =================================================
// Buffer Upload Utility
// =================================================
void BufferUploader::Upload(const DeviceContext *context, Buffer &dst,
                            const void *data, VkDeviceSize size) {
  if (size > dst.GetSize()) {
    throw std::runtime_error("Size passed in BufferUploader is invalid");
  }

  Buffer stagingBuffer(context, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  void *mapped;
  vkMapMemory(context->Device, stagingBuffer.GetMemory(), 0, size, 0, &mapped);
  memcpy(mapped, data, static_cast<size_t>(size));
  vkUnmapMemory(context->Device, stagingBuffer.GetMemory());

  VulkanUtils::CopyBuffer(context, stagingBuffer.Get(), dst.Get(), size);
}

// =================================================
// Index Buffer
// =================================================
IndexBuffer::IndexBuffer(const DeviceContext *context, VkDeviceSize size,
                         VkIndexType indexType)
    : m_Context(context), m_Buffer(context, size,
                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                       VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
      m_IndexType(indexType) {}

void IndexBuffer::Upload(const void *data) {
  BufferUploader::Upload(m_Context, m_Buffer, data, m_Buffer.GetSize());
}

// =================================================
// Uniform Buffer
// =================================================
UniformBuffer::UniformBuffer(const DeviceContext *context, VkDeviceSize size)
    : m_Context(context),
      m_Buffer(context, size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) {
  if (vkMapMemory(context->Device, m_Buffer.GetMemory(), 0, size, 0,
                  &m_Mapped) != VK_SUCCESS) {
    throw std::runtime_error("Failed to Map Memory for Uniform Buffer");
  }
}

UniformBuffer::~UniformBuffer() {
  if (m_Mapped) {
    vkUnmapMemory(m_Context->Device, m_Buffer.GetMemory());
  }
}

UniformBuffer::UniformBuffer(UniformBuffer &&other)
    : m_Context(other.m_Context), m_Buffer(std::move(other.m_Buffer)),
      m_Mapped(other.m_Mapped) {
  other.m_Context = nullptr;
  other.m_Mapped = nullptr;
}

UniformBuffer &UniformBuffer::operator=(UniformBuffer &&other) {
  if (this == &other) {
    return *this;
  }

  if (m_Mapped) {
    vkUnmapMemory(m_Context->Device, m_Buffer.GetMemory());
  }

  m_Context = other.m_Context;
  m_Buffer = std::move(other.m_Buffer);
  m_Mapped = other.m_Mapped;

  other.m_Context = nullptr;
  other.m_Mapped = nullptr;

  return *this;
}

void UniformBuffer::Update(const void *data, VkDeviceSize size) {
  memcpy(m_Mapped, data, static_cast<size_t>(size));
}
} // namespace Inferno
