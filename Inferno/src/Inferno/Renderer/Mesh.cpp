#include <pch.h>

#include "Mesh.h"

namespace Inferno {

bool Mesh::DoLoad() {
  // TODO: Create canonical path
  std::string filePath = "models/" + GetID() + ".gltf";

  std::vector<MeshVertex> vertices;
  std::vector<uint32_t> indices;
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
    m_IndexBuffer.reset();
    m_VertexBuffer.reset();
  }

  return true;
}
} // namespace Inferno
