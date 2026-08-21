#include "EditorCamera.h"
#include <pch.h>

// EditorCamera.cpp
#include "EditorCamera.h"
#include "Inferno/Events/Input.h"
#include "Inferno/Events/KeyCodes.h"
#include "Inferno/Events/MouseEvent.h"

namespace Inferno {

void EditorCamera::Init(float fov, float aspect, float nearPlane,
                        float farPlane) {
  m_Camera.SetPerspective(fov, aspect, nearPlane, farPlane);
  UpdateBasisVectors();
}

void EditorCamera::UpdateBasisVectors() {
  glm::vec3 front;
  front.x = cos(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch));
  front.y = sin(glm::radians(m_Pitch));
  front.z = sin(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch));
  m_Front = glm::normalize(front);
  m_Right = glm::normalize(glm::cross(m_Front, m_WorldUp));
  m_Up = glm::normalize(glm::cross(m_Right, m_Front));
}

void EditorCamera::OnUpdate(DeltaTime dt) {
  float velocity = m_MovementSpeed * dt.GetSeconds();
  if (Input::IsKeyDown(ENGINE_KEY_W))
    m_Position += m_Front * velocity;
  if (Input::IsKeyDown(ENGINE_KEY_S))
    m_Position -= m_Front * velocity;
  if (Input::IsKeyDown(ENGINE_KEY_A))
    m_Position -= m_Right * velocity;
  if (Input::IsKeyDown(ENGINE_KEY_D))
    m_Position += m_Right * velocity;
  if (Input::IsKeyDown(ENGINE_KEY_E))
    m_Position += m_Up * velocity * 0.5f;
  if (Input::IsKeyDown(ENGINE_KEY_Q))
    m_Position -= m_Up * velocity * 0.5f;
}

bool EditorCamera::OnMouseMoved(MouseMovedEvent &e) {
  if (m_FirstMouse) {
    m_LastMouseX = e.GetX();
    m_LastMouseY = e.GetY();
    m_FirstMouse = false;
  }
  float dx = e.GetX() - m_LastMouseX;
  float dy = m_LastMouseY - e.GetY();
  m_LastMouseX = e.GetX();
  m_LastMouseY = e.GetY();

  m_Yaw += dx * m_MouseSensitivity;
  m_Pitch = glm::clamp(m_Pitch + dy * m_MouseSensitivity, -89.0f, 89.0f);
  UpdateBasisVectors();
  return false;
}

bool EditorCamera::OnMouseScrolled(MouseScrolledEvent &e) {
  // adjust FOV or movement speed, your call — e.g.:
  // m_MovementSpeed = glm::clamp(m_MovementSpeed +
  // e.GetYOffset(), 1.0f, 50.0f);
  return false;
}

void EditorCamera::SetViewportSize(float width, float height) {
  m_Camera.SetPerspective(45.0f, width / height, 0.1f,
                          1000.0f); // or track/reuse existing FOV/near/far
}

} // namespace Inferno
