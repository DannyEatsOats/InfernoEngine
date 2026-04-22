#pragma once

#include <glm/glm.hpp>

namespace Inferno {
struct UniformBufferOjbect {
  alignas(16) glm::mat4 model;
  alignas(16) glm::mat4 view;
  alignas(16) glm::mat4 proj;
};
} // namespace Inferno
