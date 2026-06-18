#include "Inferno/Core/Memory.h"
#include "Inferno/ECS/Scene.h"
#include "Inferno/Events/ApplicationEvent.h"
#include "Inferno/Events/Input.h"
#include "Inferno/Events/KeyCodes.h"
#include "Inferno/Events/KeyEvent.h"
#include "glm/ext/quaternion_geometric.hpp"
#include "glm/ext/quaternion_trigonometric.hpp"
#include "glm/ext/vector_float3.hpp"
#include "glm/trigonometric.hpp"
#include <Inferno.h>

namespace Inferno {
class TestLayer : public Inferno::Layer {
public:
  TestLayer() : Inferno::Layer("Test Layer") {}
  ~TestLayer() {}

  virtual void OnAttach() { INFERNO_LOG_INFO("ATTACHED TEST LAYER"); };

  virtual void OnDetach() { INFERNO_LOG_INFO("DETACHED TEST LAYER"); };

  virtual void OnUpdate(DeltaTime deltaTime) {}

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

    for (int i = 0; i < 2; ++i) {
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

      transform->SetPosition(
          glm::vec3(-1.5f + i * 2.0f, 0.0f, 0.0f - i * 2.0f));

      auto mesh = m_ResourceManager->Load<Mesh>("zsamo");
      auto texture = m_ResourceManager->Load<Texture>("zsamo");
      knight->AddComponent<MeshComponent>(mesh, texture);
    }
  }

  void OnDetach() override {}

  void OnGuiRender() override {}

  void SaveScene() override {}

  void LoadScene() override {}

  void OnUpdate(DeltaTime deltaTime) override {
    float moveSpeed = 2.0f * deltaTime.GetSeconds();
    glm::vec3 movement(0.0f);

    if (Input::IsKeyDown(ENGINE_KEY_LEFT))
      movement.x -= moveSpeed;
    if (Input::IsKeyDown(ENGINE_KEY_RIGHT))
      movement.x += moveSpeed;
    if (Input::IsKeyDown(ENGINE_KEY_UP))
      movement.y += moveSpeed;
    if (Input::IsKeyDown(ENGINE_KEY_DOWN))
      movement.y -= moveSpeed;

    float rotationSpeed = glm::radians(90.0f) * deltaTime;
    glm::vec3 rotationAxis(0.0f);

    if (Input::IsKeyDown(ENGINE_KEY_F))
      rotationAxis.x -= 1.0f;
    if (Input::IsKeyDown(ENGINE_KEY_G))
      rotationAxis.x += 1.0f;
    if (Input::IsKeyDown(ENGINE_KEY_H))
      rotationAxis.y -= 1.0f;
    if (Input::IsKeyDown(ENGINE_KEY_J))
      rotationAxis.y += 1.0f;
    if (Input::IsKeyDown(ENGINE_KEY_K))
      rotationAxis.z += 1.0f;
    if (Input::IsKeyDown(ENGINE_KEY_L))
      rotationAxis.z -= 1.0f;

    for (auto &entity : GetEntities()) {
      auto transform = entity->GetComponent<TransformComponent>();

      if (glm::length(movement) > 0.0f) {
        transform->SetPosition(transform->GetPosition() + movement);
      }

      if (glm::length(rotationAxis) > 0.0f) {
        glm::vec3 normAxis = glm::normalize(rotationAxis);

        glm::quat deltaRotation = glm::angleAxis(rotationSpeed, normAxis);

        glm::quat currentRotation = transform->GetRotation();
        transform->SetRotation(deltaRotation * currentRotation);
      }
    }
  }

  void OnEvent(Event &event) override {
    EventDispatcher dispatcher(event);

    dispatcher.Dispatch<KeyPressedEvent>([this](KeyPressedEvent &event) {
      if (Input::IsKeyDown(ENGINE_KEY_LEFT_CONTROL)) {
        switch (event.GetKeyCode()) {
        case ENGINE_KEY_GRAVE_ACCENT: {
          SetLightingDebugModeEvent event(0);
          BroadcastEvent(event);
          return true;
        }
        case ENGINE_KEY_1: {
          SetLightingDebugModeEvent event(1);
          BroadcastEvent(event);
          return true;
        }
        case ENGINE_KEY_2: {
          SetLightingDebugModeEvent event(2);
          BroadcastEvent(event);
          return true;
        }
        case ENGINE_KEY_3: {
          SetLightingDebugModeEvent event(3);
          BroadcastEvent(event);
          return true;
        }
        case ENGINE_KEY_4: {
          SetLightingDebugModeEvent event(4);
          BroadcastEvent(event);
          return true;
        }
        }
      }
      return false;
    });
  }
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
