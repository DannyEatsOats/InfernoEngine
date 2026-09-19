#pragma once

#include "Inferno/Core/Window.h"
#include "Inferno/Renderer/DeviceContext.h"
#include "Inferno/Utils/DeltaTime.h"
#include "vulkan/vulkan_core.h"
namespace Inferno {
class GUISystem {
public:
  void StartUp(DeviceContext *context, Window *window);
  void ShutDown();

  void OnEvent(Event &event);
  void Update(DeltaTime deltaTime);

  void NewFrame();
  void RenderGUI(VkCommandBuffer cmd, VkImageView targetView,
                 VkExtent2D extent);

private:
  DeviceContext *m_Context = nullptr;
  VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
};
} // namespace Inferno
