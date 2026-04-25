#include <cstring>
#include <memory>
#include <pch.h>
#include <stdexcept>
#include <vulkan/vulkan_core.h>

#include "Inferno/Renderer/Buffer.h"
#include "Inferno/Renderer/RenderingContext.h"
#include "Inferno/Renderer/VulkanUtils.h"

namespace Inferno {

//=================== BUFFER ==============================
Buffer::Buffer(const RenderingContext *context, VkDeviceSize size,
               VkBufferUsageFlags usage, VkMemoryPropertyFlags properties)
    : m_pContext(context), m_Size(size) {

  VkBufferCreateInfo bufferInfo{};
  bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferInfo.size = size;
  bufferInfo.usage = usage;
  bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  if (vkCreateBuffer(m_pContext->GetDevice(), &bufferInfo, nullptr,
                     &m_Buffer) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create buffer");
  }

  VkMemoryRequirements memRequirements;
  vkGetBufferMemoryRequirements(m_pContext->GetDevice(), m_Buffer,
                                &memRequirements);

  VkMemoryAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.allocationSize = memRequirements.size;
  allocInfo.memoryTypeIndex =
      VulkanUtils::FindMemoryType(m_pContext->GetPhysicalDevice(),
                                  memRequirements.memoryTypeBits, properties);

  if (vkAllocateMemory(m_pContext->GetDevice(), &allocInfo, nullptr,
                       &m_Memory) != VK_SUCCESS) {
    throw std::runtime_error("Failed to allocate buffer memory");
  }

  vkBindBufferMemory(m_pContext->GetDevice(), m_Buffer, m_Memory, 0);
}

Buffer::~Buffer() {
  if (m_Buffer != VK_NULL_HANDLE)
    vkDestroyBuffer(m_pContext->GetDevice(), m_Buffer, nullptr);

  if (m_Memory != VK_NULL_HANDLE)
    vkFreeMemory(m_pContext->GetDevice(), m_Memory, nullptr);
}

//=================== BUFFER UTILITY ==============================

void BufferUploader::Upload(const RenderingContext *context, Buffer &dst,
                            const void *data, VkDeviceSize size) {
  if (size > dst.GetSize()) {
    throw std::runtime_error("Size passed in BufferUploader is invalid");
  }

  // 1. Create staging buffer
  Buffer stagingBuffer(context, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  // 2. Copy CPU → staging
  void *mapped;
  vkMapMemory(context->GetDevice(), stagingBuffer.GetMemory(), 0, size, 0,
              &mapped);
  memcpy(mapped, data, static_cast<size_t>(size));
  vkUnmapMemory(context->GetDevice(), stagingBuffer.GetMemory());

  // 3. Allocate command buffer
  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandPool = context->GetCommandPool();
  allocInfo.commandBufferCount = 1;

  VkCommandBuffer cmd;
  vkAllocateCommandBuffers(context->GetDevice(), &allocInfo, &cmd);

  // 4. Record copy command
  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  vkBeginCommandBuffer(cmd, &beginInfo);

  VkBufferCopy copy{};
  copy.size = size;

  vkCmdCopyBuffer(cmd, stagingBuffer.Get(), dst.Get(), 1, &copy);

  vkEndCommandBuffer(cmd);

  // 5. Submit
  VkSubmitInfo submit{};
  submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit.commandBufferCount = 1;
  submit.pCommandBuffers = &cmd;

  vkQueueSubmit(context->GetGraphicsQueue(), 1, &submit, VK_NULL_HANDLE);
  vkQueueWaitIdle(context->GetGraphicsQueue());

  // 6. Cleanup
  vkFreeCommandBuffers(context->GetDevice(), context->GetCommandPool(), 1,
                       &cmd);
}

//=================== VERTEX BUFFER ==============================

VertexBuffer::VertexBuffer(const RenderingContext *context, VkDeviceSize size)
    : m_pContext(context), m_Buffer(context, size,
                                    VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {}

void VertexBuffer::Upload(const void *data) {
  BufferUploader::Upload(m_pContext, m_Buffer, data, m_Buffer.GetSize());
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
    switch (e.Type) { // Float types
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
    } // Matrix types (split into columns)
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

std::unique_ptr<VertexBuffer>
VertexBuffer::Create(const RenderingContext *context, VkDeviceSize size) {
  return std::make_unique<VertexBuffer>(context, size);
}

//=================== INDEX BUFFER ==============================
IndexBuffer::IndexBuffer(const RenderingContext *context, VkDeviceSize size,
                         VkIndexType indexType)
    : m_pContext(context), m_Buffer(context, size,
                                    VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
      m_IndexType(indexType) {}

void IndexBuffer::Upload(const void *data) {
  BufferUploader::Upload(m_pContext, m_Buffer, data, m_Buffer.GetSize());
}

std::unique_ptr<IndexBuffer>
IndexBuffer::Create(const RenderingContext *context, VkDeviceSize size,
                    VkIndexType indexType) {
  return std::make_unique<IndexBuffer>(context, size, indexType);
}

//=================== UNIFORM BUFFER ==============================
UniformBuffer::UniformBuffer(const RenderingContext *context, VkDeviceSize size)
    : m_pContext(context),
      m_Buffer(context, size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) {
  vkMapMemory(m_pContext->GetDevice(), m_Buffer.GetMemory(), 0, size, 0,
              &m_pMapped);
}

UniformBuffer::~UniformBuffer() {
  if (m_pMapped)
    vkUnmapMemory(m_pContext->GetDevice(), m_Buffer.GetMemory());
}

void UniformBuffer::Update(const void *data, VkDeviceSize size) {
  memcpy(m_pMapped, data, static_cast<size_t>(size));
}

std::unique_ptr<UniformBuffer>
UniformBuffer::Create(const RenderingContext *context, VkDeviceSize size) {
  return std::make_unique<UniformBuffer>(context, size);
}
} // namespace Inferno
