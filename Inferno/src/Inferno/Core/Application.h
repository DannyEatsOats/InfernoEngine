#pragma once

#include "Inferno/Core/Memory.h"
#include "Inferno/ECS/Scene.h"
#include "Inferno/Events/ApplicationEvent.h"
#include "Inferno/Events/Event.h"
#include "Inferno/Renderer/DeviceContext.h"
#include "Inferno/Renderer/Renderer.h"
#include "Inferno/Resource/ResourceManager.h"
#include "Layer.h"
#include "LayerStack.h"
#include "Window.h"
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

  void SetActiveScene(Scope<Scene> scene);
  void SwitchScene(Scope<Scene> scene);

private:
  bool OnWindowClosed(WindowCloseEvent &event);
  bool OnWindowResize(WindowResizeEvent &event);

private:
  std::unique_ptr<Window> m_Window;
  LayerStack m_LayerStack;
  Scope<DeviceContext> m_RenderingContext;
  Scope<Renderer> m_Renderer;
  Scope<ResourceManager> m_ResourceManager;

  Scope<Scene> m_ActiveScene = nullptr;

  bool m_Running = true;
  bool m_Minimized = false;
  float m_LastFrameTime = 0.0f;
};
} // namespace Inferno
