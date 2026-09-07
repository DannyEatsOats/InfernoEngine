#include <pch.h>

#include "EditorSystem.h"
#include "Inferno/Core/Log.h"
#include "Inferno/Events/KeyCodes.h"
#include "Inferno/Events/MouseEvent.h"
#include "Inferno/Renderer/Renderer.h"

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
          auto optionalEntity =
              m_Renderer->PickEntity(static_cast<int32_t>(event.GetMouseX()),
                                     static_cast<int32_t>(event.GetMouseY()));

          if (optionalEntity.has_value()) {
            m_SelectedEntityID = optionalEntity.value();
            INFERNO_LOG_INFO("Optional Entity: {}", m_SelectedEntityID);

            return true;
          }
        }

        return false;
      });
}

void EditorSystem::Update(DeltaTime deltaTime) {}
} // namespace Inferno
