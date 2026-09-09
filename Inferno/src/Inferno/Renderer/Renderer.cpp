#include "Inferno/Core/Log.h"
#include "Inferno/ECS/Entity.h"
#include "Inferno/Renderer/Image.h"
#include "Inferno/Renderer/Mesh.h"
#include "Inferno/Renderer/Pipeline.h"
#include "Inferno/Renderer/VulkanUtils.h"
#include "glm/ext/matrix_float4x4.hpp"
#include "glm/matrix.hpp"
#include "tracy/Tracy.hpp"
#include <array>
#include <cstdint>
#include <cstring>
#include <optional>
#include <stdexcept>
#include <vector>
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <GLFW/glfw3.h>
#include <pch.h>
#include <volk/volk.h>
#include <vulkan/vulkan_core.h>

#include "Inferno/ECS/Component.h"
#include "Inferno/Resource/ResourceManager.h"
#include "Inferno/Tools/EditorSystem.h"
#include "Renderer.h"

#include <glm/glm.hpp>

namespace Inferno {
void Renderer::StartUp(ResourceManager *resourceManager,
                       EditorSystem *editorSystem) {
  m_ResourceManager = resourceManager;
  m_EditorSystem = editorSystem;

  CreateForwardPipeline();
  CreateGridPipeline();
  CreateOutlineDescriptorResources();
  CreateOutlinePipeline();
  AllocateCommandBuffer();
  CreateSyncObjects();
  // TODO: SET CAMERA
}

void Renderer::ShutDown() {
  if (m_Context && m_Context->Device != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(m_Context->Device);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
      vkDestroyFence(m_Context->Device, m_Frames[i].DrawFence, nullptr);
    }

    size_t swapchanSize = m_Context->Swapchain.Images.size();
    for (size_t i = 0; i < swapchanSize; ++i) {
      vkDestroySemaphore(m_Context->Device, m_RenderFinishedSemaphores[i],
                         nullptr);
    }

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
      vkDestroySemaphore(m_Context->Device,
                         m_Frames[i].PresentCompleteSemaphore, nullptr);
    }

    vkDestroyDescriptorSetLayout(m_Context->Device,
                                 m_TextureDescriptorSetLayout, nullptr);

    vkDestroyDescriptorPool(m_Context->Device, m_TextureDescriptorPool,
                            nullptr);

    vkDestroySampler(m_Context->Device, m_OutlineSampler, nullptr);

    vkDestroyDescriptorSetLayout(m_Context->Device,
                                 m_OutlineDescriptorSetLayout, nullptr);
    vkDestroyDescriptorPool(m_Context->Device, m_OutlineDescriptorPool,
                            nullptr);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
      m_Frames[i].DepthImage = Image();
    }

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
      m_Frames[i].EntityPickingImage = Image();
    }

    m_OutlinePipeline.Destroy(m_Context->Device);
    m_GridPipeline.Destroy(m_Context->Device);
    m_ForwardPipeline.Destroy(m_Context->Device);
  }
}

