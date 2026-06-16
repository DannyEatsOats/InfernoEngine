#include "Inferno/Core/Log.h"
#include "Inferno/Core/Memory.h"
#include "glm/geometric.hpp"
#include <filesystem>
#include <pch.h>
#include <stdexcept>
#include <vector>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

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
  m_VertexCount = other.m_VertexCount;
  m_IndexCount = other.m_IndexCount;

  other.m_VertexCount = 0;
  other.m_IndexCount = 0;

  return *this;
}

bool Mesh::DoLoad() {
  std::string filePath = "assets/models/" + GetID() + ".obj";

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
  m_VertexBuffer->SetLayout(MeshVertex::GetLayout());

  m_IndexBuffer = MakeScope<IndexBuffer>(
      m_Context, sizeof(indices[0]) * m_IndexCount, VK_INDEX_TYPE_UINT32);

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
                        std::vector<uint32_t> &indexBuffer) {
  std::filesystem::path exePath =
      std::filesystem::canonical("/proc/self/exe").parent_path();
  std::filesystem::path fullPath = exePath / filePath;

  tinyobj::attrib_t attrib;
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;
  std::string warn, err;

  if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err,
                        fullPath.string().c_str())) {
    throw std::runtime_error("OBJ LOADING ERROR: " + warn + " " + err);
  }

  std::unordered_map<MeshVertex, uint32_t> uniqueVertices{};

  for (const auto &shape : shapes) {
    for (const auto &index : shape.mesh.indices) {
      MeshVertex vertex{};

      float rawX = attrib.vertices[3 * index.vertex_index + 0];
      float rawY = attrib.vertices[3 * index.vertex_index + 1];
      float rawZ = attrib.vertices[3 * index.vertex_index + 2];

      float currentX = rawX;
      float currentY = rawZ;
      float currentZ = -rawY;

      vertex.Position = {-currentZ, currentY, currentX};

      if (!attrib.normals.empty() && index.normal_index >= 0) {
        float nX = attrib.normals[3 * index.normal_index + 0];
        float nY = attrib.normals[3 * index.normal_index + 1];
        float nZ = attrib.normals[3 * index.normal_index + 2];

        float currentNX = nX;
        float currentNY = nZ;
        float currentNZ = -nY;

        vertex.Normal =
            glm::normalize(glm::vec3(-currentNZ, currentNY, currentNX));

      } else {
        vertex.Normal = {0.0f, 1.0f, 0.0f};
      }
      vertex.Color = {0.5f, 0.5f, 0.5f};

      if (!attrib.texcoords.empty() && index.texcoord_index >= 0) {
        vertex.TexCoord = {
            attrib.texcoords[2 * index.texcoord_index + 0],
            1.0f - attrib.texcoords[2 * index.texcoord_index + 1],
        };
      } else {
        vertex.TexCoord = {0.0f, 0.0f};
      }
      vertex.Color = {1.0f, 1.0f, 1.0f};

      auto [it, inserted] = uniqueVertices.insert(
          {vertex, static_cast<uint32_t>(vertexBuffer.size())});

      if (inserted) {
        vertexBuffer.push_back(vertex);
      }

      indexBuffer.push_back(it->second);
    }
  }

  INFERNO_LOG_INFO("Mesh {} VERTEX COUNT: {}", filePath, vertexBuffer.size());

  return true;
}

void Mesh::SetGeometryData(std::vector<MeshVertex>& vertexBufferData,
                           std::vector<uint32_t> &indexBufferData) {
  m_VertexCount = static_cast<uint32_t>(vertexBufferData.size());
  m_IndexCount = static_cast<uint32_t>(indexBufferData.size());

  if (!m_VertexBuffer) {
    m_VertexBuffer = MakeScope<VertexBuffer<MeshVertex>>(
        m_Context, sizeof(MeshVertex) * m_VertexCount);
    m_VertexBuffer->SetLayout(MeshVertex::GetLayout());
  }

  if (!m_IndexBuffer) {
    m_IndexBuffer = MakeScope<IndexBuffer>(
        m_Context, sizeof(indexBufferData[0]) * indexBufferData.size(),
        VK_INDEX_TYPE_UINT32);
  }

  m_VertexBuffer->Upload(vertexBufferData.data());
  m_IndexBuffer->Upload(indexBufferData.data());

  m_Loaded = true;
}
} // namespace Inferno
