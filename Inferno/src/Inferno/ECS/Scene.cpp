#include "Scene.h"
#include <pch.h>
#include <tuple>
#include <utility>

namespace Inferno {
void Scene::OnUpdate(DeltaTime deltaTime) {
  // TODO: UPDATE SCRIPT COMPONENTS HERE
  for (auto &entity : m_Entities) {
  }
}

Entity *Scene::CreateEntity(const std::string &id) {
  auto entity = new Entity(id);
  std::ignore = entity->AddComponent<TransformComponent>();
  m_Entities.push_back(std::move(entity));

  return entity;
}
} // namespace Inferno