void Renderer::Render(const std::vector<Entity *> &entities) {
  if (m_Resized) {
    Resize();
  }

  bool success = true;

  // Draw Frame
  if (vkWaitForFences(m_Context->Device, 1, &m_Frames[m_FrameIndex].DrawFence,
                      VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
    throw std::runtime_error("Failed to Wait on Draw Fence");
  }

  {
    ZoneScopedN("FelRobban a Fing");
    auto [result, imageIndex] = m_Context->AcquireNextImage(
        m_Frames[m_FrameIndex].PresentCompleteSemaphore);
    m_ImageIndex = imageIndex;

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
      SignalResize();
      return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
      throw std::runtime_error("Failed to acquire swapchain image!");
    }
    vkResetFences(m_Context->Device, 1, &m_Frames[m_FrameIndex].DrawFence);
  }

  RecordForwardPass(entities);

  VkPipelineStageFlags waitDstStageMask =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

  VkSubmitInfo submitInfo{
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &m_Frames[m_FrameIndex].PresentCompleteSemaphore,
      .pWaitDstStageMask = &waitDstStageMask,
      .commandBufferCount = 1,
      .pCommandBuffers = &m_Frames[m_FrameIndex].CommandBuffer,
      .signalSemaphoreCount = 1,
      .pSignalSemaphores = &m_RenderFinishedSemaphores[m_ImageIndex],
  };

  if (vkQueueSubmit(m_Context->GraphicsQueue, 1, &submitInfo,
                    m_Frames[m_FrameIndex].DrawFence) != VK_SUCCESS) {
    throw std::runtime_error("Failed To Submit To Graphics Queue");
  }

  VkPresentInfoKHR presentInfo{
      .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &m_RenderFinishedSemaphores[m_ImageIndex],
      .swapchainCount = 1,
      .pSwapchains = &m_Context->Swapchain.Handle,
      .pImageIndices = &m_ImageIndex,
  };

  VkResult presentResult =
      vkQueuePresentKHR(m_Context->PresentQueue, &presentInfo);
  if (presentResult == VK_ERROR_OUT_OF_DATE_KHR ||
      presentResult == VK_SUBOPTIMAL_KHR) {
    SignalResize();
  } else if (presentResult != VK_SUCCESS) {
    throw std::runtime_error("Failed To Submit To Present Queue");
  }

  m_FrameIndex = (m_FrameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Renderer::CreateForwardPipeline() {
  // Depth Image Creation
  ImageSpec depthImageSpec{
      .Width = m_Context->Swapchain.Extent.width,
      .Height = m_Context->Swapchain.Extent.height,
      .MipLevels = 1,
      .Format = VK_FORMAT_D32_SFLOAT,
      .Usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
               VK_IMAGE_USAGE_SAMPLED_BIT,
      .Aspect = VK_IMAGE_ASPECT_DEPTH_BIT,
  };

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    m_Frames[i].DepthImage = Image(m_Context, depthImageSpec);
  }

  // Entity Picking Image Creation
  ImageSpec pickingImageSpec{
      .Width = m_Context->Swapchain.Extent.width,
      .Height = m_Context->Swapchain.Extent.height,
      .MipLevels = 1,
      .Format = VK_FORMAT_R32_UINT,
      .Usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
               VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      .Aspect = VK_IMAGE_ASPECT_COLOR_BIT,
  };

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    m_Frames[i].EntityPickingImage = Image(m_Context, pickingImageSpec);
  }

  // Shader Stages
  auto *shader = m_ResourceManager->Load<Shader>("test");

  // Vertex Input
  auto meshVertex = MeshVertex::GetLayout();

  // Color Blending
  VkPipelineColorBlendAttachmentState colorBlendAttachment{
      .blendEnable = VK_TRUE,
      .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
      .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
      .colorBlendOp = VK_BLEND_OP_ADD,
      .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
      .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
      .alphaBlendOp = VK_BLEND_OP_ADD,
      .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
  };

  VkPipelineColorBlendAttachmentState pickingBlendAttachment{
      .blendEnable = VK_FALSE,
      .colorWriteMask = VK_COLOR_COMPONENT_R_BIT,
  };

  std::vector<VkPipelineColorBlendAttachmentState> blendAttachments = {
      colorBlendAttachment,
      pickingBlendAttachment,
  };

  VkPipelineColorBlendStateCreateInfo colorBlendInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .logicOpEnable = VK_FALSE,
      .logicOp = VK_LOGIC_OP_COPY,
      .attachmentCount = static_cast<uint32_t>(blendAttachments.size()),
      .pAttachments = blendAttachments.data(),
  };

  // Pipeline Layout
  VkPushConstantRange pushConstantRange{
      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
      .offset = 0,
      .size = sizeof(MeshPushConstants),
  };

  // Texture Descriptor Set Layout
  VkDescriptorSetLayoutBinding samplerLayoutbinding{
      .binding = 0,
      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
      .pImmutableSamplers = nullptr,
  };

  VkDescriptorSetLayoutCreateInfo descSetLayoutInfo{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = 1,
      .pBindings = &samplerLayoutbinding,
  };

  if (vkCreateDescriptorSetLayout(m_Context->Device, &descSetLayoutInfo,
                                  nullptr, &m_TextureDescriptorSetLayout) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed To Create Texture Descriptor Set Layout");
  }

  // Texture Descriptor Pool
  VkDescriptorPoolSize poolSize{
      .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .descriptorCount = 1000,
  };

  VkDescriptorPoolCreateInfo poolInfo{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
      .maxSets = 1000,
      .poolSizeCount = 1,
      .pPoolSizes = &poolSize,
  };

  if (vkCreateDescriptorPool(m_Context->Device, &poolInfo, nullptr,
                             &m_TextureDescriptorPool) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create Texture Descriptor Pool");
  }

  std::vector<VkFormat> colorFormats = {
      m_Context->Swapchain.Format,
      VK_FORMAT_R32_UINT,
  };

  PipelineDescription description{
      .VertexShader = shader->GetVertexShaderModule(),
      .FragmentShader = shader->GetFragmentShaderModule(),
      .VertexBinding = meshVertex.GetBindingDescription(),
      .VertexAttributes = meshVertex.GetAttributeDescriptions(),
      .CullMode = VK_CULL_MODE_BACK_BIT,
      .DepthTest = VK_TRUE,
      .DepthWrite = VK_TRUE,
      .DepthFormat = VK_FORMAT_D32_SFLOAT,
      .ColorFormats = colorFormats,
      .BlendAttachments = blendAttachments,
      .DescriptorSetLayouts = {m_TextureDescriptorSetLayout},
      .PushConstantRanges = {pushConstantRange},
  };

  m_ForwardPipeline.Init(m_Context->Device, description);
}

