#include <pch.h>

#include "Inferno/Resource/Resource.h"
#include "Mesh.h"

namespace Inferno {

Mesh::Mesh(Mesh &&other)
    : Resource(std::move(other)), m_Context(other.m_Context),
      m_VertexBuffer(std::move(other.m_VertexBuffer)),
      m_IndexBuffer(std::move(other.m_IndexBuffer)),
      m_VertexCount(other.m_VertexCount), m_IndexCount(other.m_IndexCount) {
  other.m_Context = nullptr;
  other.m_VertexCount = 0;
  other.m_IndexCount = 0;
}

Mesh &Mesh::operator=(Mesh &&other) {
  if (this == &other) {
    return *this;
  }

  CleanUp();

  m_Context = other.m_Context;
  m_VertexBuffer = std::move(other.m_VertexBuffer);
  m_IndexBuffer = std::move(other.m_IndexBuffer);

  return *this;
}

bool Mesh::DoLoad() {
  // TODO: Create canonical path
  std::string filePath = "models/" + GetID() + ".gltf";

  std::vector<MeshVertex> vertices;
  std::vector<uint16_t> indices;
  if (!LoadMeshData(filePath, vertices, indices)) {
    return false;
  }

  m_VertexCount = static_cast<uint32_t>(vertices.size());
  m_IndexCount = static_cast<uint32_t>(indices.size());

  // TODO: move these params inside cosntructor, only pass the vector to
  // Buffers
  m_VertexBuffer = MakeScope<VertexBuffer<MeshVertex>>(
      m_Context, sizeof(MeshVertex) * m_VertexCount);
  m_IndexBuffer = MakeScope<IndexBuffer>(
      m_Context, sizeof(indices[0]) * m_IndexCount, VK_INDEX_TYPE_UINT16);

  m_VertexBuffer->Upload(vertices.data());
  m_IndexBuffer->Upload(indices.data());

  return true;
}

bool Mesh::DoUnLoad() {
  if (IsLoaded()) {
    CleanUp();
  }

  return true;
}
} // namespace Inferno
