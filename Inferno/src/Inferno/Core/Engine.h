#pragma once

#include "Inferno/Core/Memory.h"
#include "Inferno/ECS/Scene.h"
#include "Inferno/Events/ApplicationEvent.h"
#include "Inferno/Events/Event.h"
#include "Inferno/Renderer/DeviceContext.h"
#include "Inferno/Renderer/Renderer.h"
#include "Inferno/Resource/ResourceManager.h"
#include "Inferno/Tools/EditorCamera.h"
#include "Window.h"

namespace Inferno {
class Engine {
public:
  Engine();
  virtual ~Engine() = default;

  Engine(const Engine &) = delete;
  Engine &operator=(const Engine &) = delete;

  void StartUp();
  void ShutDown();

  void Run();
  void OnEvent(Event &event);

  void QueueActiveScene(Scope<Scene> scene);

private:
  bool OnWindowClosed(WindowCloseEvent &event);
  bool OnWindowResize(WindowResizeEvent &event);

  void SwitchScene();

private:
  // ArenaAllocator m_EngineArena;
  // ArenaAllocator m_SceneArena;

  Scope<Window> m_Window;
  Scope<DeviceContext> m_RenderingContext;
  Scope<Renderer> m_Renderer;
  Scope<ResourceManager> m_ResourceManager;
  Scope<EditorCamera> m_EditorCamera;

  Scope<Scene> m_ActiveScene = nullptr;
  Scope<Scene> m_NextScene = nullptr;

  bool m_Running = true;
  bool m_Editing = true;
  bool m_Minimized = false;
  float m_LastFrameTime = 0.0f;
};
} // namespace Inferno
