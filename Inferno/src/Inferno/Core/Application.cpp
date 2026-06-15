#include "Application.h"

#include "GLFW/glfw3.h"
#include "Inferno/Core/Memory.h"
#include "Inferno/ECS/Entity.h"
#include "Inferno/Events/Input.h"
#include "Inferno/Events/KeyCodes.h"
#include "Inferno/Events/KeyEvent.h"
#include "Inferno/Renderer/Mesh.h"
#include "Inferno/Utils/DeltaTime.h"
#include "Log.h"
#include "glm/ext/quaternion_trigonometric.hpp"
#include "glm/ext/vector_float3.hpp"
#include "glm/fwd.hpp"
#include "glm/trigonometric.hpp"
#include <algorithm>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>

namespace Inferno {
Application::Application() { StartUp(); }

void Application::StartUp() {
  Log::Init();
  INFERNO_LOG_INFO("Starting Up Engine...");

  m_Window = Window::Create();
  m_Window->SetEventCallback([this](Event &event) { this->OnEvent(event); });
  Input::SetWindowHandle(m_Window->GetNativeWindow());
  // TODO Init subsystems
  m_ResourceManager = MakeScope<ResourceManager>();
  m_RenderingContext = MakeScope<DeviceContext>();
  m_RenderingContext->StartUp(m_Window->GetNativeWindow());
  m_Renderer = std::make_unique<Renderer>(m_RenderingContext.get());
  m_Renderer->StartUp(m_ResourceManager.get());

  // TEMP

  {
    Entity *testEntity = new Entity("test");
    testEntity->AddComponent<TransformComponent>();
    // testEntity->GetComponent<TransformComponent>()->SetRotation(
    //    glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f)));
    auto mesh =
        m_ResourceManager->Load<Mesh>("viking_room", m_RenderingContext.get());
    auto texture = m_ResourceManager->Load<Texture>("viking_room",
                                                    m_RenderingContext.get());
    testEntity->AddComponent<MeshComponent>(mesh.get(), texture.get());
    // m_Entities.push_back(testEntity);
  }

  for (int i = 0; i < 1; ++i) {
    Entity *knight = new Entity("knight");
    auto *transform = knight->AddComponent<TransformComponent>();
    // transform->SetRotation(glm::angleAxis(glm::radians(90.0f),
    // glm::vec3(0.0f, 0.0f, 1.0f)));
    // transform->SetRotation(glm::angleAxis(glm::radians(-90.0f),
    // glm::vec3(0.0f, 1.0f, 0.0f)));

    auto mesh =
        m_ResourceManager->Load<Mesh>("zsamo", m_RenderingContext.get());
    auto texture =
        m_ResourceManager->Load<Texture>("zsamo", m_RenderingContext.get());
    knight->AddComponent<MeshComponent>(mesh.get(), texture.get());
    m_Entities.push_back(knight);
  }
}

void Application::ShutDown() {
  INFERNO_LOG_INFO("Shutting Down Engine...");
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

      for (auto entity : m_Entities) {
      }

      // TODO: TEMP ======================================================
      if (Input::IsKeyDown(ENGINE_KEY_LEFT_CONTROL) &&
          Input::IsKeyDown(ENGINE_KEY_GRAVE_ACCENT)) {
        m_Renderer->SetLightingDebugMode(0);
      }
      if (Input::IsKeyDown(ENGINE_KEY_LEFT_CONTROL) &&
          Input::IsKeyDown(ENGINE_KEY_1)) {
        m_Renderer->SetLightingDebugMode(1);
      }
      if (Input::IsKeyDown(ENGINE_KEY_LEFT_CONTROL) &&
          Input::IsKeyDown(ENGINE_KEY_2)) {
        m_Renderer->SetLightingDebugMode(2);
      }
      if (Input::IsKeyDown(ENGINE_KEY_LEFT_CONTROL) &&
          Input::IsKeyDown(ENGINE_KEY_3)) {
        m_Renderer->SetLightingDebugMode(3);
      }
      if (Input::IsKeyDown(ENGINE_KEY_LEFT_CONTROL) &&
          Input::IsKeyDown(ENGINE_KEY_4)) {
        m_Renderer->SetLightingDebugMode(4);
      }
      // ============================================================7=

      // TODO GUI Layer Stuff
      // m_Renderer->DrawFrame();
      m_Renderer->Render(m_Entities);
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

  // Input Testing
  dispatcher.Dispatch<KeyPressedEvent>([this](KeyPressedEvent &event) {
    for (Entity *entity : m_Entities) {
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
  m_LayerStack.PushLayer(layer);
  layer->OnAttach();
}

void Application::PushOverlay(Layer *layer) {
  m_LayerStack.PopOverlay(layer);
  layer->OnAttach();
}

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
