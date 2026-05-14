#pragma once

#include <memory>
#include <string>
#include <type_traits>
#include <typeindex>
#include <unordered_map>

#include "Inferno/Resource/Resource.h"

namespace Inferno {
template <typename T> class ResourceHandle;

class ResourceManager {
public:
  // Loading
  template <typename T> ResourceHandle<T> Load(const std::string &resourceID) {
    static_assert(std::is_base_of<Resource, T>::value,
                  "ResourceManager: T must derive from Resource");

    const std::type_index typeIdx = std::type_index(typeid(T));

    auto &typeResources = m_Resources[typeIdx];
    auto resourceIt = typeResources.find(resourceID);

    // If Resource is cached - inc and return handle
    if (resourceIt != typeResources.end()) {
      ++m_RefCounts[typeIdx][resourceID].RefCount;
      return ResourceHandle<T>(resourceID, this);
    }

    // Create new Resource and try loading
    auto resource = std::make_shared<T>(resourceID);
    if (!resource->Load()) {
      // Loading failed - return invalid handle
      return ResourceHandle<T>();
    }

    typeResources.emplace(resourceID, resource);
    m_RefCounts[typeIdx].emplace(resourceID, ResourceData{resource, 1});

    return ResourceHandle<T>(resourceID, this);
  }

  // Accessing
  template <typename T> T *GetResource(const std::string &resourceID) const {
    const std::type_index typeIdx = std::type_index(typeid(T));

    auto typeResourcesIt = m_Resources.find(typeIdx);
    if (typeResourcesIt == m_Resources.end()) {
      return nullptr;
    }

    const auto &typeResources = typeResourcesIt->second;
    auto resourceIt = typeResources.find(resourceID);

    if (resourceIt != typeResources.end()) {
      return static_cast<T *>(resourceIt->second.get());
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

    auto typeRefCountsIt = m_RefCounts.find(typeIdx);
    if (typeRefCountsIt == m_RefCounts.end()) {
      return;
    }

    auto resourceRefCountsIt = typeRefCountsIt->second.find(resourceId);
    if (resourceRefCountsIt == typeRefCountsIt->second.end()) {
      return;
    }

    --resourceRefCountsIt->second.RefCount;

    if (resourceRefCountsIt->second.RefCount <= 0) {
      auto resourcePtr = resourceRefCountsIt->second.Resource;
      if (resourcePtr) {
        resourcePtr->UnLoad();
      }

      // Erase from RefCount table
      typeRefCountsIt->second.erase(resourceRefCountsIt);
      if (typeRefCountsIt->second.empty()) {
        m_RefCounts.erase(typeRefCountsIt);
      }

      // Erase from Resource cache
      auto typeResourcesIt = m_Resources.find(typeIdx);
      if (typeResourcesIt != m_Resources.end()) {
        auto &typeResources = typeResourcesIt->second;
        typeResources.erase(resourceId);
        if (typeResources.empty()) {
          m_Resources.erase(typeResourcesIt);
        }
      }
    }
  }

  void UnloadAll() {
    for (auto &[type, typeResources] : m_Resources) {
      for (auto &[id, resource] : typeResources) {
        resource->UnLoad();
      }
      typeResources.clear();
    }
    m_RefCounts.clear();
  }

private:
  std::unordered_map<std::type_index,
                     std::unordered_map<std::string, std::shared_ptr<Resource>>>
      m_Resources;

  struct ResourceData {
    std::shared_ptr<Resource> Resource;
    int RefCount = 0;
  };

  std::unordered_map<std::type_index,
                     std::unordered_map<std::string, ResourceData>>
      m_RefCounts;
};
} // namespace Inferno
