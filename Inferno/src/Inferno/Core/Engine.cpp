#include "Engine.h"

#include "Inferno/Core/Memory.h"
#include "Inferno/Events/ApplicationEvent.h"
#include "Inferno/Events/Event.h"
#include "Inferno/Events/Input.h"
#include "Inferno/Events/KeyCodes.h"
#include "Inferno/Events/KeyEvent.h"
#include "Inferno/Utils/DeltaTime.h"
#include "Log.h"

#include <tracy/Tracy.hpp>

namespace Inferno {
class FrameLimiter {
public:
  FrameLimiter(double targetFps) {
    setTargetFps(targetFps);
    m_FrameStart = std::chrono::high_resolution_clock::now();
  }

  void setTargetFps(double targetFps) {
    m_TargetFps = targetFps;
    m_TargetDuration = std::chrono::duration<double>(1.0 / targetFps);
  }

  void startFrame() {
    m_FrameStart = std::chrono::high_resolution_clock::now();
  }

  void endFrame() {
    const auto frameEnd = std::chrono::high_resolution_clock::now();
    const auto elapsed = frameEnd - m_FrameStart;

    if (elapsed < m_TargetDuration) {
      const auto remainingTime = m_TargetDuration - elapsed;

      if (remainingTime > std::chrono::microseconds(500)) {
        std::this_thread::sleep_for(remainingTime -
                                    std::chrono::microseconds(500));
      }

      while (std::chrono::high_resolution_clock::now() - m_FrameStart <
             m_TargetDuration) {
#if defined(__GNUC__) || defined(__clang__)
        asm volatile("nop");
#elif defined(_MSC_VER)
        __nop();
#endif
      }
    }
  }

