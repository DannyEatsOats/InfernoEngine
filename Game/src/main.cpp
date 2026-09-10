#include "Inferno/Core/Memory.h"
#include "Inferno/ECS/Entity.h"
#include "Inferno/ECS/Scene.h"
#include "Inferno/Events/ApplicationEvent.h"
#include "Inferno/Events/Input.h"
#include "Inferno/Events/KeyCodes.h"
#include "Inferno/Events/KeyEvent.h"
#include "glm/ext/quaternion_trigonometric.hpp"
#include "glm/ext/vector_float3.hpp"
#include "glm/trigonometric.hpp"
#include <Inferno.h>

namespace Inferno {
class GameScene : public Inferno::Scene {
public:
  GameScene(const std::string &name) : Inferno::Scene(name) {}
  virtual ~GameScene() = default;

  Scope<Scene> Clone() override {
    Scope<GameScene> snapshot = MakeScope<GameScene>(m_Name);

    snapshot->m_ResourceManager = m_ResourceManager;
    snapshot->m_CallbackFn = m_CallbackFn;

    for (auto entity : m_Entities) {
      Entity *clonedEntity = entity->Clone();
      snapshot->m_Entities.push_back(clonedEntity);

      if (entity == m_ActiveCamera) {
        snapshot->m_ActiveCamera = clonedEntity;
      }
    }

    return snapshot;
  }

  void OnAttach() override {
    // Setting Active Gameplay Camera
    {
      Entity *mainCamera = CreateEntity("MainCamera");
      auto camera = mainCamera->AddComponent<CameraComponent>();
      auto transform = mainCamera->GetComponent<TransformComponent>();

      transform->SetPosition(glm::vec3(0.0f, 1.0f, 3.0f));

      camera->SetPerspective(45.0f, 1920.0f / 1080.0f, 0.1f, 100.0f);
      SetActiveCamera(mainCamera);
    }

    // Adding In Game Entities
    {
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

    {
      Entity *knight = CreateEntity("viking");
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
      transform->SetPosition(transform->GetPosition() + glm::vec3(0.0f, 0.0f, -3.0f));

      auto mesh = m_ResourceManager->Load<Mesh>("viking_room");
      auto texture = m_ResourceManager->Load<Texture>("viking_room");
      knight->AddComponent<MeshComponent>(mesh, texture);

    }
  }

  void OnDetach() override {}

  void OnGuiRender() override {}

  void SaveScene() override {}

  void LoadScene() override {}

  void OnUpdate(DeltaTime deltaTime) override {
    float moveSpeed = 2.0f * deltaTime;
    glm::vec3 movement(0.0f);

    if (Input::IsKeyDown(ENGINE_KEY_LEFT))
      movement.x -= moveSpeed;
    if (Input::IsKeyDown(ENGINE_KEY_RIGHT))
      movement.x += moveSpeed;
    if (Input::IsKeyDown(ENGINE_KEY_UP))
      movement.y += moveSpeed;
    if (Input::IsKeyDown(ENGINE_KEY_DOWN))
      movement.y -= moveSpeed;

    float rotationSpeed = deltaTime * glm::radians(90.0f);
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
      if (entity->GetName() == "knight") {
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
  Inferno::Engine app = Inferno::Engine();
  app.QueueActiveScene(
      std::move(Inferno::MakeScope<Inferno::GameScene>("GameScene")));
  app.Run();
}
