#pragma once

#include "Inferno/Core/Memory.h"
#include "Inferno/Renderer/BoundingBox.h"
#include "Inferno/Renderer/Buffer.h"
#include "Inferno/Renderer/DeviceContext.h"
#include "Inferno/Resource/Resource.h"
#include <vector>
#include <vulkan/vulkan_core.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

namespace Inferno {
class GeometryGenerator;

struct MeshVertex {
  glm::vec3 Position;
  glm::vec3 Normal;
  glm::vec3 Color;
  glm::vec2 TexCoord;

  bool operator==(const MeshVertex &other) const {
    return Position == other.Position && Normal == other.Normal &&
           Color == other.Color && TexCoord == other.TexCoord;
  }

  static BufferLayout<MeshVertex> GetLayout() {
    return {
        {"a_Position", ShaderDataType::Float3, offsetof(MeshVertex, Position)},
        {"a_Normal", ShaderDataType::Float3, offsetof(MeshVertex, Normal)},
        {"a_Color", ShaderDataType::Float3, offsetof(MeshVertex, Color)},
        {"a_TexCoord", ShaderDataType::Float2, offsetof(MeshVertex, TexCoord)},
    };
  }
};

class Mesh : public Resource {
public:
  explicit Mesh(const std::string &id, const DeviceContext *context)
      : Resource(id), m_Context(context) {}
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
                    std::vector<uint32_t> &indexBuffer);

  void SetGeometryData(std::vector<MeshVertex> &vertexBufferData,
                       std::vector<uint32_t> &indexBufferData);

  void CleanUp() {
    m_Context = nullptr;

    m_IndexBuffer.reset();
    m_VertexBuffer.reset();
  }

private:
  const DeviceContext *m_Context = nullptr;

  Scope<VertexBuffer<MeshVertex>> m_VertexBuffer = nullptr;
  Scope<IndexBuffer> m_IndexBuffer = nullptr;

  std::unique_ptr<BoundingBox> m_BoundingBox = nullptr;

  uint32_t m_VertexCount = 0;
  uint32_t m_IndexCount = 0;

  friend class GeometryGenerator;
};
} // namespace Inferno

namespace std {

template <> struct hash<Inferno::MeshVertex> {
  size_t operator()(Inferno::MeshVertex const &vertex) const {
    size_t seed = 0;

    auto hash_combine = [](size_t &s, size_t v) {
      s ^= v + 0x9e3779b9 + (s << 6) + (s >> 2);
    };

    hash_combine(seed, std::hash<glm::vec3>()(vertex.Position));
    hash_combine(seed, std::hash<glm::vec3>()(vertex.Normal));
    hash_combine(seed, std::hash<glm::vec3>()(vertex.Color));
    hash_combine(seed, std::hash<glm::vec2>()(vertex.TexCoord));

    return seed;
  }
};
} // namespace std
