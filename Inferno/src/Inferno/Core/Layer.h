#pragma once

#include <pch.h>

#include "Inferno/Utils/DeltaTime.h"
#include "Inferno/Events/Event.h"

namespace Inferno {
class Layer {
public:
  Layer(std::string name = "Layer") : m_Name(std::move(name)) {}
  virtual ~Layer() = default;

  virtual void OnAttach() = 0;
  virtual void OnDetach() = 0;
  virtual void OnUpdate(DeltaTime deltaTime) = 0;
  virtual void OnImGuiRender() = 0;
  virtual void OnEvent(Event &event) = 0;

  inline const std::string &GetName() const { return m_Name; }

private:
  std::string m_Name;
};
} // namespace Inferno
