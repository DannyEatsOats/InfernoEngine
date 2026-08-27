#include <pch.h>

#include <cstddef>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/trigonometric.hpp>

#include "Component.h"
#include "Entity.h"
#include "Inferno/Core/Log.h"

namespace Inferno {
// -------- COMPONENT TYPE ID SYSTEM --------
size_t ComponentTypeIDSystem::s_NextTypeID = 0;

// -------- COMPONENT BASE --------
Component::~Component() {
  if (m_State != State::Destroyed) {
    OnDestroy();
    m_State = State::Destroyed;
  }
}

void Component::Initialize() {
  if (m_State == State::Uninitialized) {
    m_State = State::Intializing;
    OnInitialize();
    m_State = State::Active;
  }
}

void Component::Destroy() {
  if (m_State == State::Active) {
    m_State = State::Destroying;
    OnDestroy();
    m_State = State::Destroyed;
  }
}

// -------- TRANSFORM COMPONENT --------
glm::mat4 TransformComponent::GetTransformmatrix() const {
  if (m_TransformDirty) {
    glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), m_Position);
    glm::mat4 rotationMatrix = glm::mat4_cast(m_Rotation);
    glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), m_Scale);

    m_TransformMatrix = translationMatrix * rotationMatrix * scaleMatrix;
    m_TransformDirty = false;
  }
  return m_TransformMatrix;
}

// -------- CAMERA COMPONENT --------
void CameraComponent::SetPerspective(float fov, float aspect, float near,
                                     float far) {
  m_Camera.SetPerspective(fov, aspect, near, far);
}

void CameraComponent::SetOrthographic(float size, float aspect, float nearPlane,
                                      float farPlane) {
  m_Camera.SetOrthographic(size, aspect, nearPlane, farPlane);
}

void CameraComponent::SetAspectRatio(float aspect) {
  m_Camera.SetAspectRatio(aspect);
}

glm::mat4 CameraComponent::GetViewMatrix() const {
  auto transform = GetEntity()->GetComponent<TransformComponent>();
  if (!transform) {
    INFERNO_LOG_WARN("Entity {} with CameraComponent has no Transform!",
                     GetEntity()->GetName());
    return glm::mat4(1.0f);
  }

  glm::vec3 position = transform->GetPosition();
  glm::quat rotation = transform->GetRotation();

  // Local -Z
  glm::vec3 forward = rotation * glm::vec3(0.0f, 0.0f, -1.0f);
  // Local +Y
  glm::vec3 up = rotation * glm::vec3(0.0f, 1.0f, 0.0f);

  return glm::lookAt(position, position + forward, up);
}

glm::mat4 CameraComponent::GetProjectionMatrix() const {
  return m_Camera.GetProjectionMatrix();
}

// -------- MESH COMPONENT --------
void MeshComponent::Render() {
  if (!m_Mesh || !/*m_Material*/ m_Texture) {
    INFERNO_LOG_WARN("Entity {} with MeshComponent has no Mesh or Material!",
                     GetEntity()->GetName());
    return;
  }

  auto transform = GetEntity()->GetComponent<TransformComponent>();
  if (!transform) {
    INFERNO_LOG_WARN("Entity {} with MeshComponent has no Transform!",
                     GetEntity()->GetName());
    return;
  }

  /*
  m_Material->Bind();
  m_Material->SetUniform("modelMatrix", transform->GetTransformmatrix());
  m_Mesh->Render();
  */
}
} // namespace Inferno