void Renderer::CreateGridPipeline() {
  auto *shader = m_ResourceManager->Load<Shader>("grid");

  // Color Blending
  VkPipelineColorBlendAttachmentState colorBlendAttachment{
      .blendEnable = VK_TRUE,
      .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
      .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
      .colorBlendOp = VK_BLEND_OP_ADD,
      .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
      .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
      .alphaBlendOp = VK_BLEND_OP_ADD,
      .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
  };

  VkPipelineColorBlendAttachmentState pickingBlendAttachment{
      .blendEnable = VK_FALSE,
      .colorWriteMask = 0,
  };

  std::vector<VkPipelineColorBlendAttachmentState> blendAttachments = {
      colorBlendAttachment,
      pickingBlendAttachment,
  };

  // Pipeline Layout
  VkPushConstantRange pushConstantRange{
      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
      .offset = 0,
      .size = sizeof(GridPushConstants),
  };

  std::vector<VkFormat> colorFormats = {
      m_Context->Swapchain.Format,
      VK_FORMAT_R32_UINT,
  };

  PipelineDescription description{
      .VertexShader = shader->GetVertexShaderModule(),
      .FragmentShader = shader->GetFragmentShaderModule(),
      .CullMode = VK_CULL_MODE_NONE,
      .DepthTest = VK_TRUE,
      .DepthWrite = VK_TRUE,
      .DepthFormat = VK_FORMAT_D32_SFLOAT,
      .ColorFormats = colorFormats,
      .BlendAttachments = blendAttachments,
      .PushConstantRanges = {pushConstantRange},
  };

  m_GridPipeline.Init(m_Context->Device, description);
}

void Renderer::CreateOutlinePipeline() {
  auto *shader = m_ResourceManager->Load<Shader>("editor_outline");

  // TODO: I'm not sure the outline needs blending
  VkPipelineColorBlendAttachmentState blendAttachment{
      .blendEnable = VK_TRUE,
      .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
      .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
      .colorBlendOp = VK_BLEND_OP_ADD,
      .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
      .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
      .alphaBlendOp = VK_BLEND_OP_ADD,
      .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
  };

  VkPushConstantRange pushConstantRange{
      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
      .offset = 0,
      .size = sizeof(OutlinePushConstants),
  };

  PipelineDescription description{
      .VertexShader = shader->GetVertexShaderModule(),
      .FragmentShader = shader->GetFragmentShaderModule(),
      .CullMode = VK_CULL_MODE_NONE,
      .DepthTest = VK_FALSE,
      .DepthWrite = VK_FALSE,
      .ColorFormats = {m_Context->Swapchain.Format},
      .BlendAttachments = {blendAttachment},
      .DescriptorSetLayouts = {m_OutlineDescriptorSetLayout},
      .PushConstantRanges = {pushConstantRange},
  };

  m_OutlinePipeline.Init(m_Context->Device, description);
}

