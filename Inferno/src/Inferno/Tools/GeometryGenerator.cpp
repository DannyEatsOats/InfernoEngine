#include "GeometryGenerator.h"
#include <cstdint>
#include <pch.h>
#include <utility>
#include <vector>

#include "Inferno/Core/Memory.h"
#include "Inferno/Renderer/Mesh.h"

namespace Inferno {
uint64_t GeometryGenerator::s_CubeCount = 0;

Scope<Mesh> GeometryGenerator::GenerateCube(const DeviceContext *context) {
  std::vector<MeshVertex> vertices = {
      // Front Face
      {{-0.5f, -0.5f, -0.5f},
       {0.0f, 0.0f, -1.0f},
       {1.0f, 1.0f, 1.0f},
       {0.0f, 1.0f}},

      {{0.5f, -0.5f, -0.5f},
       {0.0f, 0.0f, -1.0f},
       {1.0f, 1.0f, 1.0f},
       {1.0f, 1.0f}},

      {{0.5f, 0.5f, -0.5f},
       {0.0f, 0.0f, -1.0f},
       {1.0f, 1.0f, 1.0f},
       {1.0f, 0.0f}},

      {{-0.5f, 0.5f, -0.5f},
       {0.0f, 0.0f, -1.0f},
       {1.0f, 1.0f, 1.0f},
       {0.0f, 0.0f}},

      // Back face
      {{0.5f, -0.5f, 0.5f},
       {0.0f, 0.0f, 1.0f},
       {1.0f, 1.0f, 1.0f},
       {0.0f, 1.0f}},

      {{-0.5f, -0.5f, 0.5f},
       {0.0f, 0.0f, 1.0f},
       {1.0f, 1.0f, 1.0f},
       {1.0f, 1.0f}},

      {{-0.5f, 0.5f, 0.5f},
       {0.0f, 0.0f, 1.0f},
       {1.0f, 1.0f, 1.0f},
       {1.0f, 0.0f}},

      {{0.5f, 0.5f, 0.5f},
       {0.0f, 0.0f, 1.0f},
       {1.0f, 1.0f, 1.0f},
       {0.0f, 0.0f}},

      // Left face
      {{-0.5f, -0.5f, 0.5f},
       {-1.0f, 0.0f, 0.0f},
       {1.0f, 1.0f, 1.0f},
       {0.0f, 1.0f}},

      {{-0.5f, -0.5f, -0.5f},
       {-1.0f, 0.0f, 0.0f},
       {1.0f, 1.0f, 1.0f},
       {1.0f, 1.0f}},

      {{-0.5f, 0.5f, -0.5f},
       {-1.0f, 0.0f, 0.0f},
       {1.0f, 1.0f, 1.0f},
       {1.0f, 0.0f}},

      {{-0.5f, 0.5f, 0.5f},
       {-1.0f, 0.0f, 0.0f},
       {1.0f, 1.0f, 1.0f},
       {0.0f, 0.0f}},

      // Right face
      {{0.5f, -0.5f, -0.5f},
       {1.0f, 0.0f, 0.0f},
       {1.0f, 1.0f, 1.0f},
       {0.0f, 1.0f}},

      {{0.5f, -0.5f, 0.5f},
       {1.0f, 0.0f, 0.0f},
       {1.0f, 1.0f, 1.0f},
       {1.0f, 1.0f}},

      {{0.5f, 0.5f, 0.5f},
       {1.0f, 0.0f, 0.0f},
       {1.0f, 1.0f, 1.0f},
       {1.0f, 0.0f}},

      {{0.5f, 0.5f, -0.5f},
       {1.0f, 0.0f, 0.0f},
       {1.0f, 1.0f, 1.0f},
       {0.0f, 0.0f}},

      // Top face
      {{-0.5f, 0.5f, -0.5f},
       {0.0f, 1.0f, 0.0f},
       {1.0f, 1.0f, 1.0f},
       {0.0f, 1.0f}},

      {{0.5f, 0.5f, -0.5f},
       {0.0f, 1.0f, 0.0f},
       {1.0f, 1.0f, 1.0f},
       {1.0f, 1.0f}},

      {{0.5f, 0.5f, 0.5f},
       {0.0f, 1.0f, 0.0f},
       {1.0f, 1.0f, 1.0f},
       {1.0f, 0.0f}},

      {{-0.5f, 0.5f, 0.5f},
       {0.0f, 1.0f, 0.0f},
       {1.0f, 1.0f, 1.0f},
       {0.0f, 0.0f}},

      // Bottom face
      {{-0.5f, -0.5f, -0.5f},
       {0.0f, -1.0f, 0.0f},
       {1.0f, 1.0f, 1.0f},
       {0.0f, 1.0f}},

      {{-0.5f, -0.5f, 0.5f},
       {0.0f, -1.0f, 0.0f},
       {1.0f, 1.0f, 1.0f},
       {1.0f, 1.0f}},

      {{0.5f, -0.5f, 0.5f},
       {0.0f, -1.0f, 0.0f},
       {1.0f, 1.0f, 1.0f},
       {1.0f, 0.0f}},

      {{0.5f, -0.5f, -0.5f},
       {0.0f, -1.0f, 0.0f},
       {1.0f, 1.0f, 1.0f},
       {0.0f, 0.0f}},
  };

  std::vector<uint32_t> indices = {
      0,  2,  1,  0,  3,  2,  // Front Face
      4,  6,  5,  4,  7,  6,  // Back Face
      8,  10, 9,  8,  11, 10, // Left face
      12, 14, 13, 12, 15, 14, // Right Face
      16, 18, 17, 16, 19, 18, // Top face
      20, 22, 21, 20, 23, 22  // Bottom face
  };

  Scope<Mesh> mesh = MakeScope<Mesh>("runtime:://cube", context);
  mesh->SetGeometryData(vertices, indices);

  return std::move(mesh);
}
} // namespace Inferno
