#pragma once

#include "Inferno/ECS/Entity.h"
#include "Inferno/Resource/ResourceManager.h"
#include "Inferno/Utils/DeltaTime.h"

namespace Inferno {
class Application;

class Scene {
public:
  Scene(const std::string &name) : m_Name(std::move(name)) {}
  virtual ~Scene() = default;

  virtual void OnAttach() = 0;
  virtual void OnDetach() = 0;
  virtual void OnGuiRender() = 0;
  virtual void SaveScene() = 0;
  virtual void LoadScene() = 0;

  virtual void OnUpdate(DeltaTime deltaTime);

  Entity *CreateEntity(const std::string &id);
  const std::vector<Entity *> &GetEntities() const { return m_Entities; }

protected:
  std::vector<Entity *> m_Entities;
  ResourceManager *m_ResourceManager = nullptr;
  std::string m_Name;

private:
  void SetResourceManager(ResourceManager *manager) {
    m_ResourceManager = manager;
  }
  friend class Application;
};
} // namespace Inferno
