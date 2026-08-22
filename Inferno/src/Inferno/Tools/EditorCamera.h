// EditorCamera.h
#pragma once
#include "Inferno/Core/Log.h"
#include "Inferno/Events/Event.h"
#include "Inferno/Events/Input.h"
#include "Inferno/Events/KeyCodes.h"
#include "Inferno/Events/MouseEvent.h"
#include "Inferno/Renderer/Camera.h"
#include "Inferno/Utils/DeltaTime.h"
#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/ext/vector_common.hpp"
#include "glm/ext/vector_float3.hpp"
#include "glm/geometric.hpp"
#include "glm/trigonometric.hpp"
#include <cmath>

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

//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//

class DannyCamera {
public:
  void Init(float aspect) {
    m_Camera.SetPerspective(m_Camera.GetFOV(), aspect, m_Camera.GetNear(),
                            m_Camera.GetFar());
    UpdateBasisVectors();
  }

  void OnUpdate(DeltaTime deltaTime) {
    if (!m_RHMBpressed)
      return;

    float velocity = m_MovementSpeed * deltaTime;

    if (Input::IsKeyDown(ENGINE_KEY_W)) {
      m_Position += m_Front * velocity;
    }
    if (Input::IsKeyDown(ENGINE_KEY_S)) {
      m_Position -= m_Front * velocity;
    }
    if (Input::IsKeyDown(ENGINE_KEY_A)) {
      m_Position -= m_Right * velocity;
    }
    if (Input::IsKeyDown(ENGINE_KEY_D)) {
      m_Position += m_Right * velocity;
    }
    if (Input::IsKeyDown(ENGINE_KEY_E)) {
      m_Position += m_Up * velocity * 0.7f;
    }
    if (Input::IsKeyDown(ENGINE_KEY_Q)) {
      m_Position -= m_Up * velocity * 0.7f;
    }

    UpdateBasisVectors();
  }

  void OnEvent(Event &event) {
    EventDispatcher dispather(event);

    dispather.Dispatch<MouseMovedEvent>(
        [this](MouseMovedEvent &event) { return this->OnMouseMoved(event); });

    dispather.Dispatch<MouseScrolledEvent>([this](MouseScrolledEvent &event) {
      return this->OnMouseScrolled(event);
    });

    dispather.Dispatch<MouseButtonPressedEvent>(
        [this](MouseButtonPressedEvent &event) {
          return this->OnMouseButtonPressed(event);
        });

    dispather.Dispatch<MouseButtonReleasedEvent>(
        [this](MouseButtonReleasedEvent &event) {
          return this->OnMouseButtonReleased(event);
        });
  }

  const glm::mat4 &GetViewMat() const { return m_View; }

  const glm::mat4 &GetProjectionMat() const {
    return m_Camera.GetProjectionMatrix();
  }

  const glm::vec3 &GetPosition() const { return m_Position; }

  void SetViewPortSize(float width, float height) {
    switch (m_Camera.GetType()) {
    case Inferno::CamType::PERSPECTIVE:
      m_Camera.SetPerspective(m_Camera.GetFOV(), width / height,
                              m_Camera.GetNear(), m_Camera.GetFar());
      break;
    case Inferno::CamType::ORTHOGRAPHIC:
      m_Camera.SetOrthographic(m_Zoom, width / height, m_Camera.GetNear(),
                               m_Camera.GetFar());
      break;
    }
  }

private:
  void UpdateBasisVectors() {
    glm::vec3 newFront;
    newFront.x = cos(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch));
    newFront.y = sin(glm::radians(m_Pitch));
    newFront.z = sin(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch));

    m_Front = glm::normalize(newFront);
    m_Right = glm::normalize(glm::cross(m_Front, m_WorldUp));
    m_Up = glm::normalize(glm::cross(m_Right, m_Front));

    m_View = glm::lookAt(m_Position, m_Position + m_Front, m_Up);
  }

  bool OnMouseMoved(MouseMovedEvent &event) {
    if (!m_RHMBpressed)
      return false;

    if (m_FirstMouse) {
      m_LastMouseX = event.GetX();
      m_LastMouseY = event.GetY();
      m_FirstMouse = false;
    }

    float xoffset = event.GetX() - m_LastMouseX;
    float yoffset = m_LastMouseY - event.GetY();

    m_LastMouseX = event.GetX();
    m_LastMouseY = event.GetY();

    m_Yaw += xoffset * m_MouseSensitivity;
    m_Pitch = glm::clamp(m_Pitch + yoffset * m_MouseSensitivity, -89.0f, 89.0f);

    UpdateBasisVectors();

    return true;
  }

  bool OnMouseScrolled(MouseScrolledEvent &event) {
    if (m_RHMBpressed) {
      if (event.GetOffsetY() > 0) {
        m_MovementSpeed = glm::clamp(m_MovementSpeed + 0.5f, 0.5f, 20.0f);
      } else if (event.GetOffsetY() < 0) {
        m_MovementSpeed = glm::clamp(m_MovementSpeed - 0.5f, 0.5f, 20.0f);
      }
    }

    return false;
  }

  bool OnMouseButtonPressed(MouseButtonPressedEvent &event) {
    if (event.GetMouseButton() == ENGINE_MOUSE_BUTTON_RIGHT) {
      m_RHMBpressed = true;
      m_FirstMouse = true;

      return true;
    }
    return false;
  }

  bool OnMouseButtonReleased(MouseButtonReleasedEvent &event) {
    if (m_RHMBpressed) {
      m_RHMBpressed = false;
      return true;
    }
    return false;
  }

private:
  Camera m_Camera;
  glm::mat4 m_View;

  glm::vec3 m_Position = glm::vec3(0.0f, 2.0f, 5.0f);
  glm::vec3 m_Front = glm::vec3(0.0f, 0.0f, -1.0f);
  glm::vec3 m_Up = glm::vec3(0.0f, 1.0f, 0.0f);
  glm::vec3 m_Right = glm::vec3(1.0f, 0.0f, 0.0f);
  glm::vec3 m_WorldUp = glm::vec3(0.0f, 1.0f, 0.0f);

  float m_Yaw = -90.0f;
  float m_Pitch = 0.0f;

  float m_MovementSpeed = 5.0f;
  float m_MouseSensitivity = 0.05f;
  float m_Zoom = 1.0f;

  float m_LastMouseX = 0.0f;
  float m_LastMouseY = 0.0f;
  bool m_FirstMouse = true;

  bool m_RHMBpressed = false;
};

} // namespace Inferno
