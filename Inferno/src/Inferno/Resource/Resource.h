#pragma once

#include <string>

namespace Inferno {
class Resource {
public:
  explicit Resource(const std::string &id) : m_ResourceID(id) {}
  virtual ~Resource() = default;

  const std::string &GetID() const { return m_ResourceID; }
  bool IsLoaded() const { return m_Loaded; }

  bool Load() {
    m_Loaded = DoLoad();
    return m_Loaded;
  }
  bool UnLoad() {
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
