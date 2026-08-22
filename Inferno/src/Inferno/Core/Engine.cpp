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
  /**
   * @brief Construct a new Frame Limiter object
   * @param targetFps The desired maximum frame rate (e.g., 60.0)
   */
  FrameLimiter(double targetFps) {
    setTargetFps(targetFps);
    m_FrameStart = std::chrono::high_resolution_clock::now();
  }

  /**
   * @brief Dynamically change the target frame rate at runtime
   */
  void setTargetFps(double targetFps) {
    m_TargetFps = targetFps;
    m_TargetDuration = std::chrono::duration<double>(1.0 / targetFps);
  }

  /**
   * @brief Call at the absolute beginning of your main loop iteration
   */
  void startFrame() {
    m_FrameStart = std::chrono::high_resolution_clock::now();
  }

  /**
   * @brief Call at the absolute end of your main loop iteration
   * Uses a high-precision hybrid sleep/spin-lock tailored for Linux.
   */
  void endFrame() {
    const auto frameEnd = std::chrono::high_resolution_clock::now();
    const auto elapsed = frameEnd - m_FrameStart;

    if (elapsed < m_TargetDuration) {
      const auto remainingTime = m_TargetDuration - elapsed;

      // 1. Precise OS Sleep
      // Linux kernel high-resolution timers (hrtimers) are incredibly sharp.
      // We sleep for the majority of the time, leaving a tiny 0.5ms (500us)
      // buffer.
      if (remainingTime > std::chrono::microseconds(500)) {
        std::this_thread::sleep_for(remainingTime -
                                    std::chrono::microseconds(500));
      }

      // 2. Precise Spin-lock
      // Burn the remaining <0.5ms in a tight loop to hit the exact microsecond
      // target.
      while (std::chrono::high_resolution_clock::now() - m_FrameStart <
             m_TargetDuration) {
// Emit a NOP instruction to let the CPU pipeline optimize hyper-threading
// and avoid burning excessive watt-hours while spinning.
#if defined(__GNUC__) || defined(__clang__)
        asm volatile("nop");
#elif defined(_MSC_VER)
        __nop();
#endif
      }
    }
  }

  /**
   * @brief Get the current target FPS
   */
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
  m_Renderer->StartUp(m_ResourceManager.get());
  m_EditorCamera = MakeScope<DannyCamera>();
  m_EditorCamera->Init((float)m_Window->GetWidth() /
                       (float)m_Window->GetHeight());
  m_Renderer->SetActiveCamera(
      {m_EditorCamera->GetViewMat(), m_EditorCamera->GetProjectionMat()});
  Input::SetWindowHandle(m_Window->GetNativeWindow());
}

void Engine::ShutDown() {
  INFERNO_LOG_INFO("Shutting Down Engine...");
  m_ActiveScene->OnDetach();

  m_Renderer->ShutDown();
  m_ResourceManager->UnloadAll();
  m_RenderingContext->ShutDown();
  m_Running = false;
}

void Engine::Run() {
  FrameLimiter limiter(160.0);

  while (m_Running) {
    // limiter.startFrame();

    ZoneScopedN("Frame Start");

    static auto lastTime = std::chrono::high_resolution_clock::now();
    auto currentTime = std::chrono::high_resolution_clock::now();
    float dt = std::chrono::duration<float>(currentTime - lastTime).count();
    lastTime = currentTime;
    const DeltaTime deltaTime = std::min(dt, 0.05f);

    // INFERNO_LOG_INFO("Duration (ms): {}", deltaTime.GetMilliseconds());
    // INFERNO_LOG_INFO("FPTS:: {}", 1000.0f / deltaTime.GetMilliseconds());

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
        if (m_ActiveScene)
          m_ActiveScene->OnUpdate(deltaTime);
        // TODO: MOCK GAME CAMERA
        glm::mat4 proj = glm::perspective(
            glm::radians(45.0f),
            (float)m_RenderingContext->Swapchain.Extent.width /
                (float)m_RenderingContext->Swapchain.Extent.height,
            0.1f, 10.0f);
        proj[1][1] *= -1;
        glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 1.0f, 3.0f),
                                     glm::vec3(0.0f, 0.0f, 0.0f),
                                     glm::vec3(0.0f, 1.0f, 0.0f));
        m_Renderer->SetActiveCamera({view, proj});
        break;
      }

      if (m_ActiveScene)
        m_Renderer->Render(m_ActiveScene->GetEntities());

      m_Window->OnUpdate();
      // limiter.endFrame();
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
        m_RuntimeMode = RuntimeMode::GAME;
        break;
      case Inferno::RuntimeMode::GAME:
        m_RuntimeMode = RuntimeMode::EDITOR;
        break;
      }
      return true;
    }
    return false;
  });

  /*
  dispatcher.Dispatch<SetLightingDebugModeEvent>(
      [this](SetLightingDebugModeEvent &event) {
        m_Renderer->SetLightingDebugMode(event.Mode);
        return true;
      });
  */

  switch (m_RuntimeMode) {
  case Inferno::RuntimeMode::EDITOR:
    if (!event.IsHandled()) {
      m_EditorCamera->OnEvent(event);
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

  return false;
}

} // namespace Inferno
