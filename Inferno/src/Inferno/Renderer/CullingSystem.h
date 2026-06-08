#pragma once

#include "Inferno/ECS/Entity.h"
#include "Inferno/Renderer/Camera.h"
#include <vector>
#include <vulkan/vulkan_core.h>

namespace Inferno {
class CullingSystem {
public:
  explicit CullingSystem(Camera *camera) : m_Camera(camera) {}

  CullingSystem() = default;
  CullingSystem(const CullingSystem &) = delete;
  CullingSystem(CullingSystem &&) = default;

  CullingSystem &operator=(const CullingSystem &) = delete;
  CullingSystem &operator=(CullingSystem &&) = default;

  void SetCamera(Camera *camera) { m_Camera = camera; }
  const std::vector<Entity *> &GetVisibleEntities() const {
    return m_VisibleEntites;
  }

  void CullScene(const std::vector<Entity *> &allEntities);

private:
  Camera *m_Camera = nullptr;
  std::vector<Entity *> m_VisibleEntites;
};
} // namespace Inferno