  double getTargetFps() const { return m_TargetFps; }

private:
  double m_TargetFps;
  std::chrono::duration<double> m_TargetDuration;
  std::chrono::high_resolution_clock::time_point m_FrameStart;
};

Engine::Engine() { StartUp(); }

void Engine::StartUp() {
  Log::Init();
  INFERNO_LOG_INFO("Starting Up Engine...");

  m_Window = Window::Create();
  m_Window->SetEventCallback([this](Event &event) { this->OnEvent(event); });
  m_RenderingContext = MakeScope<DeviceContext>();
  m_RenderingContext->StartUp(m_Window->GetNativeWindow());
  m_ResourceManager = MakeScope<ResourceManager>(m_RenderingContext.get());
  m_Renderer = MakeScope<Renderer>(m_RenderingContext.get());
  m_EditorSystem = MakeScope<EditorSystem>();
  m_Renderer->StartUp(m_ResourceManager.get(), m_EditorSystem.get());
  m_EditorSystem->StartUp(m_Renderer.get());
  m_EditorCamera = MakeScope<DannyCamera>();
  m_EditorCamera->Init((float)m_Window->GetWidth() /
                       (float)m_Window->GetHeight());
  m_Renderer->SetActiveCamera(
      {m_EditorCamera->GetViewMat(), m_EditorCamera->GetProjectionMat()});
  Input::SetWindowHandle(m_Window->GetNativeWindow());
}

void Engine::ShutDown() {
  INFERNO_LOG_INFO("Shutting Down Engine...");
  if (m_ActiveScene) {
    m_ActiveScene->OnDetach();
  }

  m_EditorSystem->ShutDown();
  m_Renderer->ShutDown();
  m_ResourceManager->UnloadAll();
  m_RenderingContext->ShutDown();
  m_Running = false;
}

void Engine::Run() {
  FrameLimiter limiter(160.0);

  while (m_Running) {
    ZoneScopedN("Frame Start");

    static auto lastTime = std::chrono::high_resolution_clock::now();
    auto currentTime = std::chrono::high_resolution_clock::now();
    float dt = std::chrono::duration<float>(currentTime - lastTime).count();
    lastTime = currentTime;
    const DeltaTime deltaTime = std::min(dt, 0.05f);

    if (!m_Minimized) {
      if (m_NextScene) {
        SwitchScene();
      }

      switch (m_RuntimeMode) {
      case Inferno::RuntimeMode::EDITOR:
        m_EditorCamera->OnUpdate(deltaTime);
        m_Renderer->SetActiveCamera(
            {m_EditorCamera->GetViewMat(), m_EditorCamera->GetProjectionMat()});
        break;

      case Inferno::RuntimeMode::GAME:
        if (!m_ActiveScene) {
          INFERNO_LOG_WARN("Active Scene Is Not Set");
          break;
        }

        m_ActiveScene->OnUpdate(deltaTime);

        // Safe null checks for game camera during gameplay
        if (auto activeCamera = m_ActiveScene->GetActiveCamera()) {
          if (auto cameraComponent =
                  activeCamera->GetComponent<CameraComponent>()) {
            m_Renderer->SetActiveCamera(
                {cameraComponent->GetViewMatrix(),
                 cameraComponent->GetProjectionMatrix()});
          } else {
            INFERNO_LOG_ERROR("Active Camera Has No Camera Component");
          }
        }
        break;
      }

      if (m_ActiveScene)
        m_Renderer->Render(m_ActiveScene->GetEntities());

      m_Window->OnUpdate();
      FrameMark;
    }
  }
  ShutDown();
}

void Engine::OnEvent(Event &event) {
  EventDispatcher dispatcher(event);

  dispatcher.Dispatch<WindowCloseEvent>(
      [this](WindowCloseEvent &event) { return this->OnWindowClosed(event); });

  dispatcher.Dispatch<WindowResizeEvent>(
      [this](WindowResizeEvent &event) { return this->OnWindowResize(event); });

  dispatcher.Dispatch<KeyPressedEvent>([this](KeyPressedEvent &event) {
    if (event.GetKeyCode() == ENGINE_KEY_F12) {
      switch (m_RuntimeMode) {
      case Inferno::RuntimeMode::EDITOR:
        OnRuntimeStart();
        break;
      case Inferno::RuntimeMode::GAME:
        OnRuntimeStop();
        break;
      }
      return true;
    }
    return false;
  });

  switch (m_RuntimeMode) {
  case Inferno::RuntimeMode::EDITOR:
    if (!event.IsHandled()) {
      m_EditorCamera->OnEvent(event);
      m_EditorSystem->OnEvent(event);
    }
    break;
  case Inferno::RuntimeMode::GAME:
    if (!event.IsHandled() && m_ActiveScene) {
      m_ActiveScene->OnEvent(event);
    }
    break;
  }
}

void Engine::SwitchScene() {
  if (m_ActiveScene) {
    m_ActiveScene->OnDetach();
  }

  m_SceneSnapshop = nullptr;

  m_ActiveScene = std::move(m_NextScene);
  m_NextScene = nullptr;

  m_ActiveScene->SetResourceManager(m_ResourceManager.get());
  m_ActiveScene->SetEventCallback([this](Event &e) { this->OnEvent(e); });
  m_ActiveScene->OnAttach();
}

void Engine::QueueActiveScene(Scope<Scene> scene) {
  m_NextScene = std::move(scene);
}

bool Engine::OnWindowClosed(WindowCloseEvent &event) {
  m_Running = false;
  return true;
}

bool Engine::OnWindowResize(WindowResizeEvent &event) {
  if (event.GetWidth() == 0 || event.GetHeight() == 0) {
    m_Minimized = true;
    return false;
  }
  m_Minimized = false;
  m_Renderer->SignalResize();
  m_EditorCamera->SetViewPortSize((float)event.GetWidth(),
                                  (float)event.GetHeight());

  if (m_ActiveScene) {
    m_ActiveScene->SetViewPortSize((float)event.GetWidth(),
                                   (float)event.GetHeight());
  }

  return false;
}

void Engine::OnRuntimeStart() {
  if (!m_ActiveScene) {
    INFERNO_LOG_ERROR("[OnRuntimeStart] Active Scene is Not set");
    return;
  }

  m_ActiveScene->OnDetach();

  m_SceneSnapshop = std::move(m_ActiveScene);

  m_ActiveScene = m_SceneSnapshop->Clone();

  m_ActiveScene->SetResourceManager(m_ResourceManager.get());
  m_ActiveScene->SetEventCallback([this](Event &e) { this->OnEvent(e); });

  m_ActiveScene->OnAttach();

  m_ActiveScene->SetViewPortSize((float)m_Window->GetWidth(),
                                 (float)m_Window->GetHeight());

  m_RuntimeMode = RuntimeMode::GAME;

  INFERNO_LOG_INFO("Started Gameplay Runtime");
}

void Engine::OnRuntimeStop() {
  if (!m_SceneSnapshop) {
    INFERNO_LOG_ERROR("[OnRuntimeStop] Scene Snapshot has not been created");
    return;
  }

  // 1. Shut down live gameplay systems (stop sounds, physics, scripts)
  if (m_ActiveScene) {
    m_ActiveScene->OnDetach();
  }

  // 2. Restore unmutated Editor Scene snapshot (deletes mutated gameplay scene)
  m_ActiveScene = std::move(m_SceneSnapshop);
  m_SceneSnapshop = nullptr;

  // 3. Re-bind engine pointers & callbacks to restored editor instance
  m_ActiveScene->SetResourceManager(m_ResourceManager.get());
  m_ActiveScene->SetEventCallback([this](Event &e) { this->OnEvent(e); });

  // 4. Re-initialize editor systems / gizmos
  m_ActiveScene->OnAttach();

  m_RuntimeMode = RuntimeMode::EDITOR;

  INFERNO_LOG_INFO("Stopped Gameplay Runtime");
}
} // namespace Inferno
