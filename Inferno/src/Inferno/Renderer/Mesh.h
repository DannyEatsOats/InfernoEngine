#pragma once

#include "Inferno/Core/Memory.h"
#include "Inferno/Renderer/BoundingBox.h"
#include "Inferno/Renderer/Buffer.h"
#include "Inferno/Renderer/DeviceContext.h"
#include "Inferno/Resource/Resource.h"
#include <vector>
#include <vulkan/vulkan_core.h>

namespace Inferno {
struct MeshVertex {
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

class Mesh : public Resource {
public:
  explicit Mesh(const std::string &id) : Resource(id) {}
  ~Mesh() = default;

  Mesh(const Mesh &) = delete;
  Mesh &operator=(const Mesh &) = delete;

  Mesh(Mesh &&other);
  Mesh &operator=(Mesh &&other);

  VertexBuffer<MeshVertex> *GetVertexBuffer() const {
    return m_VertexBuffer.get();
  }
  IndexBuffer *GetIndexBuffer() const { return m_IndexBuffer.get(); }

  const BoundingBox &GetBoundingBox() const { return *m_BoundingBox.get(); }

  uint32_t GetVertexCount() const { return m_VertexCount; }
  uint32_t GetIndexCount() const { return m_IndexCount; }

protected:
  bool DoLoad() override;

  bool DoUnLoad() override;

private:
  bool LoadMeshData(std::string &filePath,
                    std::vector<MeshVertex> &vertexBuffer,
                    std::vector<uint16_t> indexBuffer);

  void CleanUp() {
    m_Context = nullptr;

    m_IndexBuffer.reset();
    m_VertexBuffer.reset();
  }

private:
  const DeviceContext *m_Context = nullptr;

  Scope<VertexBuffer<MeshVertex>> m_VertexBuffer;
  Scope<IndexBuffer> m_IndexBuffer;

  std::unique_ptr<BoundingBox> m_BoundingBox;

  uint32_t m_VertexCount = 0;
  uint32_t m_IndexCount = 0;
};
} // namespace Inferno
