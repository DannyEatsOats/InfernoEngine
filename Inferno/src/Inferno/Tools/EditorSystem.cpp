#include <algorithm>
#include <cstdlib>
#include <pch.h>

#include "EditorSystem.h"
#include "Inferno/ECS/Entity.h"
#include "Inferno/Events/KeyCodes.h"
#include "Inferno/Events/MouseEvent.h"
#include "Inferno/Renderer/Renderer.h"
#include "Inferno/UI/ComponentUI.h"
#include "glm/ext/quaternion_trigonometric.hpp"
#include "glm/ext/vector_float3.hpp"
#include "glm/fwd.hpp"
#include "glm/geometric.hpp"
#include "glm/gtc/quaternion.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "glm/trigonometric.hpp"
#include <imgui.h>

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
}

void EditorSystem::Update(DeltaTime deltaTime,
                          const std::vector<Entity *> &entities) {
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

  DrawComponentsPantel(*entity);
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

  if (auto *transform = entity->GetComponent<TransformComponent>()) {
    UI::DrawTransformComponent(transform,
                               m_TransformEditorStates[m_SelectedEntityID]);
  }

  ImGui::End();
}
} // namespace Inferno
