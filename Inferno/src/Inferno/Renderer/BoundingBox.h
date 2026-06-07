#pragma once

#include "glm/ext/vector_float3.hpp"
#include "glm/glm.hpp"
#include <limits>

namespace Inferno {
class BoundingBox {
public:
  BoundingBox(const glm::vec3 min, const glm::vec3 max)
      : m_Min(min), m_Max(max) {}

  BoundingBox(const BoundingBox &) = default;
  BoundingBox(BoundingBox &&) = default;
  BoundingBox &operator=(const BoundingBox &) = default;
  BoundingBox &operator=(BoundingBox &&) = default;

  const glm::vec3& GetMin() const {return m_Min;}

  const glm::vec3& GetMax() const {return m_Max;}

  glm::vec3 GetCenter() {
      return (m_Min + m_Max) * 0.5f;
  }

  glm::vec3 GetExtents() {
      return (m_Max - m_Min) * 0.5f;
  }

static BoundingBox Transform(
    const BoundingBox& bounds,
    const glm::mat4& transform)
{
    glm::vec3 corners[8] =
    {
        {bounds.m_Min.x, bounds.m_Min.y, bounds.m_Min.z},
        {bounds.m_Min.x, bounds.m_Min.y, bounds.m_Min.z},
        {bounds.m_Min.x, bounds.m_Max.y, bounds.m_Min.z},
        {bounds.m_Max.x, bounds.m_Max.y, bounds.m_Min.z},

        {bounds.m_Min.x, bounds.m_Min.y, bounds.m_Max.z},
        {bounds.m_Max.x, bounds.m_Min.y, bounds.m_Max.z},
        {bounds.m_Min.x, bounds.m_Max.y, bounds.m_Max.z},
        {bounds.m_Max.x, bounds.m_Max.y, bounds.m_Max.z}
    };

    glm::vec3 newMin(std::numeric_limits<float>::min());
    glm::vec3 newMax(std::numeric_limits<float>::max());

    for (const auto& corner : corners)
    {
        glm::vec3 transformed =
            glm::vec3(transform * glm::vec4(corner, 1.0f));

        newMin = glm::min(newMin, transformed);
        newMax = glm::max(newMax, transformed);
    }

    return BoundingBox(newMin, newMax);
}
private:
  glm::vec3 m_Min{0.0f};
  glm::vec3 m_Max{0.0f};
};

} // namespace Inferno