void Renderer::CreateOutlineDescriptorResources() {
  VkSamplerCreateInfo samplerInfo{
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .magFilter = VK_FILTER_NEAREST,
      .minFilter = VK_FILTER_NEAREST,
      .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
  };
  if (vkCreateSampler(m_Context->Device, &samplerInfo, nullptr,
                      &m_OutlineSampler) != VK_SUCCESS) {
    throw std::runtime_error("Failed To Create Outline Sampler");
  }

  VkDescriptorSetLayoutBinding entityIDBufferBinding{
      .binding = 0,
      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
  };

  VkDescriptorSetLayoutCreateInfo layoutInfo{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = 1,
      .pBindings = &entityIDBufferBinding,
  };

  if (vkCreateDescriptorSetLayout(m_Context->Device, &layoutInfo, nullptr,
                                  &m_OutlineDescriptorSetLayout) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed to Create Outline Descriptor Set Layout");
  }

  VkDescriptorPoolSize poolSize{
      .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .descriptorCount = MAX_FRAMES_IN_FLIGHT,
  };
  VkDescriptorPoolCreateInfo poolInfo{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .maxSets = MAX_FRAMES_IN_FLIGHT,
      .poolSizeCount = 1,
      .pPoolSizes = &poolSize,
  };

  if (vkCreateDescriptorPool(m_Context->Device, &poolInfo, nullptr,
                             &m_OutlineDescriptorPool) != VK_SUCCESS) {
    throw std::runtime_error("Failed To Create Outline Descriptor Pool");
  }

  std::array<VkDescriptorSetLayout, MAX_FRAMES_IN_FLIGHT> layouts;
  layouts.fill(m_OutlineDescriptorSetLayout);

  VkDescriptorSetAllocateInfo allocInfo{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = m_OutlineDescriptorPool,
      .descriptorSetCount = MAX_FRAMES_IN_FLIGHT,
      .pSetLayouts = layouts.data(),
  };

  if (vkAllocateDescriptorSets(m_Context->Device, &allocInfo,
                               m_OutlineDescriptorSets.data()) != VK_SUCCESS) {
    throw std::runtime_error("Failed To Allocate Outline DescriptorSets");
  }

  UpdateOutlineDescriptorSets();
}

void Renderer::AllocateCommandBuffer() {
  VkCommandBufferAllocateInfo allocInfo{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = m_Context->GraphicsCommandPool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1,
  };

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    if (vkAllocateCommandBuffers(m_Context->Device, &allocInfo,
                                 &m_Frames[i].CommandBuffer) != VK_SUCCESS) {
      throw std::runtime_error("Failed To Allocate Command Buffers");
    }
  }
}

void Renderer::CreateSyncObjects() {
  VkSemaphoreCreateInfo semaphoreInfo{
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
  };
  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    if (vkCreateSemaphore(m_Context->Device, &semaphoreInfo, nullptr,
                          &m_Frames[i].PresentCompleteSemaphore) !=
        VK_SUCCESS) {
      throw std::runtime_error("Failed To Create Present Complete Semaphore");
    }
    VkFenceCreateInfo fenceInfo{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };
    if (vkCreateFence(m_Context->Device, &fenceInfo, nullptr,
                      &m_Frames[i].DrawFence) != VK_SUCCESS) {
      throw std::runtime_error("Failed To Create Draw Fence");
    }
  }

  size_t swapchanSize = m_Context->Swapchain.Images.size();
  m_RenderFinishedSemaphores.resize(swapchanSize);
  for (size_t i = 0; i < swapchanSize; ++i) {
    if (vkCreateSemaphore(m_Context->Device, &semaphoreInfo, nullptr,
                          &m_RenderFinishedSemaphores[i]) != VK_SUCCESS) {
      throw std::runtime_error("Failed To Create Render Finished Semaphore");
    }
  }
}

