#pragma once

#include "Inferno/Core/Log.h"
#include "Inferno/Core/Memory.h"
#include "Inferno/ECS/Entity.h"
#include "Inferno/Events/Event.h"
#include "Inferno/Resource/ResourceManager.h"
#include "Inferno/Utils/DeltaTime.h"

namespace Inferno {
class Engine;

class Scene {
public:
  using EventCallbackFn = std::function<void(Event &)>;

  Scene(const std::string &name) : m_Name(std::move(name)) {}
  virtual ~Scene() {
    for (auto entity : m_Entities) {
      delete entity;
    }
  }

  Entity *GetActiveCamera() {
    if (!m_ActiveCamera)
      INFERNO_LOG_ERROR("[GetActiveCamera] Active Camera Not Set In Scene");

    return m_ActiveCamera;
  }

  void SetActiveCamera(Entity *cameraEntity) { m_ActiveCamera = cameraEntity; }

  void SetViewPortSize(float width, float height) {
    if (!m_ActiveCamera) {
      INFERNO_LOG_ERROR("[SetViewPortSize] Active Camera Not Set In Scene");
      return;
    }

    auto cameraComponent = m_ActiveCamera->GetComponent<CameraComponent>();

    if (!cameraComponent) {
      INFERNO_LOG_ERROR(
          "[SetViewPortSize] Active Camera Does Not Have Camera Component");
      return;
    }

    cameraComponent->SetAspectRatio(width / height);
  }

  virtual void OnAttach() = 0;
  virtual void OnDetach() = 0;
  virtual void OnGuiRender() = 0;
  virtual void SaveScene() = 0;
  virtual void LoadScene() = 0;

  virtual void OnUpdate(DeltaTime deltaTime);
  virtual void OnEvent(Event &event) {}

  virtual Scope<Scene> Clone() = 0;

  Entity *CreateEntity(const std::string &id);
  const std::vector<Entity *> &GetEntities() const { return m_Entities; }

  void SetEventCallback(const EventCallbackFn &callback) {
    m_CallbackFn = callback;
  }

protected:
  void BroadcastEvent(Event &event) { m_CallbackFn(event); }

private:
  void SetResourceManager(ResourceManager *manager) {
    m_ResourceManager = manager;
  }

protected:
  std::vector<Entity *> m_Entities;
  ResourceManager *m_ResourceManager = nullptr;
  std::string m_Name;
  Entity *m_ActiveCamera = nullptr;

  EventCallbackFn m_CallbackFn = nullptr;

  friend class Engine;
};
} // namespace Inferno
