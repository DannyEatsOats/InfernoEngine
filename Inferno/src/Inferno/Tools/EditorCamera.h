// EditorCamera.h
#pragma once
#include "Inferno/Events/Event.h"
#include "Inferno/Events/MouseEvent.h"
#include "Inferno/Renderer/Camera.h"
#include "Inferno/Utils/DeltaTime.h"
#include "glm/ext/matrix_transform.hpp"
#include "glm/ext/vector_float3.hpp"

namespace Inferno {
class EditorCamera {
public:
  void Init(float fov, float aspect, float nearPlane, float farPlane);
  void OnUpdate(DeltaTime dt);
  void OnEvent(Event &e);

  glm::mat4 GetViewMatrix() const {
    return glm::lookAt(m_Position, m_Position + m_Front, m_Up);
  }
  glm::mat4 GetProjectionMatrix() const {
    return m_Camera.GetProjectionMatrix();
  }

  void SetViewportSize(float width, float height);

  bool IsRotating() const { return m_IsRotating; }

private:
  void UpdateBasisVectors();
  bool OnMouseMoved(MouseMovedEvent &e);
  bool OnMouseScrolled(MouseScrolledEvent &e);
  bool OnMouseButtonPressed(MouseButtonPressedEvent &e);
  bool OnMouseButtonReleased(MouseButtonReleasedEvent &e);

private:
  Camera m_Camera;

  glm::vec3 m_Position = glm::vec3(0.0f, 2.0f, 5.0f);
  glm::vec3 m_Front = glm::vec3(0.0f, 0.0f, -1.0f);
  glm::vec3 m_Up = glm::vec3(0.0f, 1.0f, 0.0f);
  glm::vec3 m_Right = glm::vec3(1.0f, 0.0f, 0.0f);
  glm::vec3 m_WorldUp = glm::vec3(0.0f, 1.0f, 0.0f);

  float m_Yaw = -90.0f;
  float m_Pitch = 0.0f;

  float m_MovementSpeed = 5.0f;
  float m_MouseSensitivity = 0.1f;

  float m_LastMouseX = 0.0f, m_LastMouseY = 0.0f;
  bool m_FirstMouse = true;

  bool m_IsRotating = false;
};
} // namespace Inferno
