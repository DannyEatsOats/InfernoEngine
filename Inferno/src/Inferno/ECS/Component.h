#pragma once

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
    m_Rotation = rot;
    m_TransformDirty = true;
  }

  void SetScale(const glm::vec3 &scale) {
    m_Scale = scale;
    m_TransformDirty = true;
  }

  glm::mat4 GetTransformmatrix() const;

private:
  glm::vec3 m_Position = glm::vec3(0.0f, 0.0f, 0.0f);
  glm::quat m_Rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
  glm::vec3 m_Scale = glm::vec3(1.0f);

  mutable glm::mat4 m_TransformMatrix = glm::mat4(1.0f);
  mutable bool m_TransformDirty = true;
};

// -------- CAMERA COMPONENT --------
// TODO: CREATE SEPERATE PERSPECTIVE AND ORTHOGRAPHIC
class CameraComponent : public Component {
public:
  void SetPerspective(float fov, float aspect, float near, float far);
  glm::mat4 GetViewMatrix() const;
  glm::mat4 GetProjectionMatrix() const;

private:
  float m_FOV = 45.0f;
  float m_AspectRatio = 16.0f / 9.0f;
  float m_NearPlane = 0.1f;
  float m_FarPlane = 100.0f;

  glm::mat4 m_ViewMatrix = glm::mat4(1.0f);
  mutable glm::mat4 m_ProjectionMatrix = glm::mat4(1.0f);
  mutable bool m_ProjectionDirty = false;
};

// -------- MESH COMPONENT --------
class Mesh;
class Material;
class MeshComponent : public Component {
public:
  MeshComponent(Mesh *mesh, Material *material)
      : m_Mesh(mesh), m_Material(material) {}

  void SetMesh(Mesh *mesh) { m_Mesh = mesh; }
  void SetMaterial(Material *material) { m_Material = material; }

  Mesh *GetMesh() const { return m_Mesh; }
  Material *GetMaterial() const { return m_Material; }

  virtual void Render() override;

private:
  Mesh *m_Mesh = nullptr;
  Material *m_Material = nullptr;
};
} // namespace Inferno
