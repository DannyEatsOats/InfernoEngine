#pragma once

#include "Inferno/Events/ApplicationEvent.h"
#include "Inferno/Events/Event.h"
#include "Inferno/Layer.h"
#include "Inferno/LayerStack.h"
#include "Inferno/Renderer/Renderer.h"
#include "Inferno/Window.h"
#include <memory>

namespace Inferno {
class Application {
public:
  Application();
  virtual ~Application() = default;

  Application(const Application &) = delete;
  Application &operator=(const Application &) = delete;

  void StartUp();
  void ShutDown();

  void Run();
  void OnEvent(Event &event);
  void PushLayer(Layer *layer);
  void PushOverlay(Layer *layer);

private:
  bool OnWindowClosed(WindowCloseEvent &event);
  bool OnWindowResize(WindowResizeEvent &event);

private:
  std::unique_ptr<Window> m_Window;
  std::shared_ptr<RenderingContext> m_Context;
  std::unique_ptr<Renderer> m_Renderer;
  LayerStack m_LayerStack;
  bool m_Running = true;
  bool m_Minimized = false;
  float m_LastFrameTime = 0.0f;
};
} // namespace Inferno
