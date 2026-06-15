#include <GLFW/glfw3.h>
#include <pch.h>

#include "Input.h"

namespace Inferno {
GLFWwindow *Input::s_pWindow = nullptr;

bool Input::IsKeyDown(int keycode) {
  const int state = glfwGetKey(s_pWindow, keycode);
  return state == GLFW_PRESS || state == GLFW_REPEAT;
}

bool Input::IsKeyMouseButtonDown(int button) {
  const int state = glfwGetMouseButton(s_pWindow, button);
  return state == GLFW_PRESS;
}

float Input::GetMouseX() {
  double xpos, ypos;
  glfwGetCursorPos(s_pWindow, &xpos, &ypos);
  return static_cast<float>(xpos);
}

float Input::GetMouseY() {
  double xpos, ypos;
  glfwGetCursorPos(s_pWindow, &xpos, &ypos);
  return static_cast<float>(ypos);
}
} // namespace Inferno
