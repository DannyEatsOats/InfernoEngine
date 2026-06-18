#include "Application.h"

#include "GLFW/glfw3.h"
#include "Inferno/Core/Memory.h"
#include "Inferno/ECS/Entity.h"
#include "Inferno/Events/ApplicationEvent.h"
#include "Inferno/Events/Event.h"
#include "Inferno/Events/Input.h"
#include "Inferno/Events/KeyCodes.h"
#include "Inferno/Events/KeyEvent.h"
#include "Inferno/Utils/DeltaTime.h"
#include "Log.h"
#include "glm/ext/quaternion_trigonometric.hpp"
#include "glm/ext/vector_float3.hpp"
#include "glm/fwd.hpp"
#include "glm/trigonometric.hpp"
#include <memory>

namespace Inferno {
Application::Application() { StartUp(); }

void Application::StartUp() {
  Log::Init();
  INFERNO_LOG_INFO("Starting Up Engine...");

  m_Window = Window::Create();
  m_Window->SetEventCallback([this](Event &event) { this->OnEvent(event); });
  m_RenderingContext = MakeScope<DeviceContext>();
  m_RenderingContext->StartUp(m_Window->GetNativeWindow());
  m_ResourceManager = MakeScope<ResourceManager>(m_RenderingContext.get());
  m_Renderer = std::make_unique<Renderer>(m_RenderingContext.get());
  m_Renderer->StartUp(m_ResourceManager.get());
  Input::SetWindowHandle(m_Window->GetNativeWindow());
}

void Application::ShutDown() {
  INFERNO_LOG_INFO("Shutting Down Engine...");
  m_ActiveScene->OnDetach();

  m_Renderer->ShutDown();
  m_ResourceManager->UnloadAll();
  m_RenderingContext->ShutDown();
  m_Running = false;
}

void Application::Run() {
  while (m_Running) {
    const float time = static_cast<float>(glfwGetTime());
    const DeltaTime deltaTime = time - m_LastFrameTime;
    m_LastFrameTime = time;

    INFERNO_LOG_INFO("FPS: {}", 1.0f / deltaTime.GetSeconds());

    if (!m_Minimized) {
      for (Layer *layer : m_LayerStack) {
        layer->OnUpdate(deltaTime);
      }

      if (m_ActiveScene) {
        m_ActiveScene->OnUpdate(deltaTime);
      }

      // TODO GUI Layer Stuff
      m_Renderer->Render(m_ActiveScene->GetEntities());
    }
    // TODO Gui end
    // TODO window stuff

    m_Window->OnUpdate();
  }
  ShutDown();
}

void Application::OnEvent(Event &event) {
  INFERNO_LOG_INFO("Event: {}", event.ToString());
  EventDispatcher dispatcher(event);
  dispatcher.Dispatch<WindowCloseEvent>(
      [this](WindowCloseEvent &event) { return this->OnWindowClosed(event); });
  dispatcher.Dispatch<WindowResizeEvent>(
      [this](WindowResizeEvent &event) { return this->OnWindowResize(event); });
  dispatcher.Dispatch<SetLightingDebugModeEvent>(
      [this](SetLightingDebugModeEvent &event) {
        m_Renderer->SetLightingDebugMode(event.Mode);
        return true;
      });

  // TODO: Input Testing
  dispatcher.Dispatch<KeyPressedEvent>([this](KeyPressedEvent &event) {
    for (auto &entity : m_ActiveScene->GetEntities()) {
      auto position = entity->GetComponent<TransformComponent>()->GetPosition();
      auto rotation = entity->GetComponent<TransformComponent>()->GetRotation();

      if (event.GetKeyCode() == ENGINE_KEY_LEFT) {
        position = position + glm::vec3(-0.1f, 0.0f, 0.0f);
        entity->GetComponent<TransformComponent>()->SetPosition(position);
      } else if (event.GetKeyCode() == ENGINE_KEY_RIGHT) {
        position = position + glm::vec3(0.1f, 0.0f, 0.0f);
        entity->GetComponent<TransformComponent>()->SetPosition(position);
      } else if (event.GetKeyCode() == ENGINE_KEY_UP) {
        position = position + glm::vec3(0.0f, 0.1f, 0.0f);
        entity->GetComponent<TransformComponent>()->SetPosition(position);
      } else if (event.GetKeyCode() == ENGINE_KEY_DOWN) {
        position = position + glm::vec3(0.0f, -0.1f, 0.0f);
        entity->GetComponent<TransformComponent>()->SetPosition(position);

      } else if (event.GetKeyCode() == ENGINE_KEY_F) {
        glm::quat rotationInc =
            glm::angleAxis(glm::radians(-1.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        glm::quat newRotation = rotationInc * rotation;
        entity->GetComponent<TransformComponent>()->SetRotation(newRotation);
      } else if (event.GetKeyCode() == ENGINE_KEY_G) {
        glm::quat rotationInc =
            glm::angleAxis(glm::radians(1.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        glm::quat newRotation = rotationInc * rotation;
        entity->GetComponent<TransformComponent>()->SetRotation(newRotation);
      } else if (event.GetKeyCode() == ENGINE_KEY_H) {
        glm::quat rotationInc =
            glm::angleAxis(glm::radians(-1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        glm::quat newRotation = rotationInc * rotation;
        entity->GetComponent<TransformComponent>()->SetRotation(newRotation);
      } else if (event.GetKeyCode() == ENGINE_KEY_J) {
        glm::quat rotationInc =
            glm::angleAxis(glm::radians(1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        glm::quat newRotation = rotationInc * rotation;
        entity->GetComponent<TransformComponent>()->SetRotation(newRotation);
      } else if (event.GetKeyCode() == ENGINE_KEY_K) {
        glm::quat rotationInc =
            glm::angleAxis(glm::radians(1.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        glm::quat newRotation = rotationInc * rotation;
        entity->GetComponent<TransformComponent>()->SetRotation(newRotation);
      } else if (event.GetKeyCode() == ENGINE_KEY_L) {
        glm::quat rotationInc =
            glm::angleAxis(glm::radians(-1.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        glm::quat newRotation = rotationInc * rotation;
        entity->GetComponent<TransformComponent>()->SetRotation(newRotation);
      }
    }

    return true;
  });
  // ============================================================================

  for (auto it = m_LayerStack.end(); it != m_LayerStack.begin();) {
    (*--it)->OnEvent(event);
    if (event.IsHandled())
      break;
  }
}

void Application::PushLayer(Layer *layer) {
  layer->SetEventCallback([this](Event &e) { this->OnEvent(e); });
  m_LayerStack.PushLayer(layer);
  layer->OnAttach();
}

void Application::PushOverlay(Layer *layer) {
  layer->SetEventCallback([this](Event &e) { this->OnEvent(e); });
  m_LayerStack.PushOverlay(layer);
  layer->OnAttach();
}

void Application::SetActiveScene(Scope<Scene> scene) {
  m_ActiveScene = std::move(scene);
  m_ActiveScene->SetResourceManager(m_ResourceManager.get());
  m_ActiveScene->OnAttach();
}

void Application::SwitchScene(Scope<Scene> scene) { m_ActiveScene->OnDetach(); }

bool Application::OnWindowClosed(WindowCloseEvent &event) {
  m_Running = false;
  return true;
}

bool Application::OnWindowResize(WindowResizeEvent &event) {
  if (event.GetWidth() == 0 || event.GetHeight() == 0) {
    m_Minimized = true;
    return false;
  }
  m_Minimized = false;
  m_Renderer->SignalResize();

  return false;
}

} // namespace Inferno
