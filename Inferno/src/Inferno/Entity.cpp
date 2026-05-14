#include <pch.h>

#include "Entity.h"

namespace Inferno {

void Entity::Initialize() {
  for (auto &component : m_Components) {
    component->Initialize();
  }
}

void Entity::Update(DeltaTime deltTime) {
  if (!m_Active)
    return;

  for (auto &component : m_Components) {
    component->Update(deltTime);
  }
}

void Entity::Render() {
  if (!m_Active)
    return;

  for (auto &component : m_Components) {
    component->Render();
  }
}
} // namespace Inferno
