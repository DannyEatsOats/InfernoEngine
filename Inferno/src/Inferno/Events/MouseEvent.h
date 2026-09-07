#pragma once

#include "Event.h"

namespace Inferno {
class MouseMovedEvent : public Event {
public:
  MouseMovedEvent(float x, float y) : m_MouseX(x), m_MouseY(y) {}

  std::string ToString() const override {
    std::stringstream ss;
    ss << "MouseMovedEvent: " << m_MouseX << ", " << m_MouseY;
    return ss.str();
  }

  inline float GetX() const { return m_MouseX; }
  inline float GetY() const { return m_MouseY; }

  EVENT_CLASS_TYPE(MouseMoved)
  EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryInput)

private:
  float m_MouseX;
  float m_MouseY;
};

class MouseScrolledEvent : public Event {
public:
  MouseScrolledEvent(float xOffset, float yOffset)
      : m_offsetX(xOffset), m_offsetY(yOffset) {}

  inline float GetOffsetX() const { return m_offsetX; }
  inline float GetOffsetY() const { return m_offsetY; }

  std::string ToString() const override {
    std::stringstream ss;
    ss << "MouseScrolledEvent: " << GetOffsetX() << ", " << GetOffsetY();
    return ss.str();
  }

  EVENT_CLASS_TYPE(MouseScrolled)
  EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryInput)

private:
  float m_offsetX, m_offsetY;
};

class MouseButtonEvent : public Event {
public:
  inline int GetMouseButton() const { return m_Button; }
  inline int GetMouseX() const { return m_XPos; }
  inline int GetMouseY() const { return m_YPos; }

  EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryInput |
                       EventCategoryMouseButton)
protected:
  MouseButtonEvent(int button, int xpos, int ypos)
      : m_Button(button), m_XPos(xpos), m_YPos(ypos) {}

  int m_Button;
  int m_XPos;
  int m_YPos;
};

class MouseButtonPressedEvent : public MouseButtonEvent {
public:
  MouseButtonPressedEvent(int button, int xpos, int ypos)
      : MouseButtonEvent(button, xpos, ypos) {}

  std::string ToString() const override {
    std::stringstream ss;
    ss << "MouseButtonPressedEvent: " << m_Button;
    return ss.str();
  }

  EVENT_CLASS_TYPE(MouseButtonPressed)
};

class MouseButtonReleasedEvent : public MouseButtonEvent {
public:
  MouseButtonReleasedEvent(int button, int xpos, int ypos)
      : MouseButtonEvent(button, xpos, ypos) {}

  std::string ToString() const override {
    std::stringstream ss;
    ss << "MouseButtonReleasedEvent: " << m_Button;
    return ss.str();
  }

  EVENT_CLASS_TYPE(MouseButtonReleased)
};
} // namespace Inferno
