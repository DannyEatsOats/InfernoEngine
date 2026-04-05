#include <cstring>
#include <memory>
#include <pch.h>
#include <stdexcept>
#include <vulkan/vulkan_core.h>

#include "Buffer.h"
#include "Inferno/Log.h"
#include "Inferno/Renderer/RenderingContext.h"

namespace Inferno {

//===================BUFFER=============================
Buffer::Buffer(const RenderingContext *context, VkDeviceSize size,
               VkBufferUsageFlags usage, VkMemoryPropertyFlags properties)
    : m_pContext(context), m_Size(size) {
  VkBufferCreateInfo bufferInfo{
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size = size,
      .usage = usage,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
  };

  if (vkCreateBuffer(m_pContext->GetDevice(), &bufferInfo, nullptr,
                     &m_Buffer) != VK_SUCCESS) {
    throw std::runtime_error("Failed to Create Buffer");
  }

  VkMemoryRequirements memRequirements;
  vkGetBufferMemoryRequirements(m_pContext->GetDevice(), m_Buffer,
                                &memRequirements);

  VkMemoryAllocateInfo allocInfo = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .allocationSize = memRequirements.size,
      .memoryTypeIndex = m_pContext->FindMemoryType(
          memRequirements.memoryTypeBits, properties),
  };

  if (vkAllocateMemory(m_pContext->GetDevice(), &allocInfo, nullptr,
                       &m_Memory) != VK_SUCCESS) {
    throw std::runtime_error("Failed to Allocate Vertex Buffer Memory!");
  }

  vkBindBufferMemory(m_pContext->GetDevice(), m_Buffer, m_Memory, 0);
}

Buffer::~Buffer() {
  if (m_Buffer != VK_NULL_HANDLE)
    vkDestroyBuffer(m_pContext->GetDevice(), m_Buffer, nullptr);

  if (m_Memory != VK_NULL_HANDLE)
    vkFreeMemory(m_pContext->GetDevice(), m_Memory, nullptr);
}

void Buffer::Map(void **data) {
  vkMapMemory(m_pContext->GetDevice(), m_Memory, 0, m_Size, 0, data);
}

void Buffer::Unmap() { vkUnmapMemory(m_pContext->GetDevice(), m_Memory); }

void Buffer::SetData(const void *data, unsigned int size) {
  void *dst;
  Map(&dst);
  memcpy(dst, data, size);
  Unmap();
}

