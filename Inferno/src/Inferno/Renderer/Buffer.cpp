#include <cstring>
#include <memory>
#include <pch.h>
#include <stdexcept>
#include <vulkan/vulkan_core.h>

#include "Buffer.h"
#include "Inferno/Renderer/RenderingContext.h"

namespace Inferno {
VertexBuffer::VertexBuffer(uint32_t size) {}

VertexBuffer::VertexBuffer(const RenderingContext *context,
                           const Vertex *vertices, uint32_t size) {
  m_pRenderingContext = context;

  VkBufferCreateInfo bufferInfo{
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size = size,
      .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
  };

  if (vkCreateBuffer(m_pRenderingContext->GetDevice(), &bufferInfo, nullptr,
                     &m_Buffer) != VK_SUCCESS) {
    throw std::runtime_error("Failed to Create Vertex Buffer");
  }

  VkMemoryRequirements memRequirements;
  vkGetBufferMemoryRequirements(m_pRenderingContext->GetDevice(), m_Buffer,
                                &memRequirements);

  VkMemoryAllocateInfo allocInfo = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .allocationSize = memRequirements.size,
      .memoryTypeIndex = m_pRenderingContext->FindMemoryType(
          memRequirements.memoryTypeBits,
          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
              VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),
  };

  if (vkAllocateMemory(m_pRenderingContext->GetDevice(), &allocInfo, nullptr,
                       &m_BufferMemory) != VK_SUCCESS) {
    throw std::runtime_error("Failed to Allocate Vertex Buffer Memory!");
  }

  vkBindBufferMemory(m_pRenderingContext->GetDevice(), m_Buffer, m_BufferMemory,
                     0);
}

VertexBuffer::~VertexBuffer() {}

void VertexBuffer::SetData(const void *data, unsigned int size) {
  void *dst;
  vkMapMemory(m_pRenderingContext->GetDevice(), m_BufferMemory, 0, size, 0,
              &dst);
  memcpy(dst, data, size);
  vkUnmapMemory(m_pRenderingContext->GetDevice(), m_BufferMemory);
}

void VertexBuffer::Destroy() const {
  vkDestroyBuffer(m_pRenderingContext->GetDevice(), m_Buffer, nullptr);
  vkFreeMemory(m_pRenderingContext->GetDevice(), m_BufferMemory, nullptr);
}

std::shared_ptr<VertexBuffer>
VertexBuffer::Create(const RenderingContext *context, const Vertex *vertices,
                     uint32_t size) {
  return std::make_shared<VertexBuffer>(context, vertices, size);
}
} // namespace Inferno
