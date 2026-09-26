#pragma once

#include "Inferno/ECS/Component.h"
#include "Inferno/ECS/Entity.h"
#include "Inferno/Events/Event.h"
#include "Inferno/Renderer/Renderer.h"
#include "Inferno/Utils/DeltaTime.h"
#include <unordered_map>

namespace Inferno {
class EditorSystem {
public:
  EditorSystem() = default;
  ~EditorSystem() = default;

  void StartUp(Renderer *renderer);
  void ShutDown();

  void OnEvent(Event &event);
  void Update(DeltaTime deltaTime, const std::vector<Entity *> &entities);

  uint32_t GetSelectedEntity() { return m_SelectedEntityID; }

  // INFO: Temp
  void DrawSceneHierarchy(const std::vector<Entity *> &entities);
  void DrawComponentsPantel(Entity *entity);
  void DrawTransformComponent(TransformComponent *transform);

private:
  const Renderer *m_Renderer = nullptr;
  uint32_t m_SelectedEntityID = Entity::NULL_ENTITY;

  struct TransformEditorState {
    glm::vec3 EulerDegrees{0.0f};
    glm::quat LastRotation{1.0f, 0.0f, 0.0f, 0.0f};
    bool Initialized = false;
    bool Editing = false;
  };

  std::unordered_map<uint32_t, TransformEditorState> m_TransformEditorStates;
};
} // namespace Inferno