void Renderer::TransitionImageLayout(VkCommandBuffer cmd, VkImage image,
                                     VkImageAspectFlags aspect,
                                     VkImageLayout oldLayout,
                                     VkImageLayout newLayout,
                                     VkAccessFlags2 srcAccessMask,
                                     VkAccessFlags2 dstAccessMask,
                                     VkPipelineStageFlags2 srcStageMask,
                                     VkPipelineStageFlags2 dstStageMask) const {
  VkImageMemoryBarrier2 barrier{
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
      .srcStageMask = srcStageMask,
      .srcAccessMask = srcAccessMask,
      .dstStageMask = dstStageMask,
      .dstAccessMask = dstAccessMask,
      .oldLayout = oldLayout,
      .newLayout = newLayout,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = image,
      .subresourceRange{
          .aspectMask = aspect,
          .baseMipLevel = 0,
          .levelCount = 1,
          .baseArrayLayer = 0,
          .layerCount = 1,
      },
  };
  VkDependencyInfo dependencyInfo{
      .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
      .dependencyFlags = 0,
      .imageMemoryBarrierCount = 1,
      .pImageMemoryBarriers = &barrier,
  };

  vkCmdPipelineBarrier2(cmd, &dependencyInfo);
}

void Renderer::RecordForwardPass(const std::vector<Entity *> &entities) {

  VkCommandBufferBeginInfo beginInfo{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };

  VkCommandBuffer cmd = m_Frames[m_FrameIndex].CommandBuffer;

  if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) {
    throw std::runtime_error("Failed To start Forward Pass Command Buffer");
  }

  TransitionImageLayout(cmd, m_Context->Swapchain.Images[m_ImageIndex],
                        VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, {},
                        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);

  TransitionImageLayout(cmd, m_Frames[m_FrameIndex].DepthImage.GetImage(),
                        VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, {},
                        VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                        VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
                        VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT);

  TransitionImageLayout(cmd,
                        m_Frames[m_FrameIndex].EntityPickingImage.GetImage(),
                        VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, {},
                        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);

  VkClearValue clearColor{
      .color = {{0.14f, 0.14f, 0.14f, 1.0f}},
  };

  VkClearValue clearEntityPicking{
      .color =
          {
              .uint32 = {0, 0, 0, 0},
          },
  };

  std::array<VkRenderingAttachmentInfo, 2> colorAttachments{};
  colorAttachments[0] = {
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
      .imageView = m_Context->Swapchain.ImageViews[m_ImageIndex],
      .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .clearValue = clearColor,
  };
  colorAttachments[1] = {
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
      .imageView = m_Frames[m_FrameIndex].EntityPickingImage.GetView(),
      .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .clearValue = clearEntityPicking,
  };

  VkClearValue clearDepth{
      .depthStencil = {1.0f, 0},
  };
  VkRenderingAttachmentInfo depthAttachment{
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
      .imageView = m_Frames[m_FrameIndex].DepthImage.GetView(),
      .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .clearValue = clearDepth,
  };

  VkRenderingInfo renderingInfo{
      .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
      .renderArea = {.offset = {0, 0}, .extent = m_Context->Swapchain.Extent},
      .layerCount = 1,
      .colorAttachmentCount = static_cast<uint32_t>(colorAttachments.size()),
      .pColorAttachments = colorAttachments.data(),
      .pDepthAttachment = &depthAttachment,
  };

  vkCmdBeginRendering(cmd, &renderingInfo);

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    m_ForwardPipeline.Handle);

  VkViewport viewport{
      .x = 0.0f,
      .y = 0.0f,
      .width = static_cast<float>(m_Context->Swapchain.Extent.width),
      .height = static_cast<float>(m_Context->Swapchain.Extent.height),
      .minDepth = 0.0f,
      .maxDepth = 1.0f,
  };
  vkCmdSetViewport(cmd, 0, 1, &viewport);

  VkRect2D scissor{
      .offset{0, 0},
      .extent = m_Context->Swapchain.Extent,
  };
  vkCmdSetScissor(cmd, 0, 1, &scissor);

  for (auto *entity : entities) {
    MeshComponent *meshComponent = entity->GetComponent<MeshComponent>();
    if (!meshComponent)
      continue;
    auto texture = meshComponent->GetTexture();
    if (!texture)
      continue;

    glm::mat4 model =
        entity->GetComponent<TransformComponent>()->GetTransformmatrix();

    MeshPushConstants push{
        .Mvp = m_ActiveCamera.Proj * m_ActiveCamera.View * model,
        .Model = model,
        .EntityID = entity->GetID(),
    };

    vkCmdPushConstants(cmd, m_ForwardPipeline.Layout,
                       VK_SHADER_STAGE_VERTEX_BIT |
                           VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(MeshPushConstants), &push);

    VkBuffer vertexBuffers[] = {
        meshComponent->GetMesh()->GetVertexBuffer()->Get()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(
        cmd, meshComponent->GetMesh()->GetIndexBuffer()->Get(), 0,
        meshComponent->GetMesh()->GetIndexBuffer()->GetIndexType());

    VkDescriptorSet textureSet = texture->GetDescriptorSet();
    // TODO: Fix This On multithreading
    if (textureSet == VK_NULL_HANDLE) {
      texture->CreateDescriptorSet(m_TextureDescriptorPool,
                                   m_TextureDescriptorSetLayout);
      textureSet = texture->GetDescriptorSet();
    }
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_ForwardPipeline.Layout, 0, 1, &textureSet, 0,
                            nullptr);

    vkCmdDrawIndexed(cmd, meshComponent->GetMesh()->GetIndexCount(), 1, 0, 0,
                     0);
  }

  // Drawing Grid
  {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      m_GridPipeline.Handle);

    // TODO: Inverse should not be calculated per frame
    GridPushConstants gridPushConstants{
        .View = m_ActiveCamera.View,
        .Proj = m_ActiveCamera.Proj,
        .ViewInv = glm::inverse(m_ActiveCamera.View),
        .ProjInv = glm::inverse(m_ActiveCamera.Proj),
    };
    vkCmdPushConstants(cmd, m_GridPipeline.Layout,
                       VK_SHADER_STAGE_VERTEX_BIT |
                           VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(GridPushConstants), &gridPushConstants);

    vkCmdDraw(cmd, 6, 1, 0, 0);
  }

  // Rendering End

  vkCmdEndRendering(cmd);

  RecordOutlinePass(cmd, m_Frames[m_FrameIndex]);

  TransitionImageLayout(
      cmd, m_Context->Swapchain.Images[m_ImageIndex], VK_IMAGE_ASPECT_COLOR_BIT,
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
      VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, {},
      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
      VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT);

  vkEndCommandBuffer(cmd);
};

