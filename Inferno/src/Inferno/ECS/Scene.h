#pragma once

#include "Inferno/ECS/Entity.h"
#include "Inferno/Events/Event.h"
#include "Inferno/Resource/ResourceManager.h"
#include "Inferno/Utils/DeltaTime.h"

namespace Inferno {
class Application;

class Scene {
public:
  using EventCallbackFn = std::function<void(Event &)>;

  Scene(const std::string &name) : m_Name(std::move(name)) {}
  virtual ~Scene() = default;

  virtual void OnAttach() = 0;
  virtual void OnDetach() = 0;
  virtual void OnGuiRender() = 0;
  virtual void SaveScene() = 0;
  virtual void LoadScene() = 0;

  virtual void OnUpdate(DeltaTime deltaTime);
  virtual void OnEvent(Event &event) {}

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

  EventCallbackFn m_CallbackFn = nullptr;

  friend class Application;
};
} // namespace Inferno
