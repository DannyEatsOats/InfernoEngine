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

  VkBufferCreateInfo bufferInfo{};
  bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferInfo.size = size;
  bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  if (vkCreateBuffer(m_pRenderingContext->GetDevice(), &bufferInfo, nullptr,
                     &m_Buffer) != VK_SUCCESS) {
    throw std::runtime_error("Failed to Create Vertex Buffer");
  }
}

VertexBuffer::~VertexBuffer() {}

void VertexBuffer::Destroy() const {
  vkDestroyBuffer(m_pRenderingContext->GetDevice(), m_Buffer, nullptr);
}

std::shared_ptr<VertexBuffer>
VertexBuffer::Create(const RenderingContext *context, const Vertex *vertices,
                     uint32_t size) {
  return std::make_shared<VertexBuffer>(context, vertices, size);
}
} // namespace Inferno
