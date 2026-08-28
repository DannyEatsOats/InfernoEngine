#pragma once

#include <memory>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Component.h"
#include "Inferno/Utils/DeltaTime.h"

namespace Inferno {
class Entity {
public:
  explicit Entity(const std::string &name) : m_Name(name) {}

  const std::string &GetName() const { return m_Name; }
  bool IsActive() const { return m_Active; }
  void SetActive(bool isActive) { m_Active = isActive; }

  void Initialize();
  void Update(DeltaTime deltTime);
  void Render();

  template <typename T, typename... Args> T *AddComponent(Args &&...args) {
    static_assert(std::is_base_of<Component, T>::value,
                  "AddComponent: T must derive from Component");

    size_t typeID = Component::GetTypeID<T>();

    // Return of Entity already has Component T
    auto it = m_ComponentMap.find(typeID);
    if (it != m_ComponentMap.end()) {
      return static_cast<T *>(it->second);
    }

    // Create new Component T
    auto component = std::make_unique<T>(std::forward<Args>(args)...);
    T *componentPtr = component.get();
    componentPtr->SetEntity(this);
    m_ComponentMap[typeID] = componentPtr;
    m_Components.push_back(std::move(component));

    return componentPtr;
  }

  template <typename T> T *GetComponent() {
    size_t typeID = Component::GetTypeID<T>();
    auto it = m_ComponentMap.find(typeID);
    if (it != m_ComponentMap.end()) {
      return static_cast<T *>(it->second);
    }

    return nullptr;
  }

  template <typename T> bool RemoveComponent() {
    size_t typeID = Component::GetTypeID<T>();
    auto it = m_ComponentMap.find(typeID);

    if (it != m_ComponentMap.end()) {
      Component *componentPtr = it->second;
      m_ComponentMap.erase(it);

      for (auto compIt = m_Components.begin(); compIt != m_Components.end();
           ++compIt) {
        if (compIt->get() == componentPtr) {
          m_Components.erase(compIt);
          return true;
        }
      }
    }

    return false;
  }

  Entity *Clone() const {
    Entity *clonedEntity = new Entity(m_Name);
    clonedEntity->m_Active = m_Active;

    for (const auto &comp : m_Components) {
      if (!comp) continue;

      std::unique_ptr<Component> clonedComp = comp->Clone();
      Component *compPtr = clonedComp.get();

      compPtr->SetEntity(clonedEntity);

      size_t typeID = 0;
      for (const auto &[id, ptr] : m_ComponentMap) {
        if (ptr == comp.get()) {
          typeID = id;
          break;
        }
      }

      clonedEntity->m_ComponentMap[typeID] = compPtr;
      clonedEntity->m_Components.push_back(std::move(clonedComp));
    }

    return clonedEntity;
  }

private:
  std::string m_Name;
  bool m_Active = true;
  std::vector<std::unique_ptr<Component>> m_Components;
  std::unordered_map<size_t, Component *> m_ComponentMap;
};
} // namespace Inferno
