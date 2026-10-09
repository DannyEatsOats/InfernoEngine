#include "ComponentUI.h"
#include "Inferno/ECS/Component.h"
#include "glm/gtc/type_ptr.hpp"
#include "imgui.h"
#include <pch.h>

namespace Inferno {
void UI::DrawTransformComponent(TransformComponent *transform,
                                TransformEditorState &transformEditorState) {
  if (!transform) {
    return;
  }

  glm::vec3 position = transform->GetPosition();
  glm::vec3 scale = transform->GetScale();

  const float labelWidth = 100.0f;

  if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::Text("Position");
    ImGui::SameLine(labelWidth);
    if (ImGui::DragFloat3("##Position", glm::value_ptr(position), 0.01f)) {
      transform->SetPosition(position);
    }

    TransformEditorState &rotationState = transformEditorState;

    const glm::quat componentRotation =
        glm::normalize(transform->GetRotation());

    if (!rotationState.Initialized) {
      rotationState.EulerDegrees =
          glm::degrees(glm::eulerAngles(componentRotation));

      rotationState.LastRotation = componentRotation;
      rotationState.Initialized = true;
    } else if (!rotationState.Editing &&
               !SameOrientation(componentRotation,
                                rotationState.LastRotation)) {
      rotationState.EulerDegrees = ClosestEulerRepresentation(
          componentRotation, rotationState.EulerDegrees);

      rotationState.LastRotation = componentRotation;
    }

    ImGui::Text("Rotation");
    ImGui::SameLine(labelWidth);
    const bool rotationChanged = ImGui::DragFloat3(
        "##Rotation", glm::value_ptr(rotationState.EulerDegrees), 0.25f);

    rotationState.Editing = ImGui::IsItemActive();

    if (rotationChanged) {
      const glm::quat editedRotation =
          EulerDegToQuat(rotationState.EulerDegrees);

      transform->SetRotation(editedRotation);

      rotationState.LastRotation = transform->GetRotation();
    }

    ImGui::Text("Scale");
    ImGui::SameLine(labelWidth);
    if (ImGui::DragFloat3("##Scale", glm::value_ptr(scale), 0.01f)) {
      transform->SetScale(scale);
    }
  }
}

void UI::DrawCameraComponent(CameraComponent *camera) {
  if (!camera)
    return;

  ImGui::PushID(camera);

  if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
    auto &cam = camera->GetCamera();
    float fov = cam.GetFOV();
    float near = cam.GetNear();
    float far = cam.GetFar();
    float aspect = cam.GetAspect();

    const float labelWidth = 100.0f;

    ImGui::Text("FOV");
    ImGui::SameLine(labelWidth);
    if (ImGui::DragFloat("##FOV", &fov)) {
      camera->SetPerspective(fov, aspect, near, far);
    }

    ImGui::Text("Near");
    ImGui::SameLine(labelWidth);
    if (ImGui::DragFloat("##Near", &near)) {
      camera->SetPerspective(fov, aspect, near, far);
    }

    ImGui::Text("Far");
    ImGui::SameLine(labelWidth);
    if (ImGui::DragFloat("##Far", &far)) {
      camera->SetPerspective(fov, aspect, near, far);
    }
  }

  ImGui::PopID();
}

