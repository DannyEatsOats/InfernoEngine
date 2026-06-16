#pragma once

#include "Inferno/Core/Memory.h"
#include "Inferno/Renderer/DeviceContext.h"
#include "Inferno/Renderer/Mesh.h"
#include <cstdint>

namespace Inferno {
class GeometryGenerator {
public:
  static Scope<Mesh> GenerateCube(const DeviceContext *context);

private:
  static uint64_t s_CubeCount;
};
} // namespace Inferno
