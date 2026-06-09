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

  Resource::operator=(std::move(other));
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
  m_VertexBuffer->SetLayout(MeshVertex::GetLayout());

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

bool Mesh::LoadMeshData(std::string &filePath,
                        std::vector<MeshVertex> &vertexBuffer,
                        std::vector<uint16_t> &indexBuffer) {
  // TODO: TEMP FOR TESTING
  vertexBuffer = {
      // Front Face (Z = 0.5f) - Normal pointing straight out at +Z
      {{-0.5f, -0.5f, 0.5f},
       {0.0f, 0.0f, 1.0f},
       {1.0f, 0.0f, 0.0f},
       {0.0f, 0.0f}},
      {{0.5f, -0.5f, 0.5f},
       {0.0f, 0.0f, 1.0f},
       {1.0f, 0.0f, 0.0f},
       {1.0f, 0.0f}},
      {{0.5f, 0.5f, 0.5f},
       {0.0f, 0.0f, 1.0f},
       {1.0f, 0.0f, 0.0f},
       {1.0f, 1.0f}},
      {{-0.5f, 0.5f, 0.5f},
       {0.0f, 0.0f, 1.0f},
       {1.0f, 0.0f, 0.0f},
       {0.0f, 1.0f}},

      // Back Face (Z = -0.5f) - Normal pointing straight backward at -Z
      {{-0.5f, -0.5f, -0.5f},
       {0.0f, 0.0f, -1.0f},
       {0.0f, 1.0f, 0.0f},
       {1.0f, 0.0f}},
      {{-0.5f, 0.5f, -0.5f},
       {0.0f, 0.0f, -1.0f},
       {0.0f, 1.0f, 0.0f},
       {1.0f, 1.0f}},
      {{0.5f, 0.5f, -0.5f},
       {0.0f, 0.0f, -1.0f},
       {0.0f, 1.0f, 0.0f},
       {0.0f, 1.0f}},
      {{0.5f, -0.5f, -0.5f},
       {0.0f, 0.0f, -1.0f},
       {0.0f, 1.0f, 0.0f},
       {0.0f, 0.0f}},

      // Top Face (Y = 0.5f) - Normal pointing straight up at +Y
      {{-0.5f, 0.5f, -0.5f},
       {0.0f, 1.0f, 0.0f},
       {0.0f, 0.0f, 1.0f},
       {0.0f, 1.0f}},
      {{-0.5f, 0.5f, 0.5f},
       {0.0f, 1.0f, 0.0f},
       {0.0f, 0.0f, 1.0f},
       {0.0f, 0.0f}},
      {{0.5f, 0.5f, 0.5f},
       {0.0f, 1.0f, 0.0f},
       {0.0f, 0.0f, 1.0f},
       {1.0f, 0.0f}},
      {{0.5f, 0.5f, -0.5f},
       {0.0f, 1.0f, 0.0f},
       {0.0f, 0.0f, 1.0f},
       {1.0f, 1.0f}},

      // Bottom Face (Y = -0.5f) - Normal pointing straight down at -Y
      {{-0.5f, -0.5f, -0.5f},
       {0.0f, -1.0f, 0.0f},
       {1.0f, 1.0f, 0.0f},
       {1.0f, 1.0f}},
      {{0.5f, -0.5f, -0.5f},
       {0.0f, -1.0f, 0.0f},
       {1.0f, 1.0f, 0.0f},
       {0.0f, 1.0f}},
      {{0.5f, -0.5f, 0.5f},
       {0.0f, -1.0f, 0.0f},
       {1.0f, 1.0f, 0.0f},
       {0.0f, 0.0f}},
      {{-0.5f, -0.5f, 0.5f},
       {0.0f, -1.0f, 0.0f},
       {1.0f, 1.0f, 0.0f},
       {1.0f, 0.0f}},

      // Right Face (X = 0.5f) - Normal pointing straight right at +X
      {{0.5f, -0.5f, -0.5f},
       {1.0f, 0.0f, 0.0f},
       {1.0f, 0.0f, 1.0f},
       {1.0f, 0.0f}},
      {{0.5f, 0.5f, -0.5f},
       {1.0f, 0.0f, 0.0f},
       {1.0f, 0.0f, 1.0f},
       {1.0f, 1.0f}},
      {{0.5f, 0.5f, 0.5f},
       {1.0f, 0.0f, 0.0f},
       {1.0f, 0.0f, 1.0f},
       {0.0f, 1.0f}},
      {{0.5f, -0.5f, 0.5f},
       {1.0f, 0.0f, 0.0f},
       {1.0f, 0.0f, 1.0f},
       {0.0f, 0.0f}},

      // Left Face (X = -0.5f) - Normal pointing straight left at -X
      {{-0.5f, -0.5f, -0.5f},
       {-1.0f, 0.0f, 0.0f},
       {0.0f, 1.0f, 1.0f},
       {0.0f, 0.0f}},
      {{-0.5f, -0.5f, 0.5f},
       {-1.0f, 0.0f, 0.0f},
       {0.0f, 1.0f, 1.0f},
       {1.0f, 0.0f}},
      {{-0.5f, 0.5f, 0.5f},
       {-1.0f, 0.0f, 0.0f},
       {0.0f, 1.0f, 1.0f},
       {1.0f, 1.0f}},
      {{-0.5f, 0.5f, -0.5f},
       {-1.0f, 0.0f, 0.0f},
       {0.0f, 1.0f, 1.0f},
       {0.0f, 1.0f}}};
  // 12 Triangles (2 per cube face) using standard counter-clockwise rendering
  // layout
  indexBuffer = {
      0,  1,  2,  2,  3,  0,  // Front
      4,  5,  6,  6,  7,  4,  // Back
      8,  9,  10, 10, 11, 8,  // Top
      12, 13, 14, 14, 15, 12, // Bottom
      16, 17, 18, 18, 19, 16, // Right
      20, 21, 22, 22, 23, 20  // Left
  };
  return true;
}
} // namespace Inferno
