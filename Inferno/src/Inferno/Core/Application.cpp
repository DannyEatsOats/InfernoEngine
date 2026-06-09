#include "Application.h"

#include "GLFW/glfw3.h"
#include "Inferno/Core/Memory.h"
#include "Inferno/ECS/Entity.h"
#include "Inferno/Events/KeyCodes.h"
#include "Inferno/Events/KeyEvent.h"
#include "Inferno/Renderer/Mesh.h"
#include "Inferno/Utils/DeltaTime.h"
#include "Log.h"
#include "glm/ext/vector_float3.hpp"
#include <algorithm>
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
  // TODO Init subsystems
  m_ResourceManager = MakeScope<ResourceManager>();
  m_RenderingContext = MakeScope<DeviceContext>();
  m_RenderingContext->StartUp(m_Window->GetNativeWindow());
  m_Renderer = std::make_unique<Renderer>(m_RenderingContext.get());
  m_Renderer->StartUp(m_ResourceManager.get());

  // TEMP

  for (int i = 0; i < 10; ++i) {
    for (int j = 0; j < 10; ++j) {
      for (int k = 0; k < 1; ++k) {
        Entity *testEntity =
            new Entity("test" + std::to_string(i) + std::to_string(j));
        testEntity->AddComponent<TransformComponent>();
        testEntity->GetComponent<TransformComponent>()->SetPosition(glm::vec3(
            -1.0f + (i * 1.0f), 0.5f - (j * 1.0f), -1.0f - (k * 0.1f)));
        testEntity->GetComponent<TransformComponent>()->SetScale(
            glm::vec3(0.5f, 0.5f, 0.5f));
        auto mesh =
            m_ResourceManager->Load<Mesh>("testMesh", m_RenderingContext.get());
        testEntity->AddComponent<MeshComponent>(mesh.get(), nullptr);
        m_Entities.push_back(testEntity);
      }
    }
  }

  /*
  for (int i = 0; i < 100; ++i) {
    for (int j = 0; j < 50; ++j) {
      for (int k = 0; k < 1; ++k) {
        Entity *testEntity =
            new Entity("test" + std::to_string(i) + std::to_string(j));
        testEntity->AddComponent<TransformComponent>();
        testEntity->GetComponent<TransformComponent>()->SetPosition(glm::vec3(
            -4.0f + (i * 0.1f), 1.2f - (j * 0.1f), -3.0f - (k * 0.1f)));
        testEntity->GetComponent<TransformComponent>()->SetScale(
            glm::vec3(0.05f, 0.05f, 0.05f));
        auto mesh =
            m_ResourceManager->Load<Mesh>("testMesh", m_RenderingContext.get());
        testEntity->AddComponent<MeshComponent>(mesh.get(), nullptr);
        m_Entities.push_back(testEntity);
      }
    }
  }
  */
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

    if (!m_Minimized) {
      for (Layer *layer : m_LayerStack) {
        layer->OnUpdate(deltaTime);
      }

      for (auto entity : m_Entities) {
      }

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
  // m_Renderer->OnWindowResize(event.GetWidth(), event.GetHeight());

  return false;
}

} // namespace Inferno
