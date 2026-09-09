#pragma once

#include "Inferno/ECS/Entity.h"
#include "Inferno/Events/Event.h"
#include "Inferno/Renderer/Renderer.h"
#include "Inferno/Utils/DeltaTime.h"

namespace Inferno {
class EditorSystem {
public:
  EditorSystem() = default;
  ~EditorSystem() = default;

  void StartUp(Renderer *renderer);
  void ShutDown();

  void OnEvent(Event &event);
  void Update(DeltaTime deltaTime);

  uint32_t GetSelectedEntity() { return m_SelectedEntityID; }

private:
  const Renderer *m_Renderer = nullptr;
  uint32_t m_SelectedEntityID = Entity::NULL_ENTITY;
};
} // namespace Inferno
