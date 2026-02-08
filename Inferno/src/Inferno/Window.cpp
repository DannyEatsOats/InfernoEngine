#include "Window.h"
#include "Inferno/Renderer/RenderingContext.h"

// #include "imgui_impl_glfw.h"
#define GLFW_INCLUDE_VULKAN
#include "Events/ApplicationEvent.h"
#include "Events/KeyEvent.h"
#include "Events/MouseEvent.h"
#include "Log.h"
#include <GLFW/glfw3.h>

namespace Inferno {
static bool s_WindowInitialized = false;

Window *Window::Create(const WindowProperties &properties) {
  return new Window(properties);
}

Window::Window(const WindowProperties &properties) { Window::Init(properties); }

Window::~Window() { Window::Shutdown(); }

void Window::Init(const WindowProperties &properties) {
  m_Data.Width = properties.Width;
  m_Data.Height = properties.Height;
  m_Data.Title = properties.Title;

  INFERNO_LOG_INFO("Creating window {} ({} {})", properties.Title,
                   properties.Width, properties.Height);

  if (!s_WindowInitialized) {
    if (!glfwInit()) {
      INFERNO_LOG_ERROR("Could not initialize GLFW!");
      return;
    }
    glfwSetErrorCallback([](int error, const char *description) {
      INFERNO_LOG_ERROR("GLFW Error {} {}", error, description);
    });
    s_WindowInitialized = true;
  }

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

  m_Window = glfwCreateWindow(m_Data.Width, m_Data.Height, m_Data.Title.c_str(),
                              nullptr, nullptr);
  if (!m_Window) {
    INFERNO_LOG_ERROR("Failed to create GLFW window!");
    glfwTerminate();
    return;
  }

  m_Context = RenderingContext::CreateContext(m_Window);
  m_Context->Init();

  glfwSetWindowUserPointer(m_Window, &m_Data);
  SetVSync(true);

  glfwSetFramebufferSizeCallback(
      m_Window, [](GLFWwindow *window, int width, int height) {
        WindowData &data =
            *static_cast<WindowData *>(glfwGetWindowUserPointer(window));

        data.Width = width;
        data.Height = height;

        WindowResizeEvent event(width, height);
        data.EventCallback(event);
      });

  glfwSetWindowCloseCallback(m_Window, [](GLFWwindow *window) {
    WindowData &data =
        *static_cast<WindowData *>(glfwGetWindowUserPointer(window));
    WindowCloseEvent event;
    data.EventCallback(event);
  });

  glfwSetKeyCallback(m_Window, [](GLFWwindow *window, int key, int scancode,
                                  int action, int mods) {
    WindowData &data =
        *static_cast<WindowData *>(glfwGetWindowUserPointer(window));
    switch (action) {
    case GLFW_PRESS: {
      KeyPressedEvent event(key, 0);
      data.EventCallback(event);
      break;
    }
    case GLFW_RELEASE: {
      KeyReleasedEvent event(key);
      data.EventCallback(event);
      break;
    }
    case GLFW_REPEAT: {
      KeyPressedEvent event(key, 1);
      data.EventCallback(event);
      break;
    }
    default:
      break;
    }
  });

  glfwSetMouseButtonCallback(
      m_Window, [](GLFWwindow *window, int button, int action, int mods) {
        WindowData &data =
            *static_cast<WindowData *>(glfwGetWindowUserPointer(window));
        switch (action) {
        case GLFW_PRESS: {
          MouseButtonPressedEvent event(button);
          data.EventCallback(event);
          break;
        }
        case GLFW_RELEASE: {
          MouseButtonReleasedEvent event(button);
          break;
        }
        default:
          break;
        }
      });

  glfwSetScrollCallback(
      m_Window, [](GLFWwindow *window, double xoffset, double yoffset) {
        WindowData &data =
            *static_cast<WindowData *>(glfwGetWindowUserPointer(window));

        MouseScrolledEvent event(static_cast<float>(xoffset),
                                 static_cast<float>(yoffset));
        data.EventCallback(event);
      });

  glfwSetCursorPosCallback(m_Window, [](GLFWwindow *window, double xpos,
                                        double ypos) {
    WindowData &data =
        *static_cast<WindowData *>(glfwGetWindowUserPointer(window));

    MouseMovedEvent event(static_cast<float>(xpos), static_cast<float>(ypos));
    data.EventCallback(event);
  });
}

void Window::Shutdown() { glfwDestroyWindow(m_Window); }

void Window::OnUpdate() { glfwPollEvents(); }

unsigned int Window::GetWidth() const { return m_Data.Width; }

unsigned int Window::GetHeight() const { return m_Data.Height; }

void Window::SetEventCallback(const EventCallbackFn &callback) {
  m_Data.EventCallback = callback;
}

void Window::SetVSync(const bool enabled) {
  m_Data.VSync = enabled;
  // Rereacte swapchain
}

bool Window::IsVSync() const { return m_Data.VSync; }
} // namespace Inferno
