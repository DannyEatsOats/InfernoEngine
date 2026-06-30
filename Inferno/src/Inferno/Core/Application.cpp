#include "Application.h"

#include "GLFW/glfw3.h"
#include "Inferno/Core/Memory.h"
#include "Inferno/Events/ApplicationEvent.h"
#include "Inferno/Events/Event.h"
#include "Inferno/Events/Input.h"
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
  FrameLimiter limiter(160.0);

  while (m_Running) {
    //limiter.startFrame();

    ZoneScopedN("Frame Start");
    const float time = static_cast<float>(glfwGetTime());
    const DeltaTime deltaTime = time - m_LastFrameTime;
    m_LastFrameTime = time;

    //INFERNO_LOG_INFO("Duration (ms): {}", deltaTime.GetMilliseconds());
    //INFERNO_LOG_INFO("FPTS:: {}", 1000.0f / deltaTime.GetMilliseconds());

    float dt = std::min(deltaTime.GetSeconds(), 0.1f);

    if (!m_Minimized) {
      for (Layer *layer : m_LayerStack) {
        layer->OnUpdate(dt);
      }

      if (m_ActiveScene) {
        m_ActiveScene->OnUpdate(dt);
      }

      // TODO GUI Layer Stuff
      m_Renderer->Render(m_ActiveScene->GetEntities());
    }
    // TODO Gui end
    // TODO window stuff

    m_Window->OnUpdate();
    //limiter.endFrame();
    FrameMark;
  }
  ShutDown();
}

void Application::OnEvent(Event &event) {
  EventDispatcher dispatcher(event);
  dispatcher.Dispatch<WindowCloseEvent>(
      [this](WindowCloseEvent &event) { return this->OnWindowClosed(event); });
  dispatcher.Dispatch<WindowResizeEvent>(
      [this](WindowResizeEvent &event) { return this->OnWindowResize(event); });
  dispatcher.Dispatch<SetLightingDebugModeEvent>(
      [this](SetLightingDebugModeEvent &event) {
        //m_Renderer->SetLightingDebugMode(event.Mode);
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
