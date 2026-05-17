#pragma once

#include "Inferno/Core/Memory.h"
#include "Inferno/Renderer/Buffer.h"
#include "Inferno/Renderer/DeviceContext.h"
#include "Inferno/Resource/Resource.h"

namespace Inferno {
struct MeshVertex {};

class Mesh : public Resource {
private:
  const DeviceContext *context;

  Scope<VertexBuffer<MeshVertex>> m_VertexBuffer;
  Scope<IndexBuffer> m_IndexBuffer;

  uint32_t vertexCount = 0;
  uint32_t indexCount = 0;

public:
  explicit Mesh(const std::string &id) : Resource(id) {}
  ~Mesh() = default;
};
} // namespace Inferno