void Renderer::RecordOutlinePass(VkCommandBuffer cmd, FrameData &frame) {
  TransitionImageLayout(
      cmd, frame.EntityPickingImage.GetImage(), VK_IMAGE_ASPECT_COLOR_BIT,
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_2_SHADER_READ_BIT,
      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
      VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);

  VkRenderingAttachmentInfo colorAttachment{
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
      .imageView = m_Context->Swapchain.ImageViews[m_ImageIndex],
      .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
  };

  VkRenderingInfo renderingInfo{
      .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
      .renderArea =
          {
              .offset = {0, 0},
              .extent = m_Context->Swapchain.Extent,
          },
      .layerCount = 1,
      .colorAttachmentCount = 1,
      .pColorAttachments = &colorAttachment,
  };

  vkCmdBeginRendering(cmd, &renderingInfo);
  uint32_t selectedEntity = m_EditorSystem->GetSelectedEntity();

  if (selectedEntity != Entity::NULL_ENTITY) {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      m_OutlinePipeline.Handle);

    VkViewport viewport{
        .width = static_cast<float>(m_Context->Swapchain.Extent.width),
        .height = static_cast<float>(m_Context->Swapchain.Extent.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{
        .extent = m_Context->Swapchain.Extent,
    };
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_OutlinePipeline.Layout, 0, 1,
                            &m_OutlineDescriptorSets[m_FrameIndex], 0, nullptr);

    OutlinePushConstants pushConstants{
        .Color = {1.0f, 0.6f, 0.0f},
        .EntityID = selectedEntity,
        .ThicknessPX = 2,
    };
    vkCmdPushConstants(cmd, m_OutlinePipeline.Layout,
                       VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pushConstants),
                       &pushConstants);

    vkCmdDraw(cmd, 3, 1, 0, 0);
  }

  vkCmdEndRendering(cmd);
}

