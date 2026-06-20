#pragma once

#include <memory>
#include <string>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <utility>

#include "Inferno/Core/Memory.h"
#include "Inferno/Renderer/DeviceContext.h"
#include "Inferno/Renderer/Mesh.h"
#include "Inferno/Renderer/Shader.h"
#include "Inferno/Renderer/Texture.h"
#include "Inferno/Resource/Resource.h"

namespace Inferno {
class ResourceManager {
public:
  ResourceManager(const DeviceContext *context) : m_Context(context) {}
  ~ResourceManager() = default;

  // Loading
  template <typename T, typename... Args>
  T *Load(const std::string &resourceID) {
    static_assert(std::is_base_of<Resource, T>::value,
                  "ResourceManager: T must derive from Resource");

    const std::type_index typeIdx = std::type_index(typeid(T));

    auto &typeResources = m_Resources[typeIdx];
    auto resourceIt = typeResources.find(resourceID);

    // If exists return
    if (resourceIt != typeResources.end()) {
      return std::static_pointer_cast<T>(resourceIt->second).get();
    }

    constexpr bool isRenderingResource = std::is_same_v<T, Mesh> ||
                                         std::is_same_v<T, Texture> ||
                                         std::is_same_v<T, Shader>;

    Ref<T> resource = nullptr;

    if constexpr (isRenderingResource) {
      resource = MakeRef<T>(resourceID, m_Context);
    } else {
      resource = MakeRef<T>(resourceID);
    }

    // Create new Resource and try loading
    if (!resource->Load()) {
      return nullptr;
    }

    typeResources.emplace(resourceID, resource);

    return resource.get();
  }

  // Accessing
  template <typename T>
  T *GetResource(const std::string &resourceID) const {
    const std::type_index typeIdx = std::type_index(typeid(T));

    auto typeResourcesIt = m_Resources.find(typeIdx);
    if (typeResourcesIt == m_Resources.end()) {
      return nullptr;
    }

    const auto &typeResources = typeResourcesIt->second;
    auto resourceIt = typeResources.find(resourceID);

    if (resourceIt != typeResources.end()) {
      return std::static_pointer_cast<T>(resourceIt->second).get();
    }

    return nullptr;
  }

  // Validation
  template <typename T> bool HasResource(const std::string &resourceID) const {
    const std::type_index typeIdx = std::type_index(typeid(T));

    auto typeResourcesIt = m_Resources.find(typeIdx);
    if (typeResourcesIt == m_Resources.end()) {
      return false;
    }

    const auto &typeResources = typeResourcesIt->second;

    return typeResources.find(resourceID) != typeResources.end();
  }

  // Releasing Resources
  template <typename T> void Release(const std::string &resourceId) {
    const std::type_index typeIdx = std::type_index(typeid(T));

    auto typeResourcesIt = m_Resources.find(typeIdx);
    if (typeResourcesIt == m_Resources.end()) {
      return;
    }

    auto &typeResources = typeResourcesIt->second;
    auto resourceIt = typeResources.find(resourceId);
    if (resourceIt == typeResources.end()) {
      return;
    }

    resourceIt->second->UnLoad();
    typeResources.erase(resourceIt);

    if (typeResources.empty()) {
      m_Resources.erase(typeResourcesIt);
    }
  }

  void UnloadAll() {
    for (auto &[type, typeResources] : m_Resources) {
      for (auto &[id, resource] : typeResources) {
        resource->UnLoad();
      }
      typeResources.clear();
    }
    m_Resources.clear();
  }

private:
  const DeviceContext *m_Context = nullptr;

  std::unordered_map<std::type_index,
                     std::unordered_map<std::string, Ref<Resource>>>
      m_Resources;
};
} // namespace Inferno
