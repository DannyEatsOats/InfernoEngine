#include <algorithm>
#include <cstdlib>
#include <pch.h>

#include "EditorSystem.h"
#include "Inferno/Core/Log.h"
#include "Inferno/ECS/Component.h"
#include "Inferno/ECS/Entity.h"
#include "Inferno/Events/KeyCodes.h"
#include "Inferno/Events/KeyEvent.h"
#include "Inferno/Events/MouseEvent.h"
#include "Inferno/Renderer/Renderer.h"
#include "Inferno/UI/ComponentUI.h"
#include "glm/ext.hpp"
#include "glm/ext/quaternion_trigonometric.hpp"
#include "glm/ext/vector_float3.hpp"
#include "glm/fwd.hpp"
#include "glm/geometric.hpp"
#include "glm/gtc/quaternion.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "glm/trigonometric.hpp"
#include "imgui.h"
#include <ImGuizmo.h>

namespace Inferno {
void EditorSystem::StartUp(Renderer *renderer) { m_Renderer = renderer; }

void EditorSystem::ShutDown() {}

void EditorSystem::OnEvent(Event &event) {
  if (event.IsHandled()) {
    return;
  }

  EventDispatcher dispatcher(event);
  dispatcher.Dispatch<MouseButtonPressedEvent>(
      [this](MouseButtonPressedEvent &event) {
        if (event.GetMouseButton() == ENGINE_MOUSE_BUTTON_LEFT) {
          ImGuiIO &io = ImGui::GetIO();
          if (io.WantCaptureMouse) {
            return true;
          }

          if (ImGuizmo::IsOver() || ImGuizmo::IsUsing()) {
            return true;
          }

          auto optionalEntity =
              m_Renderer->PickEntity(static_cast<int32_t>(event.GetMouseX()),
                                     static_cast<int32_t>(event.GetMouseY()));

          if (optionalEntity.has_value()) {
            m_SelectedEntityID = optionalEntity.value();

            return true;
          } else {
            m_SelectedEntityID = Entity::NULL_ENTITY;
          }
        }

        return false;
      });

  dispatcher.Dispatch<KeyPressedEvent>([this](KeyPressedEvent &event) {
    if (!ImGui::GetIO().WantTextInput && !ImGuizmo::IsUsing()) {
      switch (event.GetKeyCode()) {
      case ENGINE_KEY_1:
        m_GizmoOperation = GIZMO_OPERATION::TRANSLATE;
        return true;
      case ENGINE_KEY_2:
        m_GizmoOperation = GIZMO_OPERATION::ROTATE;
        return true;
      case ENGINE_KEY_3:
        m_GizmoOperation = GIZMO_OPERATION::SCALE;
        return true;
      default:
        return false;
      }
    }

    return false;
  });
}

void EditorSystem::Update(DeltaTime deltaTime,
                          const std::vector<Entity *> &entities,
                          const RenderCamera &camera) {
  ImGui::ShowDemoWindow();

  DrawSceneHierarchy(entities);

  if (m_SelectedEntityID == Entity::NULL_ENTITY)
    return;

  auto entity =
      std::find_if(entities.begin(), entities.end(), [&](Entity *entity) {
        return entity->GetID() == m_SelectedEntityID;
      });

  if (entity == entities.end()) {
    m_TransformEditorStates.erase(m_SelectedEntityID);
    m_SelectedEntityID = Entity::NULL_ENTITY;
    return;
  }

  // We Have the right entity selected

  DrawTransformGizmos(*entity, camera);
  DrawComponentsPantel(*entity);
}

void EditorSystem::AppendDebugLines(
    const std::vector<Entity *> &entities,
    std::vector<DebugLineVertex> &debugLines) const {
  if (m_SelectedEntityID == Entity::NULL_ENTITY)
    return;

  auto entityIt =
      std::find_if(entities.begin(), entities.end(), [this](Entity *entity) {
        return entity->GetID() == m_SelectedEntityID;
      });

  if (entityIt == entities.end())
    return;

  auto *camera = (*entityIt)->GetComponent<CameraComponent>();
  if (!camera)
    return;

  const glm::mat4 view = camera->GetViewMatrix();
  const glm::mat4 projection = camera->GetProjectionMatrix();
  const glm::mat4 inverseViewProjection = glm::inverse(projection * view);

  constexpr float visualizationDistance = 2.0f;
  const glm::vec4 projectedDistance =
      projection *
      glm::vec4(0.0f, 0.0f, -visualizationDistance, 1.0f);
  const float farNdcDepth = projectedDistance.z / projectedDistance.w;

  const std::array<glm::vec4, 8> ndcCorners = {
      glm::vec4{-1.0f, -1.0f, 0.0f, 1.0f},
      glm::vec4{1.0f, -1.0f, 0.0f, 1.0f},
      glm::vec4{1.0f, 1.0f, 0.0f, 1.0f},
      glm::vec4{-1.0f, 1.0f, 0.0f, 1.0f},
      glm::vec4{-1.0f, -1.0f, farNdcDepth, 1.0f},
      glm::vec4{1.0f, -1.0f, farNdcDepth, 1.0f},
      glm::vec4{1.0f, 1.0f, farNdcDepth, 1.0f},
      glm::vec4{-1.0f, 1.0f, farNdcDepth, 1.0f},
  };

  std::array<glm::vec3, 8> corners;
  for (size_t i = 0; i < corners.size(); ++i) {
    const glm::vec4 world = inverseViewProjection * ndcCorners[i];
    corners[i] = glm::vec3(world) / world.w;
  }

  constexpr uint32_t edges[][2] = {
      {0, 1}, {1, 2}, {2, 3}, {3, 0},
      {4, 5}, {5, 6}, {6, 7}, {7, 4},
      {0, 4}, {1, 5}, {2, 6}, {3, 7},
  };

  constexpr glm::vec4 color{1.0f, 1.0f, 0.0f, 1.0f};
  debugLines.reserve(debugLines.size() + std::size(edges) * 2);

  for (const auto &[a, b] : edges) {
    debugLines.push_back({.Position = corners[a], .Color = color});
    debugLines.push_back({.Position = corners[b], .Color = color});
  }
}

void EditorSystem::DrawSceneHierarchy(const std::vector<Entity *> &entities) {
  ImGui::Begin("Scene Hierarchy");

  for (auto entity : entities) {
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth |
                               ImGuiTreeNodeFlags_OpenOnArrow |
                               ImGuiTreeNodeFlags_OpenOnDoubleClick;

    if (m_SelectedEntityID == entity->GetID()) {
      flags |= ImGuiTreeNodeFlags_Selected;
    }

    ImGui::PushID(entity->GetID());
    bool isOpen = ImGui::TreeNodeEx(entity->GetName().data(), flags);
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
      m_SelectedEntityID = entity->GetID();
    }

    if (isOpen) {
      ImGui::TreePop();
    }
    ImGui::PopID();
  }

