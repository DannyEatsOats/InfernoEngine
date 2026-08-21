#pragma once

#include "Inferno/Renderer/BoundingBox.h"
#include "glm/ext/matrix_clip_space.hpp"
namespace Inferno {
struct Plane {
  glm::vec3 Normal{0.0f};
  float Distance = 0.0f;

  float GetSignedDistance(const glm::vec3 &point) const {
    return glm::dot(Normal, point) + Distance;
  }
};

struct Frustum {
  Plane Left;
  Plane Right;
  Plane Top;
  Plane Bottom;
  Plane Near;
  Plane Far;

  bool IsOutsidePlane(const Plane &plane,
                      const BoundingBox &boundingBox) const {
    glm::vec3 positive;
    positive.x = plane.Normal.x >= 0.0f ? boundingBox.GetMax().x
                                        : boundingBox.GetMin().x;
    positive.y = plane.Normal.y >= 0.0f ? boundingBox.GetMax().y
                                        : boundingBox.GetMin().y;
    positive.z = plane.Normal.z >= 0.0f ? boundingBox.GetMax().z
                                        : boundingBox.GetMin().z;

    return plane.GetSignedDistance(positive) < 0.0f;
  }

  bool Intersects(const BoundingBox &boundingBox) const {
    if (IsOutsidePlane(Left, boundingBox)) {
      return false;
    }

    if (IsOutsidePlane(Right, boundingBox)) {
      return false;
    }

    if (IsOutsidePlane(Top, boundingBox)) {
      return false;
    }

    if (IsOutsidePlane(Bottom, boundingBox)) {
      return false;
    }

    if (IsOutsidePlane(Near, boundingBox)) {
      return false;
    }

    if (IsOutsidePlane(Far, boundingBox)) {
      return false;
    }

    return true;
  }
};

enum class CamType { PERSPECTIVE, ORTHOGRAPHIC };

class Camera {
public:
  Camera() {}
  Camera(const Camera &) = default;
  Camera(Camera &&) = default;
  Camera &operator=(const Camera &) = default;
  Camera &operator=(Camera &&) = default;

  void SetPerspective(float fov, float aspect, float nearPlane,
                      float farPlane) {
    m_Type = CamType::PERSPECTIVE;
    m_FOV = fov;
    m_AspectRatio = aspect;
    m_NearPlane = nearPlane;
    m_FarPlane = farPlane;
    m_ProjectionDirty = true;
  }

  void SetOrthographic(float size, float aspect, float nearPlane,
                       float farPlane) {
    m_Type = CamType::ORTHOGRAPHIC;
    m_OrthoLeft = -size * aspect;
    m_OrthoRight = size * aspect;
    m_OrthoBottom = -size;
    m_OrthoTop = size;
    m_NearPlane = nearPlane;
    m_FarPlane = farPlane;
    m_ProjectionDirty = true;
  }

  glm::mat4 GetProjectionMatrix() const {
    if (m_ProjectionDirty) {
      if (m_Type == CamType::PERSPECTIVE) {
        m_ProjectionMatrix = glm::perspective(
            glm::radians(m_FOV), m_AspectRatio, m_NearPlane, m_FarPlane);
      } else {
        m_ProjectionMatrix =
            glm::ortho(m_OrthoLeft, m_OrthoRight, m_OrthoBottom, m_OrthoTop,
                       m_NearPlane, m_FarPlane);
      }
      m_ProjectionMatrix[1][1] *= -1; // Vulkan Y-flip applies to both
      m_ProjectionDirty = false;
    }
    return m_ProjectionMatrix;
  }

  CamType GetType() const { return m_Type; }

private:
  mutable glm::mat4 m_ProjectionMatrix = glm::mat4(1.0f);

  // Perspective params
  float m_FOV = 45.0f;
  float m_AspectRatio = 16.0f / 9.0f;

  // Ortho params
  float m_OrthoLeft = -1.0f, m_OrthoRight = 1.0f;
  float m_OrthoBottom = -1.0f, m_OrthoTop = 1.0f;

  // Shared
  float m_NearPlane = 0.1f;
  float m_FarPlane = 100.0f;

  mutable bool m_ProjectionDirty = true;
  CamType m_Type = CamType::PERSPECTIVE;
};
} // namespace Inferno