//===================VERTEX BUFFER=============================
VertexBuffer::VertexBuffer(const RenderingContext *context, uint32_t size)
    : m_pRenderingContext(context),
      m_VertexBuffer(context, size,
                     VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                         VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {}

void VertexBuffer::SetData(const Vertex *vertices, uint32_t size) {
  Buffer stagingBuffer(m_pRenderingContext, size,
                       VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  stagingBuffer.SetData(vertices, size);
  CopyBuffer(stagingBuffer.GetBuffer(), m_VertexBuffer.GetBuffer(), size);
}

VkVertexInputBindingDescription VertexBuffer::GetBindingDescription() const {
  VkVertexInputBindingDescription binding = {
      .binding = 0,
      .stride = m_Layout.GetStride(),
      .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
  };
  return binding;
}

std::vector<VkVertexInputAttributeDescription>
VertexBuffer::GetAttributeDescriptions() const {
  std::vector<VkVertexInputAttributeDescription> attributes;
  const auto &elements = m_Layout.GetElements();

  uint32_t locationCounter = 0;

  for (const auto &e : elements) {
    switch (e.Type) {
    // Float types
    case ShaderDataType::Float: {
      VkVertexInputAttributeDescription attr{};
      attr.binding = 0;
      attr.location = locationCounter++;
      attr.offset = e.Offset;
      attr.format = VK_FORMAT_R32_SFLOAT;
      attributes.push_back(attr);
      break;
    }
    case ShaderDataType::Float2: {
      VkVertexInputAttributeDescription attr{};
      attr.binding = 0;
      attr.location = locationCounter++;
      attr.offset = e.Offset;
      attr.format = VK_FORMAT_R32G32_SFLOAT;
      attributes.push_back(attr);
      break;
    }
    case ShaderDataType::Float3: {
      VkVertexInputAttributeDescription attr{};
      attr.binding = 0;
      attr.location = locationCounter++;
      attr.offset = e.Offset;
      attr.format = VK_FORMAT_R32G32B32_SFLOAT;
      attributes.push_back(attr);
      break;
    }
    case ShaderDataType::Float4: {
      VkVertexInputAttributeDescription attr{};
      attr.binding = 0;
      attr.location = locationCounter++;
      attr.offset = e.Offset;
      attr.format = VK_FORMAT_R32G32B32A32_SFLOAT;
      attributes.push_back(attr);
      break;
    }

    // Integer types
    case ShaderDataType::Int: {
      VkVertexInputAttributeDescription attr{};
      attr.binding = 0;
      attr.location = locationCounter++;
      attr.offset = e.Offset;
      attr.format = VK_FORMAT_R32_SINT;
      attributes.push_back(attr);
      break;
    }
    case ShaderDataType::Int2: {
      VkVertexInputAttributeDescription attr{};
      attr.binding = 0;
      attr.location = locationCounter++;
      attr.offset = e.Offset;
      attr.format = VK_FORMAT_R32G32_SINT;
      attributes.push_back(attr);
      break;
    }
    case ShaderDataType::Int3: {
      VkVertexInputAttributeDescription attr{};
      attr.binding = 0;
      attr.location = locationCounter++;
      attr.offset = e.Offset;
      attr.format = VK_FORMAT_R32G32B32_SINT;
      attributes.push_back(attr);
      break;
    }
    case ShaderDataType::Int4: {
      VkVertexInputAttributeDescription attr{};
      attr.binding = 0;
      attr.location = locationCounter++;
      attr.offset = e.Offset;
      attr.format = VK_FORMAT_R32G32B32A32_SINT;
      attributes.push_back(attr);
      break;
    }

    // Matrix types (split into columns)
    case ShaderDataType::Mat3: {
      for (uint32_t i = 0; i < 3; ++i) {
        VkVertexInputAttributeDescription attr{};
        attr.binding = 0;
        attr.location = locationCounter++;
        attr.offset = e.Offset + i * sizeof(glm::vec3);
        attr.format = VK_FORMAT_R32G32B32_SFLOAT;
        attributes.push_back(attr);
      }
      break;
    }
    case ShaderDataType::Mat4: {
      for (uint32_t i = 0; i < 4; ++i) {
        VkVertexInputAttributeDescription attr{};
        attr.binding = 0;
        attr.location = locationCounter++;
        attr.offset = e.Offset + i * sizeof(glm::vec4);
        attr.format = VK_FORMAT_R32G32B32A32_SFLOAT;
        attributes.push_back(attr);
      }
      break;
    }

    case ShaderDataType::Bool: {
      VkVertexInputAttributeDescription attr{};
      attr.binding = 0;
      attr.location = locationCounter++;
      attr.offset = e.Offset;
      attr.format = VK_FORMAT_R32_UINT; // Booleans are usually stored as uint32
      attributes.push_back(attr);
      break;
    }

    default:
      throw std::runtime_error("Unsupported ShaderDataType in VertexBuffer");
    }
  }

  return attributes;
}
std::shared_ptr<VertexBuffer>
VertexBuffer::Create(const RenderingContext *context,
                     uint32_t size) {
  return std::make_shared<VertexBuffer>(context, size);
}

void VertexBuffer::CopyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size) {
  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandPool = m_pRenderingContext->GetCommandPool();
  allocInfo.commandBufferCount = 1;

  VkCommandBuffer commandBuffer;
  vkAllocateCommandBuffers(m_pRenderingContext->GetDevice(), &allocInfo,
                           &commandBuffer);

  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  vkBeginCommandBuffer(commandBuffer, &beginInfo);

  VkBufferCopy copyRegion{};
  copyRegion.srcOffset = 0;
  copyRegion.dstOffset = 0;
  copyRegion.size = size;

  vkCmdCopyBuffer(commandBuffer, src, dst, 1, &copyRegion);
  vkEndCommandBuffer(commandBuffer);

  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;

  vkQueueSubmit(m_pRenderingContext->GetGraphicsQueue(), 1, &submitInfo,
                VK_NULL_HANDLE);
  vkQueueWaitIdle(m_pRenderingContext->GetGraphicsQueue());

  vkFreeCommandBuffers(m_pRenderingContext->GetDevice(),
                       m_pRenderingContext->GetCommandPool(), 1,
                       &commandBuffer);
}
} // namespace Inferno
