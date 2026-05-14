#pragma once

#include "ResourceManager.h"

namespace Inferno {
template <typename T> class ResourceHandle {
public:
  ResourceHandle() : m_ResourceManager(nullptr) {}
  ResourceHandle(const std::string &id, ResourceManager *resourceManager)
      : m_ResourceID(id), m_ResourceManager(resourceManager) {}

  T *Get() const {
    if (!m_ResourceManager)
      return nullptr;

    return m_ResourceManager->GetResource<T>(m_ResourceID);
  }

  bool IsValid() const {
    return m_ResourceManager && m_ResourceManager->HasResource<T>(m_ResourceID);
  }

  const std::string &GetID() const { return m_ResourceID; }

  // Convenience
  T *operator->() const { return Get(); }

  T &operator&() const { return *Get(); }

  operator bool() const { return IsValid(); }

private:
  std::string m_ResourceID;
  ResourceManager *m_ResourceManager = nullptr;
};
} // namespace Inferno