  ImGui::End();
}

void EditorSystem::DrawComponentsPantel(Entity *entity) {
  ImGui::Begin("Components");

  // Drawing Components
  if (auto *transform = entity->GetComponent<TransformComponent>()) {
    UI::DrawTransformComponent(transform,
                               m_TransformEditorStates[m_SelectedEntityID]);
  }

  if (auto *camera = entity->GetComponent<CameraComponent>()) {
    UI::DrawCameraComponent(camera);
  }

  ImGui::End();
}

void EditorSystem::DrawTransformGizmos(Entity *entity,
                                       const RenderCamera &camera) {
  if (!entity)
    return;

  auto *transform = entity->GetComponent<TransformComponent>();
  if (!transform) {
    INFERNO_LOG_WARN("Entity {} {} has no TransformComponent, wth.",
                     entity->GetID(), entity->GetName());
    return;
  }

  ImGuizmo::OPERATION operation = ImGuizmo::TRANSLATE;

  switch (m_GizmoOperation) {
  case Inferno::GIZMO_OPERATION::TRANSLATE:
    operation = ImGuizmo::TRANSLATE;
    break;
  case Inferno::GIZMO_OPERATION::ROTATE:
    operation = ImGuizmo::ROTATE;
    break;
  case Inferno::GIZMO_OPERATION::SCALE:
    operation = ImGuizmo::SCALE;
    break;
  }

  ImGuizmo::MODE mode =
      (m_GizmoSpace == GIZMO_SPACE::LOCAL) ? ImGuizmo::LOCAL : ImGuizmo::WORLD;

  // World scaling is not that useful so we are keeping it at LOCAL. Might
  // change later IDK
  if (operation == ImGuizmo::SCALE) {
    mode = ImGuizmo::LOCAL;
  }

  glm::mat4 view = camera.View;
  glm::mat4 proj = camera.Proj;
  glm::mat4 model = transform->GetTransformmatrix();

  // ImGuizmo does internal flipping already so we gotta flip back (then flip
  // again in Renderer lmao)
  proj[1][1] *= -1;

  ImGuiViewport *viewport = ImGui::GetMainViewport();

  ImGuizmo::SetOrthographic(false);
  ImGuizmo::SetDrawlist(ImGui::GetForegroundDrawList());

  ImGuizmo::SetRect(viewport->Pos.x, viewport->Pos.y, viewport->Size.x,
                    viewport->Size.y);

  const bool modified =
      ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj),
                           operation, mode, glm::value_ptr(model));

  if (!modified)
    return;

  glm::vec3 position;
  glm::quat rotation;
  glm::vec3 scale;
  glm::vec3 skew;
  glm::vec4 perspective;

  if (!glm::decompose(model, scale, rotation, position, skew, perspective)) {
    return;
  }

  transform->SetPosition(position);
  transform->SetRotation(glm::normalize(rotation));
  transform->SetScale(scale);
}
} // namespace Inferno
