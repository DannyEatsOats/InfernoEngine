#pragma once

#include <GLFW/glfw3.h>
namespace Inferno {
class Input {
public:
  static bool IsKeyDown(int keycode);
  static bool IsKeyMouseButtonDown(int button);
  static float GetMouseX();
  static float GetMouseY();

  static void SetWindowHandle(GLFWwindow *window) { s_pWindow = window; }

private:
  static GLFWwindow *s_pWindow;
};
} // namespace Inferno
