#include "Inferno/Application.h"
#include "Inferno/Layer.h"
#include "Inferno/Log.h"
#include <iostream>
#include <memory>

#include <Inferno.h>

namespace Inferno {
class TestLayer : public Inferno::Layer {
public:
  TestLayer() : Inferno::Layer("Test Layer") {}
  ~TestLayer() {}

  virtual void OnAttach() { INFERNO_LOG_INFO("ATTACHED TEST LAYER"); };

  virtual void OnDetach() { INFERNO_LOG_INFO("DETACHED TEST LAYER"); };

  virtual void OnUpdate(DeltaTime deltaTime) {
    //INFERNO_LOG_INFO("UPDATE TEST LAYER {}", static_cast<float>(deltaTime));
  }

  virtual void OnImGuiRender() { INFERNO_LOG_INFO("IMGUIRENDER TEST LAYER"); }
  virtual void OnEvent(Event &event) { INFERNO_LOG_INFO("ONEVENT TEST LAYER"); }
};
} // namespace Inferno

int main() {
  Inferno::Application *app = new Inferno::Application();
  app->PushLayer(new Inferno::TestLayer());
  app->Run();
  delete app;
}
