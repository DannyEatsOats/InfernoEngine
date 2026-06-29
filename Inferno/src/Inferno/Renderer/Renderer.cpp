#include "Inferno/Core/Memory.h"
#include "Inferno/ECS/Entity.h"
#include "Inferno/Renderer/Image.h"
#include "Inferno/Renderer/Mesh.h"
#include "glm/ext/matrix_float4x4.hpp"
#include "glm/matrix.hpp"
#include "tracy/Tracy.hpp"
#include <cstdint>
#include <stdexcept>
#include <vector>
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <GLFW/glfw3.h>
#include <pch.h>
#include <vulkan/vulkan_core.h>

#include "Inferno/ECS/Component.h"
#include "Inferno/Renderer/CullingSystem.h"
#include "Inferno/Resource/ResourceManager.h"
#include "Renderer.h"

#include <glm/glm.hpp>

namespace Inferno {
struct MeshPushConstants {
  glm::mat4 Mvp;
  glm::mat4 Model;
};

void Renderer::StartUp(ResourceManager *resourceManager) {
  m_CullingSystem = MakeScope<CullingSystem>();
  m_ResourceManager = resourceManager;

  CreateForwardPipeline();
  CreateGridPipeline();
  AllocateCommandBuffer();
  CreateSyncObjects();
  // TODO: SET CAMERA
}

void Renderer::ShutDown() {
  if (m_Context && m_Context->Device != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(m_Context->Device);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
      vkDestroyFence(m_Context->Device, m_DrawFences[i], nullptr);
    }

    size_t swapchanSize = m_Context->Swapchain.Images.size();
    for (size_t i = 0; i < swapchanSize; ++i) {
      vkDestroySemaphore(m_Context->Device, m_RenderFinishedSemaphores[i],
                         nullptr);
    }

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
      vkDestroySemaphore(m_Context->Device, m_PresentCompleteSemaphores[i],
                         nullptr);
    }

    vkDestroyDescriptorSetLayout(m_Context->Device,
                                 m_TextureDescriptorSetLayout, nullptr);

    vkDestroyDescriptorPool(m_Context->Device, m_TextureDescriptorPool,
                            nullptr);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
      m_DepthImages[i] = Image();
    }

    vkDestroyPipelineLayout(m_Context->Device, m_GridLayout, nullptr);
    vkDestroyPipeline(m_Context->Device, m_GridPipeline, nullptr);

    vkDestroyPipelineLayout(m_Context->Device, m_ForwardLayout, nullptr);
    vkDestroyPipeline(m_Context->Device, m_ForwardPipeline, nullptr);
  }
}

