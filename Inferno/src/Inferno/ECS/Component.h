#pragma once

#include "Inferno/Core/Memory.h"
#include "Inferno/Renderer/Camera.h"
#include "Inferno/Renderer/Texture.h"
#include "Inferno/Utils/DeltaTime.h"
#include "glm/ext/matrix_float4x4.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Inferno {

class Entity;

class ComponentTypeIDSystem {
public:
  template <typename T> static size_t GetTypeID() {
    static size_t typeID = s_NextTypeID++;
    return typeID;
  }

private:
  static size_t s_NextTypeID;
};

class Component {
public:
  enum class State {
    Uninitialized = 0,
    Intializing,
    Active,
    Destroying,
    Destroyed
  };

public:
  virtual ~Component();

  virtual Scope<Component> Clone() = 0;

  void Initialize();
  void Destroy();

  bool IsActive() const { return m_State == State::Active; }

  Entity *GetEntity() const { return m_Entity; }
  void SetEntity(Entity *entity) { m_Entity = entity; }

  template <typename T> static size_t GetTypeID() {
    return ComponentTypeIDSystem::GetTypeID<T>();
  }

protected:
  virtual void OnInitialize() {}
  virtual void OnDestroy() {}
  virtual void Update(DeltaTime deltaTime) {}
  virtual void Render() {}

protected:
  State m_State = State::Uninitialized;
  Entity *m_Entity = nullptr;

  friend class Entity;
};

// -------- TRANSFORM COMPONENT --------
class TransformComponent : public Component {
public:
  const glm::vec3 &GetPosition() const { return m_Position; }
  const glm::quat &GetRotation() const { return m_Rotation; }
  const glm::vec3 &GetScale() const { return m_Scale; }

  void SetPosition(const glm::vec3 &pos) {
    m_Position = pos;
    m_TransformDirty = true;
  }

  void SetRotation(const glm::quat &rot) {
    m_Rotation = glm::normalize(rot);
    m_TransformDirty = true;
  }

  void SetScale(const glm::vec3 &scale) {
    m_Scale = scale;
    m_TransformDirty = true;
  }

  glm::mat4 GetTransformmatrix() const;

  Scope<Component> Clone() override;

private:
  glm::vec3 m_Position = glm::vec3(0.0f, 0.0f, 0.0f);
  glm::quat m_Rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
  glm::vec3 m_Scale = glm::vec3(1.0f);

  mutable glm::mat4 m_TransformMatrix = glm::mat4(1.0f);
  mutable bool m_TransformDirty = true;
};

// -------- CAMERA COMPONENT --------
class CameraComponent : public Component {
public:
  void SetPerspective(float fov, float aspect, float near, float far);
  void SetOrthographic(float size, float aspect, float nearPlane,
                       float farPlane);
  void SetAspectRatio(float aspect);
  glm::mat4 GetViewMatrix() const;
  glm::mat4 GetProjectionMatrix() const;

  const Camera &GetCamera() const { return m_Camera; }

  Scope<Component> Clone() override;

private:
  Camera m_Camera;
};

// -------- DIRECTIONAL LIGHT COMPONENT --------
class DirectionalLightComponent : public Component {
public:
  const glm::vec3 &GetColor() const { return m_Color; }
  float GetIntensity() const { return m_Intensity; }

  void SetColor(const glm::vec3 &color) { m_Color = color; }
  void SetIntensity(float intensity) { m_Intensity = intensity; }

  Scope<Component> Clone() override;

private:
  glm::vec3 m_Color{1.0f};
  float m_Intensity = 1.0f;
};

// -------- POINT LIGHT COMPONENT --------
class PointLightComponent : public Component {
public:
  const glm::vec3 &GetColor() const { return m_Color; }
  float GetIntensity() const { return m_Intensity; }
  float GetRange() const { return m_Range; }

  void SetColor(const glm::vec3 &color) { m_Color = color; }
  void SetIntensity(float intensity) { m_Intensity = intensity; }
  void SetRange(float range) { m_Range = range; }

  Scope<Component> Clone() override;

private:
  glm::vec3 m_Color{1.0f};
  float m_Intensity = 1.0f;
  float m_Range = 10.0f;
};

// -------- SPOT LIGHT COMPONENT --------
class SpotLightComponent : public Component {
public:
  const glm::vec3 &GetColor() const { return m_Color; }
  float GetIntensity() const { return m_Intensity; }
  float GetRange() const { return m_Range; }
  float GetInnerConeAngleDegrees() const { return m_InnerConeAngleDegrees; }
  float GetOuterConeAngleDegrees() const { return m_OuterConeAngleDegrees; }

  void SetColor(const glm::vec3 &color) { m_Color = color; }
  void SetIntensity(float intensity) { m_Intensity = intensity; }
  void SetRange(float range) { m_Range = range; }
  void SetInnerConeAngleDegrees(float angle) {
    m_InnerConeAngleDegrees = angle;
  }
  void SetOuterConeAngleDegrees(float angle) {
    m_OuterConeAngleDegrees = angle;
  }

  Scope<Component> Clone() override;

private:
  glm::vec3 m_Color{1.0f};
  float m_Intensity = 1.0f;
  float m_Range = 10.0f;
  float m_InnerConeAngleDegrees = 20.0f;
  float m_OuterConeAngleDegrees = 30.0f;
};

// -------- MESH COMPONENT --------
class Mesh;
class Material;
class MeshComponent : public Component {
public:
  MeshComponent(Mesh *mesh, Texture *texture)
      : m_Mesh(mesh), m_Texture(texture) {}

  void SetMesh(Mesh *mesh) { m_Mesh = mesh; }
  // void SetMaterial(Material *material) { m_Material = material; }
  void SetTexture(Texture *texture) { m_Texture = texture; }

  const Mesh *GetMesh() const { return m_Mesh; }
  // Material *GetMaterial() const { return m_Material; }
  Texture *GetTexture() { return m_Texture; }

  virtual void Render() override;

  Scope<Component> Clone() override;

private:
  Mesh *m_Mesh = nullptr;
  // Material *m_Material = nullptr;
  Texture *m_Texture = nullptr;
};
} // namespace Inferno
