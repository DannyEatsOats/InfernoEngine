#pragma once

#include "Event.h"

namespace Inferno {
class KeyEvent : public Event {
public:
  inline int GetKeyCode() const { return m_KeyCode; }
  inline int GetMods() const { return m_Mods; }

  EVENT_CLASS_CATEGORY(EventCategoryKeyboard | EventCategoryInput)
protected:
  KeyEvent(int keyCode, int mods) : m_KeyCode(keyCode) {}

  int m_KeyCode;
  int m_Mods;
};

class KeyPressedEvent : public KeyEvent {
public:
  KeyPressedEvent(int keyCode, int mods, int repeatCount)
      : KeyEvent(keyCode, mods), m_RepeatCount(repeatCount) {}

  inline int GetRepeatCount() const { return m_RepeatCount; }

  std::string ToString() const override {
    std::stringstream ss;
    ss << "KeyPressedEvent: " << m_KeyCode << " (" << m_RepeatCount
       << " repeats)";
    return ss.str();
  }

  EVENT_CLASS_TYPE(KeyPressed)

private:
  int m_RepeatCount;
};

class KeyReleasedEvent : public KeyEvent {
public:
  KeyReleasedEvent(int keyCode, int mods) : KeyEvent(keyCode, mods) {}

  std::string ToString() const override {
    std::stringstream ss;
    return ss.str();
  }

  EVENT_CLASS_TYPE(KeyReleased)
};
} // namespace Inferno
