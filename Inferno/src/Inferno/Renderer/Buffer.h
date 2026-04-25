#pragma once

#include "Inferno/Renderer/RenderingContext.h"
#include <glm/glm.hpp>
#include <memory>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace Inferno {

//============================================================
// Shader Data Types
//============================================================
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
  Bool
};

inline uint32_t ShaderDataTypeSize(ShaderDataType type) {
  switch (type) {
  case ShaderDataType::Float:
    return 4;
  case ShaderDataType::Float2:
    return 4 * 2;
  case ShaderDataType::Float3:
    return 4 * 3;
  case ShaderDataType::Float4:
    return 4 * 4;
  case ShaderDataType::Mat3:
    return 4 * 3 * 3;
  case ShaderDataType::Mat4:
    return 4 * 4 * 4;
  case ShaderDataType::Int:
    return 4;
  case ShaderDataType::Int2:
    return 4 * 2;
  case ShaderDataType::Int3:
    return 4 * 3;
  case ShaderDataType::Int4:
    return 4 * 4;
  case ShaderDataType::Bool:
    return 4;
  default:
    return 0;
  }
}

//============================================================
// Vertex
//============================================================
struct Vertex {
  glm::vec2 Position;
  glm::vec3 Color;
};

//============================================================
// Buffer Layout
//============================================================
struct BufferElement {
  std::string Name;
  uint32_t Offset = 0;
  uint32_t Size = 0;
  ShaderDataType Type = ShaderDataType::None;
  bool Normalized = false;

  BufferElement() = default;

  BufferElement(std::string name, ShaderDataType type)
      : Name(std::move(name)), Size(ShaderDataTypeSize(type)), Type(type) {}

  uint32_t GetComponentCount() const {
    switch (Type) {
    case ShaderDataType::Float:
      return 1;
    case ShaderDataType::Float2:
      return 2;
    case ShaderDataType::Float3:
      return 3;
    case ShaderDataType::Float4:
      return 4;
    case ShaderDataType::Mat3:
      return 9;
    case ShaderDataType::Mat4:
      return 16;
    case ShaderDataType::Int:
      return 1;
    case ShaderDataType::Int2:
      return 2;
    case ShaderDataType::Int3:
      return 3;
    case ShaderDataType::Int4:
      return 4;
    case ShaderDataType::Bool:
      return 1;
    default:
      return 0;
    }
  }
};

class BufferLayout {
public:
  BufferLayout() = default;

  BufferLayout(std::initializer_list<BufferElement> elements)
      : m_Elements(elements) {
    CalculateOffsetsAndStride();
  }

  uint32_t GetStride() const { return m_Stride; }
  const std::vector<BufferElement> &GetElements() const { return m_Elements; }

private:
  void CalculateOffsetsAndStride() {
    uint32_t offset = 0;
    m_Stride = 0;

    for (auto &element : m_Elements) {
      element.Offset = offset;
      offset += element.Size;
      m_Stride += element.Size;
    }
  }

private:
  std::vector<BufferElement> m_Elements;
  uint32_t m_Stride = 0;
};

//============================================================
// Buffer (GPU Resource Only)
//============================================================
class Buffer {
public:
  Buffer(const RenderingContext *context, VkDeviceSize size,
         VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);

  ~Buffer();

  Buffer(const Buffer &) = delete;
  Buffer &operator=(const Buffer &) = delete;

  VkBuffer Get() const { return m_Buffer; }
  VkDeviceMemory GetMemory() const { return m_Memory; }
  VkDeviceSize GetSize() const { return m_Size; }

private:
  VkBuffer m_Buffer = VK_NULL_HANDLE;
  VkDeviceMemory m_Memory = VK_NULL_HANDLE;
  VkDeviceSize m_Size = 0;

  const RenderingContext *m_pContext = nullptr;
};

//============================================================
// Buffer Upload Utility (Staging + Copy)
//============================================================
class BufferUploader {
public:
  static void Upload(const RenderingContext *context, Buffer &dst,
                     const void *data, VkDeviceSize size);

  static void UploadToImage(const RenderingContext *context, VkImage image,
                            uint32_t imageSize);
};

//============================================================
// Vertex Buffer (Thin Wrapper)
//============================================================
class VertexBuffer {
public:
  VertexBuffer(const RenderingContext *context, VkDeviceSize size);

  VertexBuffer(const VertexBuffer &) = delete;
  VertexBuffer &operator=(const VertexBuffer &) = delete;

  VkBuffer Get() const { return m_Buffer.Get(); }

  void Upload(const void *data);

  void SetLayout(const BufferLayout &layout) { m_Layout = layout; }
  const BufferLayout &GetLayout() const { return m_Layout; }

  VkVertexInputBindingDescription GetBindingDescription() const;
  std::vector<VkVertexInputAttributeDescription>
  GetAttributeDescriptions() const;

  static std::unique_ptr<VertexBuffer> Create(const RenderingContext *context,
                                              VkDeviceSize size);

private:
  Buffer m_Buffer;
  BufferLayout m_Layout;

  const RenderingContext *m_pContext = nullptr;
};
//============================================================
// Index Buffer (Thin Wrapper)
//============================================================
class IndexBuffer {
public:
  IndexBuffer(const RenderingContext *context, VkDeviceSize size,
              VkIndexType indexType);

  IndexBuffer(const IndexBuffer &) = delete;
  IndexBuffer &operator=(const IndexBuffer &) = delete;

  VkBuffer Get() const { return m_Buffer.Get(); }
  VkIndexType GetIndexType() const { return m_IndexType; }

  void Upload(const void *data);

  static std::unique_ptr<IndexBuffer> Create(const RenderingContext *context,
                                             VkDeviceSize size,
                                             VkIndexType indexType);

private:
  Buffer m_Buffer;
  VkIndexType m_IndexType;

  const RenderingContext *m_pContext = nullptr;
};
//============================================================
// Uniform Buffer (Thin Wrapper)
//============================================================
class UniformBuffer {
public:
  UniformBuffer(const RenderingContext *context, VkDeviceSize size);
  ~UniformBuffer();

  UniformBuffer(const UniformBuffer &) = delete;
  UniformBuffer &operator=(const UniformBuffer &) = delete;

  VkBuffer Get() const { return m_Buffer.Get(); }

  void Update(const void *data, VkDeviceSize size);

  static std::unique_ptr<UniformBuffer> Create(const RenderingContext *context,
                                               VkDeviceSize size);

private:
  Buffer m_Buffer;
  void *m_pMapped = nullptr;

  const RenderingContext *m_pContext = nullptr;
};
} // namespace Inferno
