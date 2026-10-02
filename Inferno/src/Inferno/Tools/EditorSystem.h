#pragma once

#include "Inferno/ECS/Component.h"
#include "Inferno/ECS/Entity.h"
#include "Inferno/Events/Event.h"
#include "Inferno/Renderer/Renderer.h"
#include "Inferno/Utils/DeltaTime.h"
#include <unordered_map>

namespace Inferno {
enum class GIZMO_OPERATION {
  TRANSLATE,
  ROTATE,
  SCALE,
};

enum class GIZMO_SPACE { LOCAL, WORLD };

struct TransformEditorState {
  glm::vec3 EulerDegrees{0.0f};
  glm::quat LastRotation{1.0f, 0.0f, 0.0f, 0.0f};
  bool Initialized = false;
  bool Editing = false;
};

class EditorSystem {
public:
  EditorSystem() = default;
  ~EditorSystem() = default;

  void StartUp(Renderer *renderer);
  void ShutDown();

  void OnEvent(Event &event);
  void Update(DeltaTime deltaTime, const std::vector<Entity *> &entities,
              const RenderCamera &camera);

  uint32_t GetSelectedEntity() { return m_SelectedEntityID; }

  // INFO: Temp
  void DrawSceneHierarchy(const std::vector<Entity *> &entities);
  void DrawComponentsPantel(Entity *entity);

  void DrawTransformGizmos(Entity *entity, const RenderCamera &camera);

private:
  const Renderer *m_Renderer = nullptr;
  uint32_t m_SelectedEntityID = Entity::NULL_ENTITY;

  std::unordered_map<uint32_t, TransformEditorState> m_TransformEditorStates;

  GIZMO_OPERATION m_GizmoOperation = GIZMO_OPERATION::TRANSLATE;
  GIZMO_SPACE m_GizmoSpace = GIZMO_SPACE::LOCAL;
};
} // namespace Inferno
