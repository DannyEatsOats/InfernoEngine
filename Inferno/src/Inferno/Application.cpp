#include "Application.h"

#include "GLFW/glfw3.h"
#include "Inferno/Core/DeltaTime.h"
#include "Inferno/Events/ApplicationEvent.h"
#include "Inferno/Events/Event.h"
#include "Inferno/LayerStack.h"
#include "Inferno/Renderer/RenderingContext.h"
#include "Inferno/Window.h"
#include "Log.h"
#include <memory>

namespace Inferno {
Application::Application() { StartUp(); }

void Application::StartUp() {
  Log::Init();
  INFERNO_LOG_INFO("Starting Up Engine...");

  m_Window = std::unique_ptr<Window>(Window::Create());
  m_Window->SetEventCallback([this](Event &event) { this->OnEvent(event); });
  // TODO Init subsystems
  m_Renderer = std::make_unique<Renderer>();
  m_Renderer->Init();
}

void Application::ShutDown() {
  INFERNO_LOG_INFO("Shutting Down Engine...");
  m_Renderer->ShutDown();
  RenderingContext::GetContext()->ShutDown();
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

      // TODO GUI Layer Stuff
      m_Renderer->DrawFrame();
    }
    // TODO Gui end
    // TODO window stuff

    m_Window->OnUpdate();
  }
}

void Application::OnEvent(Event &event) {
  INFERNO_LOG_INFO("Event: {}", event.ToString());
  EventDispatcher dispatcher(event);
  dispatcher.Dispatch<WindowCloseEvent>(
      [this](WindowCloseEvent &event) { return this->OnWindowClosed(event); });
  dispatcher.Dispatch<WindowResizeEvent>(
      [this](WindowResizeEvent &event) { return this->OnWindowResize(event); });

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
  m_Renderer->OnWindowResize(event.GetWidth(), event.GetHeight());

  return false;
}

} // namespace Inferno
