#pragma once

#include <functional>
#include <pch.h>

#include "Inferno/Events/Event.h"
#include "Inferno/Utils/DeltaTime.h"

namespace Inferno {
class Layer {
public:
  using EventCallbackFn = std::function<void(Event &)>;

  Layer(std::string name = "Layer") : m_Name(std::move(name)) {}
  virtual ~Layer() = default;

  virtual void OnAttach() = 0;
  virtual void OnDetach() = 0;
  virtual void OnUpdate(DeltaTime deltaTime) = 0;
  virtual void OnImGuiRender() = 0;
  virtual void OnEvent(Event &event) = 0;

  void SetEventCallback(const EventCallbackFn &callback) {
    m_CallbackFn = callback;
  }

  inline const std::string &GetName() const { return m_Name; }

protected:
  void BroadcastEvent(Event &event) { m_CallbackFn(event); }

private:
  std::string m_Name;
  EventCallbackFn m_CallbackFn = nullptr;
};
} // namespace Inferno
