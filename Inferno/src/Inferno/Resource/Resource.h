#pragma once

#include <string>

namespace Inferno {
class Resource {
public:
  explicit Resource(const std::string &id) : m_ResourceID(id) {}
  virtual ~Resource() = default;

  Resource(Resource &&other)
      : m_ResourceID(std::move(other.m_ResourceID)), m_Loaded(other.m_Loaded) {
    other.m_ResourceID = "";
    other.m_Loaded = false;
  }

  Resource &operator=(Resource &&other) {
    if (this == &other) {
      return *this;
    }
    m_ResourceID = other.m_ResourceID;
    m_Loaded = other.m_Loaded;

    other.m_ResourceID = "";
    other.m_Loaded = false;

    return *this;
  }

  const std::string &GetID() const { return m_ResourceID; }
  bool IsLoaded() const { return m_Loaded; }

  bool Load() {
    if (m_Loaded)
      return true;

    m_Loaded = DoLoad();
    return m_Loaded;
  }
  bool UnLoad() {
    if (!m_Loaded)
      return true;

    bool success = DoUnLoad();

    if (success)
      m_Loaded = false;

    return success;
  }

protected:
  virtual bool DoLoad() = 0;
  virtual bool DoUnLoad() = 0;

private:
  std::string m_ResourceID; // Unique ID
  bool m_Loaded = false;
};
} // namespace Inferno
