#pragma once

#include "glm/ext/matrix_float4x4.hpp"

// ================================================
// TODO: Maybe Camera stuff could be a serparate push constants struct
// ================================================

namespace Inferno {
struct MeshPushConstants {
  glm::mat4 Mvp;
  glm::mat4 Model;
  uint32_t EntityID;
};

struct OutlinePushConstants {
  glm::vec3 Color;
  uint32_t EntityID;
  uint32_t ThicknessPX;
};

struct GridPushConstants {
  glm::mat4 View;
  glm::mat4 Proj;
  glm::mat4 ViewInv;
  glm::mat4 ProjInv;
};

struct GizmoPushConstants {
  glm::mat4 View;
  glm::mat4 Proj;
};
} // namespace Inferno
