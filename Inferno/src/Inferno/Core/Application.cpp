#include "Application.h"

#include "GLFW/glfw3.h"
#include "Inferno/Core/Memory.h"
#include "Inferno/Events/ApplicationEvent.h"
#include "Inferno/Events/Event.h"
#include "Inferno/Events/Input.h"
#include "Inferno/Utils/DeltaTime.h"
#include "Log.h"

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
  m_Renderer = MakeScope<Renderer>(m_RenderingContext.get());
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

  for (auto it = m_LayerStack.end(); it != m_LayerStack.begin();) {
    (*--it)->OnEvent(event);
    if (event.IsHandled())
      break;
  }

  if (!event.IsHandled() && m_ActiveScene) {
    m_ActiveScene->OnEvent(event);
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
  if (m_ActiveScene) {
    m_ActiveScene->OnDetach();
  }

  m_ActiveScene = std::move(scene);

  m_ActiveScene->SetResourceManager(m_ResourceManager.get());
  m_ActiveScene->SetEventCallback([this](Event &e) { this->OnEvent(e); });
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
