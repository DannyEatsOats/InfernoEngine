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
  void AppendDebugLines(const std::vector<Entity *> &entities,
                        std::vector<DebugLineVertex> &debugLines) const;

  uint32_t GetSelectedEntity() { return m_SelectedEntityID; }
  GBufferDebugView GetGBufferDebugView() const { return m_GBufferDebugView; }
  float GetExposure() const { return m_Exposure; }

  // INFO: Temp
  void DrawSceneHierarchy(const std::vector<Entity *> &entities);
  void DrawRenderDebugPanel();
  void DrawComponentsPantel(Entity *entity);

  void DrawTransformGizmos(Entity *entity, const RenderCamera &camera);

private:
  const Renderer *m_Renderer = nullptr;
  uint32_t m_SelectedEntityID = Entity::NULL_ENTITY;
  GBufferDebugView m_GBufferDebugView = GBufferDebugView::LIT;
  float m_Exposure = 1.0f;

  std::unordered_map<uint32_t, TransformEditorState> m_TransformEditorStates;

  GIZMO_OPERATION m_GizmoOperation = GIZMO_OPERATION::TRANSLATE;
  GIZMO_SPACE m_GizmoSpace = GIZMO_SPACE::LOCAL;
};
} // namespace Inferno
