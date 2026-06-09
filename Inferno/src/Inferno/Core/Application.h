#pragma once

#include "Inferno/Core/Memory.h"
#include "Inferno/Events/ApplicationEvent.h"
#include "Inferno/Events/Event.h"
#include "Inferno/Renderer/DeviceContext.h"
#include "Inferno/Renderer/Renderer.h"
#include "Inferno/Resource/ResourceManager.h"
#include "Layer.h"
#include "LayerStack.h"
#include "Window.h"
#include <memory>
#include <vector>

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
  LayerStack m_LayerStack;
  Scope<DeviceContext> m_RenderingContext;
  Scope<Renderer> m_Renderer;
  Scope<ResourceManager> m_ResourceManager;
  bool m_Running = true;
  bool m_Minimized = false;
  float m_LastFrameTime = 0.0f;

  // TEMP
  std::vector<Entity *> m_Entities;
};
} // namespace Inferno