void Renderer::Render(const std::vector<Entity *> &entities) {
  if (m_Resized) {
    Resize();
  }

  m_VisibleEntites = entities;
  bool success = true;

  // Draw Frame
  if (vkWaitForFences(m_Context->Device, 1, &m_DrawFences[m_FrameIndex],
                      VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
    throw std::runtime_error("Failed to Wait on Draw Fence");
  }

  {
    ZoneScopedN("FelRobban a Fing");
    auto [result, imageIndex] =
        m_Context->AcquireNextImage(m_PresentCompleteSemaphores[m_FrameIndex]);
    m_ImageIndex = imageIndex;

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
      SignalResize();
      return;
    } else if (result != VK_SUCCESS) {
      throw std::runtime_error("Failed to acquire swapchain image!");
    }
    vkResetFences(m_Context->Device, 1, &m_DrawFences[m_FrameIndex]);
  }

  RecordForwardPass();

  VkPipelineStageFlags waitDstStageMask =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

  VkSubmitInfo submitInfo{
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &m_PresentCompleteSemaphores[m_FrameIndex],
      .pWaitDstStageMask = &waitDstStageMask,
      .commandBufferCount = 1,
      .pCommandBuffers = &m_CommandBuffers[m_FrameIndex],
      .signalSemaphoreCount = 1,
      .pSignalSemaphores = &m_RenderFinishedSemaphores[m_ImageIndex],
  };

  if (vkQueueSubmit(m_Context->GraphicsQueue, 1, &submitInfo,
                    m_DrawFences[m_FrameIndex]) != VK_SUCCESS) {
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

  if (vkQueuePresentKHR(m_Context->PresentQueue, &presentInfo) != VK_SUCCESS) {
    throw std::runtime_error("Failed To Submit To Present Queue");
  }

  m_FrameIndex = (m_FrameIndex + 1) % MAX_FRAMES_IN_FLIGHT;

  // =============================
  if (!success) {
    Resize();
  }
}

void Renderer::CreateForwardPipeline() {
  ImageSpec imageSpec{
      .Width = m_Context->Swapchain.Extent.width,
      .Height = m_Context->Swapchain.Extent.height,
      .MipLevels = 1,
      .Format = VK_FORMAT_D32_SFLOAT,
      .Usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
               VK_IMAGE_USAGE_SAMPLED_BIT,
      .Aspect = VK_IMAGE_ASPECT_DEPTH_BIT,
  };

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    m_DepthImages[i] = Image(m_Context, imageSpec);
  }

  // Shader Stages
  auto *shader = m_ResourceManager->Load<Shader>("test");

  VkPipelineShaderStageCreateInfo vertexShaderInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_VERTEX_BIT,
      .module = shader->GetVertexShaderModule(),
      .pName = "main",
  };

  VkPipelineShaderStageCreateInfo fragmentShaderInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
      .module = shader->GetFragmentShaderModule(),
      .pName = "main",
  };
  VkPipelineShaderStageCreateInfo shaderStages[]{vertexShaderInfo,
                                                 fragmentShaderInfo};

  // Vertex Input
  auto meshVertex = MeshVertex::GetLayout();
  VkVertexInputBindingDescription bindingDescription =
      meshVertex.GetBindingDescription();
  std::vector<VkVertexInputAttributeDescription> attributeDescriptions =
      meshVertex.GetAttributeDescriptions();

  VkPipelineVertexInputStateCreateInfo vertexInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .vertexBindingDescriptionCount = 1,
      .pVertexBindingDescriptions = &bindingDescription,
      .vertexAttributeDescriptionCount =
          static_cast<uint32_t>(attributeDescriptions.size()),
      .pVertexAttributeDescriptions = attributeDescriptions.data(),
  };

  // Input Assembly
  VkPipelineInputAssemblyStateCreateInfo assemblyInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      .primitiveRestartEnable = VK_FALSE,
  };

  // Viewport (ignoring cuz of dynamic states dawgh)
  VkPipelineViewportStateCreateInfo viewportInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .viewportCount = 1,
      .pViewports = nullptr,
      .scissorCount = 1,
      .pScissors = nullptr,
  };

  // Rasterizer
  VkPipelineRasterizationStateCreateInfo rasterizationInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .depthClampEnable = VK_FALSE,
      .rasterizerDiscardEnable = VK_FALSE,
      .polygonMode = VK_POLYGON_MODE_FILL,
      .cullMode = VK_CULL_MODE_BACK_BIT,
      .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
      .depthBiasEnable = VK_FALSE,
      .lineWidth = 1.0f,
  };

  // Multisampling
  VkPipelineMultisampleStateCreateInfo multisampleInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
      .sampleShadingEnable = VK_FALSE,
  };

  // Depth Stencil
  VkPipelineDepthStencilStateCreateInfo depthInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
      .depthTestEnable = VK_TRUE,
      .depthWriteEnable = VK_TRUE,
      .depthCompareOp = VK_COMPARE_OP_LESS,
      .stencilTestEnable = VK_FALSE,
  };

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

  VkPipelineColorBlendStateCreateInfo colorBlendInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .logicOpEnable = VK_FALSE,
      .logicOp = VK_LOGIC_OP_COPY,
      .attachmentCount = 1,
      .pAttachments = &colorBlendAttachment,
  };

  // Pipeline Layout
  VkPushConstantRange pushConstantRange{
      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
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

  VkPipelineLayoutCreateInfo layoutInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = 1,
      .pSetLayouts = &m_TextureDescriptorSetLayout,
      .pushConstantRangeCount = 1,
      .pPushConstantRanges = &pushConstantRange,
  };

  if (vkCreatePipelineLayout(m_Context->Device, &layoutInfo, nullptr,
                             &m_ForwardLayout) != VK_SUCCESS) {
    throw std::runtime_error("Failed To Create Forward Pipeline");
  }

  // Dynamic Rendering
  VkPipelineRenderingCreateInfo renderingInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
      .colorAttachmentCount = 1,
      .pColorAttachmentFormats = &m_Context->Swapchain.Format,
      .depthAttachmentFormat =
          VK_FORMAT_D32_SFLOAT, // TODO: Query this in startup
  };

  // Dynamic State
  std::vector<VkDynamicState> dynamicState{VK_DYNAMIC_STATE_VIEWPORT,
                                           VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamicStateInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      .dynamicStateCount = static_cast<uint32_t>(dynamicState.size()),
      .pDynamicStates = dynamicState.data(),
  };

  // Pipeline Creation
  VkGraphicsPipelineCreateInfo pipelineInfo{
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .pNext = &renderingInfo,
      .stageCount = 2,
      .pStages = shaderStages,
      .pVertexInputState = &vertexInfo,
      .pInputAssemblyState = &assemblyInfo,
      .pViewportState = &viewportInfo,
      .pRasterizationState = &rasterizationInfo,
      .pMultisampleState = &multisampleInfo,
      .pDepthStencilState = &depthInfo,
      .pColorBlendState = &colorBlendInfo,
      .pDynamicState = &dynamicStateInfo,
      .layout = m_ForwardLayout,
      .renderPass = nullptr,
  };

  if (vkCreateGraphicsPipelines(m_Context->Device, nullptr, 1, &pipelineInfo,
                                nullptr, &m_ForwardPipeline) != VK_SUCCESS) {
    throw std::runtime_error("Failed To Create Forward Pipeline");
  }
}

