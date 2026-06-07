#pragma once

#include "Inferno/Renderer/BoundingBox.h"
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

  bool IsOutsidePlane(const Plane &plane, const BoundingBox &boundingBox) const {
    glm::vec3 positive;
    positive.x = plane.Normal.x >= 0.0f ? boundingBox.GetMax().x : boundingBox.GetMin().x;
    positive.y = plane.Normal.y >= 0.0f ? boundingBox.GetMax().y : boundingBox.GetMin().y;
    positive.z = plane.Normal.z >= 0.0f ? boundingBox.GetMax().z : boundingBox.GetMin().z;

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

class Camera {
public:
  explicit Camera(Frustum m_Frustrum) : m_Frustum(m_Frustrum) {}

  Camera(const Camera &) = default;
  Camera(Camera &&) = default;

  Camera &operator=(const Camera &) = default;
  Camera &operator=(Camera &&) = default;

  const Frustum &GetFrustum() const { return m_Frustum; }

private:
  Frustum m_Frustum;
};
} // namespace Inferno
