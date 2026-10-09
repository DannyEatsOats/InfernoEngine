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

struct alignas(16) DirectionalLightData {
  glm::vec4 Direction{0.0f, -1.0f, 0.0f, 0.0f};
  glm::vec4 ColorIntensity{1.0f, 1.0f, 1.0f, 1.0f};
};

struct alignas(16) PointLightData {
  glm::vec4 PositionRange{0.0f, 0.0f, 0.0f, 10.0f};
  glm::vec4 ColorIntensity{1.0f, 1.0f, 1.0f, 1.0f};
};

struct alignas(16) SpotLightData {
  glm::vec4 PositionRange{0.0f, 0.0f, 0.0f, 10.0f};
  glm::vec4 DirectionInnerConeCos{0.0f, -1.0f, 0.0f, 0.9f};
  glm::vec4 ColorIntensity{1.0f, 1.0f, 1.0f, 1.0f};
  glm::vec4 OuterConeCos{0.8f, 0.0f, 0.0f, 0.0f};
};

static_assert(sizeof(DirectionalLightData) == 32);
static_assert(sizeof(PointLightData) == 32);
static_assert(sizeof(SpotLightData) == 64);

struct RenderWorld {
  std::vector<RenderObject> Objects;
  std::vector<DirectionalLightData> DirectionalLights;
  std::vector<PointLightData> PointLights;
  std::vector<SpotLightData> SpotLights;
};

} // namespace Inferno
