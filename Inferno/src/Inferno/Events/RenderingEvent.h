#pragma once

#include "Event.h"

namespace Inferno {
class TextureAddedEvent : public Event {
public:
  TextureAddedEvent() = default;
  ~TextureAddedEvent() = default;

  std::string ToString() const override {
    std::stringstream ss;
    ss << "TextureAddedEvent";
    return ss.str();
  }

  EVENT_CLASS_TYPE(TextureAddedEvent)
  EVENT_CLASS_CATEGORY(EventCategoryRendering)
};
} // namespace Inferno
