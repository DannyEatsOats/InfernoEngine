#pragma once

#include "Inferno/Renderer/RenderingContext.h"
#include <glm/glm.hpp>
#include <memory>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace Inferno {
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

static uint32_t ShaderDataTypeSize(ShaderDataType type) {
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

struct Vertex {
  glm::vec2 Position;
  glm::vec3 Color;
};

struct BufferElement {
  std::string Name;
  uint32_t Offset;
  uint32_t Size;
  ShaderDataType Type;
  bool Normalized;

  BufferElement() = default;
  BufferElement(std::string name, ShaderDataType type)
      : Name(std::move(name)), Offset(0), Size(ShaderDataTypeSize(type)),
        Type(type), Normalized(false) {}

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
      return 3 * 3;
    case ShaderDataType::Mat4:
      return 4 * 4;
    case ShaderDataType::Int:
      return 4;
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

  BufferLayout(const std::initializer_list<BufferElement> &elements)
      : m_Elements(elements) {
    CalculateOffsetsAndStride();
  }

  const inline uint32_t GetStride() const { return m_Stride; }
  const inline std::vector<BufferElement> &GetElements() const {
    return m_Elements;
  }

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

//===================BUFFER=============================
class Buffer {
public:
  Buffer(const RenderingContext *context, VkDeviceSize size,
         VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);

  ~Buffer();

  void Destroy() const;

  void Map(void **data);
  void Unmap();
  void SetData(const void *data, unsigned int size);

  const VkBuffer &GetBuffer() const { return m_Buffer; }

private:
  VkBuffer m_Buffer = VK_NULL_HANDLE;
  VkDeviceMemory m_Memory = VK_NULL_HANDLE;
  VkDeviceSize m_Size = 0;

  const RenderingContext *m_pContext = nullptr;
};

//===================VERTEX BUFFER=============================
class VertexBuffer {
public:
  VertexBuffer(const RenderingContext *context, const Vertex *vertices,
               uint32_t size);

  void Destroy() const;

  const VkBuffer &GetBuffer() const { return m_Buffer->GetBuffer(); }

  const inline BufferLayout &GetLayout() { return m_Layout; }
  void inline SetLayout(const BufferLayout &layout) { m_Layout = layout; }

  VkVertexInputBindingDescription GetBindingDescription() const;

  std::vector<VkVertexInputAttributeDescription>
  GetAttributeDescriptions() const;

  static std::shared_ptr<VertexBuffer> Create(const RenderingContext *context,
                                              const Vertex *vertices,
                                              uint32_t size);

private:
  std::unique_ptr<Buffer> m_Buffer;
  BufferLayout m_Layout;

  const RenderingContext *m_pRenderingContext;
};
} // namespace Inferno