void UI::DrawDirectionalLightComponent(DirectionalLightComponent *light) {
  if (!light)
    return;

  ImGui::PushID(light);

  if (ImGui::CollapsingHeader("Directional Light",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    glm::vec3 color = light->GetColor();
    float intensity = light->GetIntensity();

    if (ImGui::ColorEdit3("Color", glm::value_ptr(color))) {
      light->SetColor(color);
    }

    if (ImGui::DragFloat("Intensity", &intensity, 0.05f, 0.0f, 100000.0f)) {
      light->SetIntensity(intensity);
    }
  }

  ImGui::PopID();
}

void UI::DrawPointLightComponent(PointLightComponent *light) {
  if (!light)
    return;

  ImGui::PushID(light);

  if (ImGui::CollapsingHeader("Point Light",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    glm::vec3 color = light->GetColor();
    float intensity = light->GetIntensity();
    float range = light->GetRange();

    if (ImGui::ColorEdit3("Color", glm::value_ptr(color))) {
      light->SetColor(color);
    }

    if (ImGui::DragFloat("Intensity", &intensity, 0.05f, 0.0f, 100000.0f)) {
      light->SetIntensity(intensity);
    }

    if (ImGui::DragFloat("Range", &range, 0.1f, 0.0f, 100000.0f)) {
      light->SetRange(range);
    }
  }

  ImGui::PopID();
}

void UI::DrawSpotLightComponent(SpotLightComponent *light) {
  if (!light)
    return;

  ImGui::PushID(light);

  if (ImGui::CollapsingHeader("Spot Light",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    glm::vec3 color = light->GetColor();
    float intensity = light->GetIntensity();
    float range = light->GetRange();
    float innerAngle = light->GetInnerConeAngleDegrees();
    float outerAngle = light->GetOuterConeAngleDegrees();

    if (ImGui::ColorEdit3("Color", glm::value_ptr(color))) {
      light->SetColor(color);
    }

    if (ImGui::DragFloat("Intensity", &intensity, 0.05f, 0.0f, 100000.0f)) {
      light->SetIntensity(intensity);
    }

    if (ImGui::DragFloat("Range", &range, 0.1f, 0.0f, 100000.0f)) {
      light->SetRange(range);
    }

    if (ImGui::SliderFloat("Inner Angle", &innerAngle, 0.0f, outerAngle,
                           "%.1f deg")) {
      light->SetInnerConeAngleDegrees(innerAngle);
    }

    if (ImGui::SliderFloat("Outer Angle", &outerAngle, innerAngle, 89.0f,
                           "%.1f deg")) {
      light->SetOuterConeAngleDegrees(outerAngle);
    }
  }

  ImGui::PopID();
}

void UI::DrawMeshComponent(MeshComponent *mesh) {}

// Helpers
glm::quat UI::EulerDegToQuat(const glm::vec3 &eulerDegrees) {
  const glm::vec3 eulerRadians = glm::radians(eulerDegrees);

  const glm::quat rotationX =
      glm::angleAxis(eulerRadians.x, glm::vec3(1.0f, 0.0f, 0.0f));

  const glm::quat rotationY =
      glm::angleAxis(eulerRadians.y, glm::vec3(0.0f, 1.0f, 0.0f));

  const glm::quat rotationZ =
      glm::angleAxis(eulerRadians.z, glm::vec3(0.0f, 0.0f, 1.0f));

  return glm::normalize(rotationZ * rotationY * rotationX);
}

bool UI::SameOrientation(const glm::quat &a, const glm::quat &b) {
  const glm::quat normalizedA = glm::normalize(a);
  const glm::quat normalizedB = glm::normalize(b);

  const float similarity = std::abs(glm::dot(normalizedA, normalizedB));

  return similarity >= 1.0f - 0.000001f;
}

float UI::UnwrapAngleNear(float angle, float referece) {
  return angle + 360.0f * std::round((referece - angle) / 360.0f);
}

glm::vec3 UI::UnwrapEulerNear(glm::vec3 euler, const glm::vec3 &reference) {
  euler.x = UnwrapAngleNear(euler.x, reference.x);
  euler.y = UnwrapAngleNear(euler.y, reference.y);
  euler.z = UnwrapAngleNear(euler.z, reference.z);

  return euler;
}

glm::vec3
UI::ClosestEulerRepresentation(const glm::quat &rotation,
                               const glm::vec3 &previousEulerDegrees) {
  const glm::quat normalizedRotation = glm::normalize(rotation);

  glm::vec3 primary = glm::degrees(glm::eulerAngles(normalizedRotation));

  glm::vec3 alternate{
      primary.x + 180.0f,
      180.0f - primary.y,
      primary.z + 180.0f,
  };

  primary = UnwrapEulerNear(primary, previousEulerDegrees);
  alternate = UnwrapEulerNear(alternate, previousEulerDegrees);

  const glm::vec3 primaryDifference = primary - previousEulerDegrees;

  const glm::vec3 alternateDifference = alternate - previousEulerDegrees;

  const float primaryDistanceSquared =
      glm::dot(primaryDifference, primaryDifference);

  const float alternateDistanceSquared =
      glm::dot(alternateDifference, alternateDifference);

  if (primaryDistanceSquared <= alternateDistanceSquared) {
    return primary;
  }

  return alternate;
}
} // namespace Inferno
