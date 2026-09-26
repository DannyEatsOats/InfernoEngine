#pragma once

#include "Inferno/ECS/Component.h"
#include "Inferno/Tools/EditorSystem.h"

namespace Inferno {
namespace UI {
void DrawTransformComponent(TransformComponent *transform,
                            TransformEditorState &transformEditorState);

// Helpers

glm::quat EulerDegToQuat(const glm::vec3 &eulerDegrees);
bool SameOrientation(const glm::quat &a, const glm::quat &b);
float UnwrapAngleNear(float angle, float referece);
glm::vec3 UnwrapEulerNear(glm::vec3 euler, const glm::vec3 &reference);
glm::vec3 ClosestEulerRepresentation(const glm::quat &rotation,
                                     const glm::vec3 &previousEulerDegrees);
}; // namespace UI
} // namespace Inferno
