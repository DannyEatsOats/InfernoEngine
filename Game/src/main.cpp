#include "Inferno/Core/Memory.h"
#include "Inferno/ECS/Scene.h"
#include "Inferno/Events/ApplicationEvent.h"
#include "Inferno/Events/Input.h"
#include "Inferno/Events/KeyCodes.h"
#include <Inferno.h>

namespace Inferno {
class TestLayer : public Inferno::Layer {
public:
  TestLayer() : Inferno::Layer("Test Layer") {}
  ~TestLayer() {}

  virtual void OnAttach() { INFERNO_LOG_INFO("ATTACHED TEST LAYER"); };

  virtual void OnDetach() { INFERNO_LOG_INFO("DETACHED TEST LAYER"); };

  virtual void OnUpdate(DeltaTime deltaTime) {
    if (Input::IsKeyDown(ENGINE_KEY_LEFT_CONTROL) &&
        Input::IsKeyDown(ENGINE_KEY_GRAVE_ACCENT)) {
      SetLightingDebugModeEvent event(0);
      BroadcastEvent(event);
    }
    if (Input::IsKeyDown(ENGINE_KEY_LEFT_CONTROL) &&
        Input::IsKeyDown(ENGINE_KEY_1)) {
      SetLightingDebugModeEvent event(1);
      BroadcastEvent(event);
    }
    if (Input::IsKeyDown(ENGINE_KEY_LEFT_CONTROL) &&
        Input::IsKeyDown(ENGINE_KEY_2)) {
      SetLightingDebugModeEvent event(2);
      BroadcastEvent(event);
    }
    if (Input::IsKeyDown(ENGINE_KEY_LEFT_CONTROL) &&
        Input::IsKeyDown(ENGINE_KEY_3)) {
      SetLightingDebugModeEvent event(3);
      BroadcastEvent(event);
    }
    if (Input::IsKeyDown(ENGINE_KEY_LEFT_CONTROL) &&
        Input::IsKeyDown(ENGINE_KEY_4)) {
      SetLightingDebugModeEvent event(4);
      BroadcastEvent(event);
    }
  }

  virtual void OnImGuiRender() { INFERNO_LOG_INFO("IMGUIRENDER TEST LAYER"); }
  virtual void OnEvent(Event &event) { INFERNO_LOG_INFO("ONEVENT TEST LAYER"); }
};

class GameScene : public Inferno::Scene {
public:
  GameScene(const std::string &name) : Inferno::Scene(name) {}
  virtual ~GameScene() = default;

  void OnAttach() override {
    {
      Entity *testEntity = new Entity("test");
      testEntity->AddComponent<TransformComponent>();
      // testEntity->GetComponent<TransformComponent>()->SetRotation(
      //    glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f)));
      // auto mesh =
      // m_ResourceManager->Load<Mesh>("viking_room", m_RenderingContext.get());
      // auto texture = m_ResourceManager->Load<Texture>("viking_room",
      // m_RenderingContext.get());
      // testEntity->AddComponent<MeshComponent>(mesh.get(), texture.get());
      //  m_Entities.push_back(testEntity);
    }

    /*
    {
      Entity *cube = new Entity("cube");
      cube->AddComponent<TransformComponent>();
      m_CubeMesh = GeometryGenerator::GenerateCube(m_RenderingContext.get());
      auto texture = m_ResourceManager->Load<Texture>("viking_room",
                                                      m_RenderingContext.get());
      // TODO: I SHOULD PASS A SHARED PTR HERE I THINK IDKKKKK
      cube->AddComponent<MeshComponent>(m_CubeMesh.get(), texture.get());
      m_Entities.push_back(cube);
    }
    */

    for (int i = 0; i < 1; ++i) {
      Entity *knight = CreateEntity("knight");
      auto *transform = knight->AddComponent<TransformComponent>();

      auto rotation = transform->GetRotation();
      glm::quat rotationInc =
          glm::angleAxis(glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
      glm::quat newRotation = rotationInc * rotation;
      transform->SetRotation(newRotation);

      rotation = transform->GetRotation();
      rotationInc =
          glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
      newRotation = rotationInc * rotation;
      transform->SetRotation(newRotation);

      auto mesh = m_ResourceManager->Load<Mesh>("zsamo");
      auto texture = m_ResourceManager->Load<Texture>("zsamo");
      knight->AddComponent<MeshComponent>(mesh, texture);
    }
  }

  void OnDetach() override {}

  void OnGuiRender() override {}

  void SaveScene() override {}

  void LoadScene() override {}

  void OnUpdate(DeltaTime deltaTime) override {}
};
} // namespace Inferno

int main() {
  Inferno::Application *app = new Inferno::Application();
  app->PushLayer(new Inferno::TestLayer());
  app->SetActiveScene(
      std::move(Inferno::MakeScope<Inferno::GameScene>("GameScene")));
  app->Run();
  delete app;
}
