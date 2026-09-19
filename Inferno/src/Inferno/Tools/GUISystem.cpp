#include "GUISystem.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include "vulkan/vulkan_core.h"
#include <pch.h>
#include <stdexcept>
#include <volk/volk.h>
#include <vulkan/vulkan_core.h>

namespace Inferno {
void GUISystem::StartUp(DeviceContext *context, Window *window) {
  m_Context = context;

  std::array<VkDescriptorPoolSize, 1> poolSizes{{{
      .type = VK_DESCRIPTOR_TYPE_SAMPLER,
      .descriptorCount = IMGUI_IMPL_VULKAN_MINIMUM_SAMPLED_IMAGE_POOL_SIZE,
  }}};

  VkDescriptorPoolCreateInfo poolInfo{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
      .maxSets = IMGUI_IMPL_VULKAN_MINIMUM_SAMPLED_IMAGE_POOL_SIZE,
      .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
      .pPoolSizes = poolSizes.data(),
  };

  if (vkCreateDescriptorPool(m_Context->Device, &poolInfo, nullptr,
                             &m_DescriptorPool) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create descriptor pool for imgui");
  }

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  io.ConfigFlags |=
      ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
  io.ConfigFlags |=
      ImGuiConfigFlags_NavEnableGamepad; // Enable Gamepad Controls
  // io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // IF using Docking
  // Branch

  ImGui::StyleColorsDark();

  ImGui_ImplGlfw_InitForVulkan(window->GetNativeWindow(), true);

  VkFormat colorFormat = m_Context->Swapchain.Format;
  VkPipelineRenderingCreateInfo renderingInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
      .colorAttachmentCount = 1,
      .pColorAttachmentFormats = &colorFormat,
  };

  ImGui_ImplVulkan_InitInfo initInfo{
      .Instance = m_Context->Instance,
      .PhysicalDevice = m_Context->PhysicalDevice,
      .Device = m_Context->Device,
      .QueueFamily = m_Context->GraphicsQueueFamily,
      .Queue = m_Context->GraphicsQueue,
      .DescriptorPool = m_DescriptorPool,
      .MinImageCount = 2,
      .ImageCount = static_cast<uint32_t>(m_Context->Swapchain.Images.size()),
      .PipelineInfoMain =
          {
              .MSAASamples = VK_SAMPLE_COUNT_1_BIT,
              .PipelineRenderingCreateInfo = renderingInfo,
          },
      .UseDynamicRendering = VK_TRUE,
      .CheckVkResultFn =
          [](VkResult result) {
            if (result != VK_SUCCESS) {
              throw std::runtime_error(
                  "Failed to initialize IMGUI Vulkan Backend");
            }
          },
  };
  ImGui_ImplVulkan_Init(&initInfo);
}

void GUISystem::ShutDown() {
  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
  vkDestroyDescriptorPool(m_Context->Device, m_DescriptorPool, nullptr);
}

void GUISystem::NewFrame() {
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
}

void GUISystem::RenderGUI(VkCommandBuffer cmd, VkImageView targetView,
                          VkExtent2D extent) {
  ImGui::Render();
  ImDrawData *drawData = ImGui::GetDrawData();

  const bool isMinimized =
      (drawData->DisplaySize.x <= 0.0f || drawData->DisplaySize.y <= 0.0f);

  if (!isMinimized) {
    return;
  }

  VkRenderingAttachmentInfo colorAttachment{
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
      .imageView = targetView,
      .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
  };

  VkRenderingInfo renderingInfo{
      .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
      .renderArea = {.offset = {0, 0}, .extent = extent},
      .layerCount = 1,
      .colorAttachmentCount = 1,
      .pColorAttachments = &colorAttachment,
  };

  vkCmdBeginRendering(cmd, &renderingInfo);
  ImGui_ImplVulkan_RenderDrawData(drawData, cmd);
  vkCmdEndRendering(cmd);
}

} // namespace Inferno
