#pragma once

#include "Inferno/Renderer/DeviceContext.h"
#include <cstddef>
#include <glm/glm.hpp>
#include <initializer_list>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace Inferno {
// =================================================
// Shader Data Type
// =================================================

enum class ShaderDataType {
  None = 0,
  Float,
  Float2,
  Float3,
  Float4,
  Mat3,
  Mat4,
  Int,
  Int2,
  Int3,
  Int4,
  Bool,
};

inline uint32_t ShaderDataTypeSize(ShaderDataType type) {
  switch (type) {
  case ShaderDataType::None:
    return 0;
  case ShaderDataType::Float:
    return 4;
  case ShaderDataType::Float2:
    return 2 * 4;
  case ShaderDataType::Float3:
    return 3 * 4;
  case ShaderDataType::Float4:
    return 4 * 4;
  case ShaderDataType::Mat3:
    return 3 * 3 * 4;
  case ShaderDataType::Mat4:
    return 4 * 4 * 4;
  case ShaderDataType::Int:
    return 4;
  case ShaderDataType::Int2:
    return 2 * 4;
  case ShaderDataType::Int3:
    return 3 * 4;
  case ShaderDataType::Int4:
    return 4 * 4;
  case ShaderDataType::Bool:
    return 4;
  default:
    return 0;
  }
}

// =================================================
// Buffer Utility
// =================================================
struct BufferElement {
  std::string Name;
  uint32_t Offset = 0;
  uint32_t Size = 0;
  ShaderDataType Type = ShaderDataType::None;
  bool Normalized = false;

  BufferElement(std::string name, ShaderDataType type, uint32_t offset)
      : Name(std::move(name)), Offset(offset), Size(ShaderDataTypeSize(type)),
        Type(type) {}
};

template <typename VertexType> class BufferLayout {
public:
  BufferLayout() = default;

  BufferLayout(std::initializer_list<BufferElement> elements)
      : m_Elements(elements), m_Stride(sizeof(VertexType)) {}

  const std::vector<BufferElement> &GetElements() const { return m_Elements; }
  uint32_t GetStride() const { return m_Stride; }

private:
  std::vector<BufferElement> m_Elements;
  uint32_t m_Stride = 0;
};

// =================================================
// Vertex
// =================================================
struct Vertex {
  glm::vec3 Position;
  glm::vec3 Color;
  glm::vec2 TexCoord;

  static BufferLayout<Vertex> GetLayout() {
    return {
        {"a_Position", ShaderDataType::Float3, offsetof(Vertex, Position)},
        {"a_Color", ShaderDataType::Float3, offsetof(Vertex, Color)},
        {"a_TexCoord", ShaderDataType::Float2, offsetof(Vertex, TexCoord)},
    };
  }
};

// =================================================
// GPU Buffer
// =================================================
class Buffer {
public:
  Buffer(const DeviceContext *context, VkDeviceSize size,
         VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
  ~Buffer();

  Buffer(const Buffer &) = delete;
  Buffer &operator=(const Buffer &) = delete;

  Buffer(Buffer &&other);
  Buffer &operator=(Buffer &&other);

  VkBuffer Get() const { return m_Buffer; }
  VkDeviceMemory GetMemory() const { return m_Memory; }
  VkDeviceSize GetSize() const { return m_Size; }

private:
  void CleanUp();

private:
  const DeviceContext *m_DeviceContext = nullptr;

  VkBuffer m_Buffer = VK_NULL_HANDLE;
  VkDeviceMemory m_Memory = VK_NULL_HANDLE;
  VkDeviceSize m_Size = 0;
};

// =================================================
// Buffer Upload Utility
// =================================================
class BufferUploader {
public:
  static void Upload(const DeviceContext *context, Buffer &dst,
                     const void *data, VkDeviceSize size);
};

// =================================================
// Vertex Buffer
// =================================================
template <typename VertexType> class VertexBuffer {
public:
  VertexBuffer(const DeviceContext *context, VkDeviceSize size)
      : m_Context(context), m_Buffer(context, size,
                                     VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                         VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {}
  ~VertexBuffer() = default;

  VertexBuffer(const VertexBuffer &) = delete;
  VertexBuffer &operator=(const VertexBuffer &) = delete;

  VertexBuffer(VertexBuffer &&) = default;
  VertexBuffer &operator=(VertexBuffer &&) = default;

  VkBuffer Get() const { return m_Buffer.Get(); }

  void Upload(const void *data) {
    BufferUploader::Upload(m_Context, m_Buffer, data, m_Buffer.GetSize());
  }

  void SetLayout(const BufferLayout<VertexType> &layout) { m_Layout = layout; }
  const BufferLayout<VertexType> &GetLayout() const { return m_Layout; }

  VkVertexInputBindingDescription GetBindingDescription() const {
    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = m_Layout.GetStride();
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    return binding;
  }

  std::vector<VkVertexInputAttributeDescription>
  GetAttributeDescriptions() const {
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
        attr.format = VK_FORMAT_R32_UINT;
        attributes.push_back(attr);
        break;
      }
      default:
        throw std::runtime_error("Unsupported ShaderDataType in VertexBuffer");
      }
    }
    return attributes;
  }

private:
  const DeviceContext *m_Context;

  Buffer m_Buffer;
  BufferLayout<VertexType> m_Layout;
};

// =================================================
// Index Buffer
// =================================================
class IndexBuffer {
public:
  IndexBuffer(const DeviceContext *context, VkDeviceSize size,
              VkIndexType indexType);
  ~IndexBuffer() = default;

  IndexBuffer(const IndexBuffer &) = delete;
  IndexBuffer &operator=(const IndexBuffer &) = delete;

  IndexBuffer(IndexBuffer &&) = default;
  IndexBuffer &operator=(IndexBuffer &&) = default;

  VkBuffer Get() const { return m_Buffer.Get(); }
  VkIndexType GetIndexType() const { return m_IndexType; }

  void Upload(const void *data);

private:
  const DeviceContext *m_Context;

  Buffer m_Buffer;
  VkIndexType m_IndexType;
};
// =================================================
// Uniform Buffer
// =================================================
class UniformBuffer {
public:
  UniformBuffer(const DeviceContext *context, VkDeviceSize size);

  ~UniformBuffer();

  UniformBuffer(const UniformBuffer &) = delete;
  UniformBuffer &operator=(const UniformBuffer &) = delete;

  UniformBuffer(UniformBuffer &&other);

  UniformBuffer &operator=(UniformBuffer &&other);

  VkBuffer Get() const { return m_Buffer.Get(); }
  void Update(const void *data, VkDeviceSize size);

private:
  const DeviceContext *m_Context;

  Buffer m_Buffer;
  void *m_Mapped = nullptr;
};
} // namespace Inferno
