#pragma once

#include "Inferno/Renderer/Buffer.h"
#include "glm/glm.hpp"

namespace Inferno {

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

struct DebugLineVertex {
  glm::vec3 Position;
  glm::vec4 Color;

  static BufferLayout<DebugLineVertex> GetLayout() {
    return {
        {"a_Position", ShaderDataType::Float3,
         offsetof(DebugLineVertex, Position)},
        {"a_Color", ShaderDataType::Float4, offsetof(DebugLineVertex, Color)},
    };
  };
};
} // namespace Inferno
