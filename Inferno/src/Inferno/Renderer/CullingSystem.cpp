#include <pch.h>

#include "CullingSystem.h"
#include "Inferno/Core/Log.h"
#include "Inferno/ECS/Component.h"
#include "Inferno/Renderer/BoundingBox.h"
#include "Inferno/Renderer/Mesh.h"

namespace Inferno {
void CullingSystem::CullScene(const std::vector<Entity *> &allEntities) {
  m_VisibleEntites.clear();

  if (!m_Camera) {
    INFERNO_LOG_WARN("Camera is nullptr in CullingSystem");
    return;
  }

  const auto &frustum = m_Camera->GetFrustum();

  for (auto &entity : allEntities) {
    if (!entity->IsActive())
      continue;

    MeshComponent *meshComponent = entity->GetComponent<MeshComponent>();
    if (!meshComponent)
      continue;

    TransformComponent *transformComponent =
        entity->GetComponent<TransformComponent>();
    if (!transformComponent)
      continue;

    auto &localBounds = meshComponent->GetMesh()->GetBoundingBox();

    BoundingBox worldBounds = BoundingBox::Transform(
        localBounds, transformComponent->GetTransformmatrix());

    if (frustum.Intersects(worldBounds)) {
      m_VisibleEntites.push_back(entity);
    }
  }
}
} // namespace Inferno