void Renderer::UpdateOutlineDescriptorSets() {
  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    VkDescriptorImageInfo imageInfo{
        .sampler = m_OutlineSampler,
        .imageView = m_Frames[i].EntityPickingImage.GetView(),
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    VkWriteDescriptorSet write{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = m_OutlineDescriptorSets[i],
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &imageInfo,
    };
    vkUpdateDescriptorSets(m_Context->Device, 1, &write, 0, nullptr);
  }
}

void Renderer::Resize() {
  if (m_Context->Swapchain.Extent.width == 0 ||
      m_Context->Swapchain.Extent.height == 0) {
    return;
  }

  vkDeviceWaitIdle(m_Context->Device);
  m_Context->RecreateSwapchain();

  ImageSpec depthImageSpec{
      .Width = m_Context->Swapchain.Extent.width,
      .Height = m_Context->Swapchain.Extent.height,
      .MipLevels = 1,
      .Format = VK_FORMAT_D32_SFLOAT,
      .Usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
               VK_IMAGE_USAGE_SAMPLED_BIT,
      .Aspect = VK_IMAGE_ASPECT_DEPTH_BIT,
  };

  ImageSpec pickingImageSpec{
      .Width = m_Context->Swapchain.Extent.width,
      .Height = m_Context->Swapchain.Extent.height,
      .MipLevels = 1,
      .Format = VK_FORMAT_R32_UINT,
      .Usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
               VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      .Aspect = VK_IMAGE_ASPECT_COLOR_BIT,
  };

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    Image newDepth = Image(m_Context, depthImageSpec);
    Image newPicking = Image(m_Context, pickingImageSpec);
    m_Frames[i].DepthImage = std::move(newDepth);
    m_Frames[i].EntityPickingImage = std::move(newPicking);
  }

  UpdateOutlineDescriptorSets();

  m_Resized = false;
}

// Event Methods
std::optional<uint32_t> Renderer::PickEntity(int32_t mouseX,
                                             int32_t mouseY) const {
  uint32_t lastFrameIndex =
      (m_FrameIndex + MAX_FRAMES_IN_FLIGHT - 1) % MAX_FRAMES_IN_FLIGHT;
  VkImage pickingImage = m_Frames[lastFrameIndex].EntityPickingImage.GetImage();

  uint32_t width = m_Context->Swapchain.Extent.width;
  uint32_t height = m_Context->Swapchain.Extent.height;

  VkDeviceSize size = width * height * sizeof(int32_t);

  Buffer stagingBuffer(m_Context, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  VkCommandBuffer cmdBuffer = VulkanUtils::BeginSingleTimeCommands(m_Context);

  TransitionImageLayout(
      cmdBuffer, pickingImage, VK_IMAGE_ASPECT_COLOR_BIT,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_2_SHADER_READ_BIT,
      VK_ACCESS_2_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
      VK_PIPELINE_STAGE_2_COPY_BIT);

  VkBufferImageCopy copyRegion{
      .bufferOffset = 0,
      .bufferRowLength = 0,
      .bufferImageHeight = 0,
      .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
      .imageOffset = {0, 0, 0},
      .imageExtent = {width, height, 1},
  };

  vkCmdCopyImageToBuffer(cmdBuffer, pickingImage,
                         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                         stagingBuffer.Get(), 1, &copyRegion);

  VulkanUtils::EndSingleTimeCommands(m_Context, cmdBuffer);

  void *mapped;
  vkMapMemory(m_Context->Device, stagingBuffer.GetMemory(), 0, size, 0,
              &mapped);

  uint32_t *data = static_cast<uint32_t *>(mapped);
  uint32_t entityID = data[mouseY * width + mouseX];
  vkUnmapMemory(m_Context->Device, stagingBuffer.GetMemory());

  INFERNO_LOG_INFO("EntityID:{}", entityID);

  if (entityID == Entity::NULL_ENTITY) {
    return std::nullopt;
  }

  return std::make_optional(entityID);
}
} // namespace Inferno