void Renderer::CreateGridPipeline() {
  auto *shader = m_ResourceManager->Load<Shader>("grid");

  // Shader Stages
  VkPipelineShaderStageCreateInfo shaderStages[]{
      {
          .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage = VK_SHADER_STAGE_VERTEX_BIT,
          .module = shader->GetVertexShaderModule(),
          .pName = "main",
      },
      {
          .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
          .module = shader->GetFragmentShaderModule(),
          .pName = "main",
      }};

  // Vertex Input
  VkPipelineVertexInputStateCreateInfo vertexInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .vertexBindingDescriptionCount = 0,
      .vertexAttributeDescriptionCount = 0,
  };

  // Input Assembly
  VkPipelineInputAssemblyStateCreateInfo assemblyInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      .primitiveRestartEnable = VK_FALSE,
  };

  // Viewport (ignoring cuz of dynamic states dawgh)
  VkPipelineViewportStateCreateInfo viewportInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .viewportCount = 1,
      .pViewports = nullptr,
      .scissorCount = 1,
      .pScissors = nullptr,
  };

  // Rasterizer
  VkPipelineRasterizationStateCreateInfo rasterizationInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .depthClampEnable = VK_FALSE,
      .rasterizerDiscardEnable = VK_FALSE,
      .polygonMode = VK_POLYGON_MODE_FILL,
      .cullMode = VK_CULL_MODE_NONE,
      .depthBiasEnable = VK_FALSE,
      .lineWidth = 1.0f,
  };

  // Multisampling
  VkPipelineMultisampleStateCreateInfo multisampleInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
      .sampleShadingEnable = VK_FALSE,
  };

  // Depth Stencil
  VkPipelineDepthStencilStateCreateInfo depthInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
      .depthTestEnable = VK_TRUE,
      .depthWriteEnable = VK_TRUE,
      .depthCompareOp = VK_COMPARE_OP_LESS,
      .stencilTestEnable = VK_FALSE,
  };

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

  VkPipelineColorBlendStateCreateInfo colorBlendInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .logicOpEnable = VK_FALSE,
      .logicOp = VK_LOGIC_OP_COPY,
      .attachmentCount = 1,
      .pAttachments = &colorBlendAttachment,
  };

  // Pipeline Layout
  VkPushConstantRange pushConstantRange{
      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
      .offset = 0,
      .size = sizeof(GridPushConstants),
  };

  VkPipelineLayoutCreateInfo layoutInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = 0,
      .pushConstantRangeCount = 1,
      .pPushConstantRanges = &pushConstantRange,
  };

  if (vkCreatePipelineLayout(m_Context->Device, &layoutInfo, nullptr,
                             &m_GridLayout) != VK_SUCCESS) {
    throw std::runtime_error("Failed To Create Forward Pipeline");
  }

  // Dynamic Rendering
  VkPipelineRenderingCreateInfo renderingInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
      .colorAttachmentCount = 1,
      .pColorAttachmentFormats = &m_Context->Swapchain.Format,
      .depthAttachmentFormat =
          VK_FORMAT_D32_SFLOAT, // TODO: Query this in startup
  };

  // Dynamic State
  std::vector<VkDynamicState> dynamicState{VK_DYNAMIC_STATE_VIEWPORT,
                                           VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamicStateInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      .dynamicStateCount = static_cast<uint32_t>(dynamicState.size()),
      .pDynamicStates = dynamicState.data(),
  };

  // Pipeline Creation
  VkGraphicsPipelineCreateInfo pipelineInfo{
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .pNext = &renderingInfo,
      .stageCount = 2,
      .pStages = shaderStages,
      .pVertexInputState = &vertexInfo,
      .pInputAssemblyState = &assemblyInfo,
      .pViewportState = &viewportInfo,
      .pRasterizationState = &rasterizationInfo,
      .pMultisampleState = &multisampleInfo,
      .pDepthStencilState = &depthInfo,
      .pColorBlendState = &colorBlendInfo,
      .pDynamicState = &dynamicStateInfo,
      .layout = m_GridLayout,
      .renderPass = nullptr,
  };

  if (vkCreateGraphicsPipelines(m_Context->Device, nullptr, 1, &pipelineInfo,
                                nullptr, &m_GridPipeline) != VK_SUCCESS) {
    throw std::runtime_error("Failed To Create Grid Pipeline");
  }
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
                                 &m_CommandBuffers[i]) != VK_SUCCESS) {
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
                          &m_PresentCompleteSemaphores[i]) != VK_SUCCESS) {
      throw std::runtime_error("Failed To Create Present Complete Semaphore");
    }
    VkFenceCreateInfo fenceInfo{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };
    if (vkCreateFence(m_Context->Device, &fenceInfo, nullptr,
                      &m_DrawFences[i]) != VK_SUCCESS) {
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

void Renderer::TransitionImageLayout(VkImage image, VkImageAspectFlags aspect,
                                     VkImageLayout oldLayout,
                                     VkImageLayout newLayout,
                                     VkAccessFlags2 srcAccessMask,
                                     VkAccessFlags2 dstAccessMask,
                                     VkPipelineStageFlags2 srcStageMask,
                                     VkPipelineStageFlags2 dstStageMask) {
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

  vkCmdPipelineBarrier2(m_CommandBuffers[m_FrameIndex], &dependencyInfo);
}

void Renderer::RecordForwardPass() {

  VkCommandBufferBeginInfo beginInfo{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };

  if (vkBeginCommandBuffer(m_CommandBuffers[m_FrameIndex], &beginInfo) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed To start Forward Pass Command Buffer");
  }

  TransitionImageLayout(m_Context->Swapchain.Images[m_ImageIndex],
                        VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, {},
                        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);

  TransitionImageLayout(m_DepthImages[m_FrameIndex].GetImage(),
                        VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, {},
                        VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                        VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
                        VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT);

  VkClearValue clearColor{
      .color = {{0.14f, 0.14f, 0.14f, 1.0f}},
  };
  VkRenderingAttachmentInfo colorAttachment{
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
      .imageView = m_Context->Swapchain.ImageViews[m_ImageIndex],
      .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .clearValue = clearColor,
  };

  VkClearValue clearDepth{
      .depthStencil = {1.0f, 0},
  };
  VkRenderingAttachmentInfo depthAttachment{
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
      .imageView = m_DepthImages[m_FrameIndex].GetView(),
      .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .clearValue = clearDepth,
  };

  VkRenderingInfo renderingInfo{
      .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
      .renderArea = {.offset = {0, 0}, .extent = m_Context->Swapchain.Extent},
      .layerCount = 1,
      .colorAttachmentCount = 1,
      .pColorAttachments = &colorAttachment,
      .pDepthAttachment = &depthAttachment,
  };

  vkCmdBeginRendering(m_CommandBuffers[m_FrameIndex], &renderingInfo);

  vkCmdBindPipeline(m_CommandBuffers[m_FrameIndex],
                    VK_PIPELINE_BIND_POINT_GRAPHICS, m_ForwardPipeline);

  VkViewport viewport{
      .x = 0.0f,
      .y = 0.0f,
      .width = static_cast<float>(m_Context->Swapchain.Extent.width),
      .height = static_cast<float>(m_Context->Swapchain.Extent.height),
      .minDepth = 0.0f,
      .maxDepth = 1.0f,
  };
  vkCmdSetViewport(m_CommandBuffers[m_FrameIndex], 0, 1, &viewport);

  VkRect2D scissor{
      .offset{0, 0},
      .extent = m_Context->Swapchain.Extent,
  };
  vkCmdSetScissor(m_CommandBuffers[m_FrameIndex], 0, 1, &scissor);

  // Mock Camera Setup
  glm::mat4 proj =
      glm::perspective(glm::radians(45.0f),
                       (float)m_Context->Swapchain.Extent.width /
                           (float)m_Context->Swapchain.Extent.height,
                       0.1f, 10.0f);
  proj[1][1] *= -1;
  glm::mat4 view =
      glm::lookAt(glm::vec3(0.0f, 1.0f, 3.0f), glm::vec3(0.0f, 0.0f, 0.0f),
                  glm::vec3(0.0f, 1.0f, 0.0f));

  for (auto *entity : m_VisibleEntites) {
    MeshComponent *meshComponent = entity->GetComponent<MeshComponent>();
    if (!meshComponent)
      continue;
    auto texture = meshComponent->GetTexture();
    if (!texture)
      continue;

    glm::mat4 model =
        entity->GetComponent<TransformComponent>()->GetTransformmatrix();

    MeshPushConstants push{};
    push.Mvp = proj * view * model;
    push.Model = model;

    vkCmdPushConstants(m_CommandBuffers[m_FrameIndex], m_ForwardLayout,
                       VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(MeshPushConstants),
                       &push);

    VkBuffer vertexBuffers[] = {
        meshComponent->GetMesh()->GetVertexBuffer()->Get()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(m_CommandBuffers[m_FrameIndex], 0, 1, vertexBuffers,
                           offsets);
    vkCmdBindIndexBuffer(
        m_CommandBuffers[m_FrameIndex],
        meshComponent->GetMesh()->GetIndexBuffer()->Get(), 0,
        meshComponent->GetMesh()->GetIndexBuffer()->GetIndexType());

    VkDescriptorSet textureSet = texture->GetDescriptorSet();
    // TODO: Fix This On multithreading
    if (textureSet == VK_NULL_HANDLE) {
      texture->CreateDescriptorSet(m_TextureDescriptorPool,
                                   m_TextureDescriptorSetLayout);
      textureSet = texture->GetDescriptorSet();
    }
    vkCmdBindDescriptorSets(m_CommandBuffers[m_FrameIndex],
                            VK_PIPELINE_BIND_POINT_GRAPHICS, m_ForwardLayout, 0,
                            1, &textureSet, 0, nullptr);

    vkCmdDrawIndexed(m_CommandBuffers[m_FrameIndex],
                     meshComponent->GetMesh()->GetIndexCount(), 1, 0, 0, 0);
  }

  // Drawing Grid
  /*
  vkCmdBindPipeline(m_CommandBuffers[m_FrameIndex],
                    VK_PIPELINE_BIND_POINT_GRAPHICS, m_GridPipeline);

  GridPushConstants gridPushConstants{
      .View = view,
      .Proj = proj,
      .ViewInv = glm::inverse(view),
      .ProjInv = glm::inverse(proj),
  };
  vkCmdPushConstants(m_CommandBuffers[m_FrameIndex], m_GridLayout,
                     VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                     0, sizeof(GridPushConstants), &gridPushConstants);

  vkCmdDraw(m_CommandBuffers[m_FrameIndex], 6, 1, 0, 0);
  */

  // Rendering End

  vkCmdEndRendering(m_CommandBuffers[m_FrameIndex]);

  TransitionImageLayout(
      m_Context->Swapchain.Images[m_ImageIndex], VK_IMAGE_ASPECT_COLOR_BIT,
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
      VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, {},
      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
      VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT);

  vkEndCommandBuffer(m_CommandBuffers[m_FrameIndex]);
};

void Renderer::Resize() {
  if (m_Context->Swapchain.Extent.width == 0 ||
      m_Context->Swapchain.Extent.height == 0) {
    return;
  }

  vkDeviceWaitIdle(m_Context->Device);
  m_Context->RecreateSwapchain();

  ImageSpec imageSpec{
      .Width = m_Context->Swapchain.Extent.width,
      .Height = m_Context->Swapchain.Extent.height,
      .MipLevels = 1,
      .Format = VK_FORMAT_D32_SFLOAT,
      .Usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
               VK_IMAGE_USAGE_SAMPLED_BIT,
      .Aspect = VK_IMAGE_ASPECT_DEPTH_BIT,
  };

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    Image newDepth = Image(m_Context, imageSpec);
    m_DepthImages[i] = std::move(newDepth);
  }

  m_Resized = false;
}
} // namespace Inferno
