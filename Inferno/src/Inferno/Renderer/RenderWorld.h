#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <vector>

namespace Inferno {

class Mesh;
class Texture;

struct RenderObject {
  glm::mat4 Transform{1.0f};
  const Mesh *MeshResource = nullptr;
  Texture *TextureResource = nullptr;
  uint32_t EntityID = 0;
};

struct RenderWorld {
  std::vector<RenderObject> Objects;
};

} // namespace Inferno
