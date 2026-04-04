#include <pch.h>
#include <stdexcept>
#include <vulkan/vulkan_core.h>

#include "Inferno/Renderer/Buffer.h"
#include "Inferno/Renderer/RenderingContext.h"
#include "Renderer.h"

#include "Shader.h"

namespace Inferno {
const std::vector<Vertex> vertices = {{{0.0f, -0.5f}, {0.322f, 0.133f, 0.346f}},
                                      {{0.5f, 0.5f}, {0.549f, 0.188f, 0.380f}},
                                      {{-0.5f, 0.5f}, {0.776f, 0.235f, 0.318f}}};

void Renderer::Init() {
  m_Context = RenderingContext::GetContext();
  m_VertexBuffer = VertexBuffer::Create(m_Context.get(), vertices.data(),
                                        sizeof(Vertex) * vertices.size());
  m_VertexBuffer->SetLayout({
      {"inPosition", ShaderDataType::Float2},
      {"inColor", ShaderDataType::Float3},
  });

  CreateRenderPass();
  CreateGraphicsPipeline();
  CreateFramebuffers();
  CreateCommandPool();
  CreateCommandBuffers();
  CreateSyncObjects();
}

void Renderer::ShutDown() {
  if (m_Context->GetDevice() != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(m_Context->GetDevice());
  }

  m_VertexBuffer->Destroy();

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    vkDestroySemaphore(m_Context->GetDevice(), m_RenderFinishedSemaphores[i],
                       nullptr);
    vkDestroySemaphore(m_Context->GetDevice(), m_ImageAvailableSemaphores[i],
                       nullptr);
    vkDestroyFence(m_Context->GetDevice(), m_InFlightFences[i], nullptr);
  }
  vkDestroyCommandPool(m_Context->GetDevice(), m_CommandPool, nullptr);

  for (auto framebuffer : m_SwapChainFrameBuffers) {
    vkDestroyFramebuffer(m_Context->GetDevice(), framebuffer, nullptr);
  }

  vkDestroyPipeline(m_Context->GetDevice(), m_GraphicsPipeline, nullptr);
  vkDestroyPipelineLayout(m_Context->GetDevice(), m_PipelineLayout, nullptr);
  vkDestroyRenderPass(m_Context->GetDevice(), m_RenderPass, nullptr);
}

void Renderer::DrawFrame() {
  vkWaitForFences(m_Context->GetDevice(), 1, &m_InFlightFences[m_CurrentFrame],
                  VK_TRUE, UINT64_MAX);

  uint32_t imageIndex;
  VkResult result = vkAcquireNextImageKHR(
      m_Context->GetDevice(), m_Context->GetSwapChain(), UINT64_MAX,
      m_ImageAvailableSemaphores[m_CurrentFrame], VK_NULL_HANDLE, &imageIndex);

  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    RecreateSwapChain();
    return;
  } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
    throw std::runtime_error("Failed to acquire swapchain image");
  }

  vkResetFences(m_Context->GetDevice(), 1, &m_InFlightFences[m_CurrentFrame]);

  // maybe this could be moved to record command buffer
  vkResetCommandBuffer(m_CommandBuffers[m_CurrentFrame], 0);
  RecordCommandBuffer(m_CommandBuffers[m_CurrentFrame], imageIndex);

  VkSemaphore waitSemaphores[] = {m_ImageAvailableSemaphores[m_CurrentFrame]};
  VkPipelineStageFlags waitStages[] = {
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

  VkSemaphore signalSemaphores[] = {m_RenderFinishedSemaphores[m_CurrentFrame]};

  VkSubmitInfo submitInfo = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = waitSemaphores,
      .pWaitDstStageMask = waitStages,
      .commandBufferCount = 1,
      .pCommandBuffers = &m_CommandBuffers[m_CurrentFrame],
      .signalSemaphoreCount = 1,
      .pSignalSemaphores = signalSemaphores,
  };

  if (vkQueueSubmit(m_Context->GetGraphicsQueue(), 1, &submitInfo,
                    m_InFlightFences[m_CurrentFrame]) != VK_SUCCESS) {
    throw std::runtime_error("Failed to Submit Draw Command Buffer");
  }

  VkSwapchainKHR swapChains[] = {m_Context->GetSwapChain()};

  VkPresentInfoKHR presentInfo = {
      .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = signalSemaphores,
      .swapchainCount = 1,
      .pSwapchains = swapChains,
      .pImageIndices = &imageIndex,
      .pResults = nullptr,
  };

  result = vkQueuePresentKHR(m_Context->GetPresentQueue(), &presentInfo);
  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
      m_FrameBufferResized) {
    m_FrameBufferResized = false;
    RecreateSwapChain();
    return;
  } else if (result != VK_SUCCESS) {
    throw std::runtime_error("Failed to Present Swap Chain Image");
  }

  vkQueueWaitIdle(m_Context->GetPresentQueue());

  m_CurrentFrame = (m_CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Renderer::OnWindowResize(uint32_t width, uint32_t height) {
  m_FrameBufferResized = true;
}

void Renderer::CreateRenderPass() {
  VkAttachmentDescription colorAttachment = {
      .format = m_Context->GetSwapChainImgFormat(),
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
  };

  VkAttachmentReference colorAttachmentRef = {
      .attachment = 0,
      .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
  };

  VkSubpassDescription subpassDesc = {
      .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
      .colorAttachmentCount = 1,
      .pColorAttachments = &colorAttachmentRef,
  };

  VkSubpassDependency dependency = {
      .srcSubpass = VK_SUBPASS_EXTERNAL,
      .dstSubpass = 0,
      .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      .srcAccessMask = 0,
      .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
  };

  VkRenderPassCreateInfo renderPassCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
      .attachmentCount = 1,
      .pAttachments = &colorAttachment,
      .subpassCount = 1,
      .pSubpasses = &subpassDesc,
      .dependencyCount = 1,
      .pDependencies = &dependency,
  };

  if (vkCreateRenderPass(m_Context->GetDevice(), &renderPassCreateInfo, nullptr,
                         &m_RenderPass) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create render pass");
  }
}

void Renderer::CreateGraphicsPipeline() {
  // PIPELINE SHADER STAGE
  auto shader =
      Shader::Create("shader", &m_Context->GetDevice(),
                     "assets/shaders/vert.spv", "assets/shaders/frag.spv");

  VkPipelineShaderStageCreateInfo vertShaderStageInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_VERTEX_BIT,
      .module = shader->GetVertexShaderModule(),
      .pName = "main",
  };

  VkPipelineShaderStageCreateInfo fragShaderStageInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
      .module = shader->GetFragmentShaderModule(),
      .pName = "main",
  };

  VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo,
                                                    fragShaderStageInfo};

  // PIPELINE VERTEX INPUT STATE
  auto bindingDescription = m_VertexBuffer->GetBindingDescription();
  auto attributeDescriptions = m_VertexBuffer->GetAttributeDescriptions();
  VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .vertexBindingDescriptionCount = 1,
      .pVertexBindingDescriptions = &bindingDescription,
      .vertexAttributeDescriptionCount =
          static_cast<uint32_t>(attributeDescriptions.size()),
      .pVertexAttributeDescriptions = attributeDescriptions.data(),
  };

  // PIPELINE INPUT ASSEMBLY
  VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      .primitiveRestartEnable = VK_FALSE,
  };

  // Viewport and Scissor are nullptr because they are recreated every command
  // buffer record
  VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .viewportCount = 1,
      .pViewports = nullptr,
      .scissorCount = 1,
      .pScissors = nullptr,
  };

  // Rasterizer
  VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .depthClampEnable = VK_FALSE,
      .rasterizerDiscardEnable = VK_FALSE,
      .polygonMode = VK_POLYGON_MODE_FILL,
      .cullMode = VK_CULL_MODE_BACK_BIT,
      .frontFace = VK_FRONT_FACE_CLOCKWISE,
      .depthBiasEnable = VK_FALSE,
      .depthBiasConstantFactor = 0.0f,
      .depthBiasClamp = 0.0f,
      .depthBiasSlopeFactor = 0.0f,
      .lineWidth = 1.0f,
  };

  // Multisampling
  VkPipelineMultisampleStateCreateInfo mutlisampleStateCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
      .sampleShadingEnable = VK_FALSE,
      .minSampleShading = 1.0f,
      .pSampleMask = nullptr,
      .alphaToCoverageEnable = VK_FALSE,
      .alphaToOneEnable = VK_FALSE,
  };

  // TODO: Depth Buffering

  // Color Blending
  VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {
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

  VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .logicOpEnable = VK_FALSE,
      .logicOp = VK_LOGIC_OP_COPY,
      .attachmentCount = 1,
      .pAttachments = &colorBlendAttachmentState,
  };

  // Pipeline Layout
  VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = 0,
      .pSetLayouts = nullptr,
      .pushConstantRangeCount = 0,
      .pPushConstantRanges = nullptr,
  };

  if (vkCreatePipelineLayout(m_Context->GetDevice(), &pipelineLayoutCreateInfo,
                             nullptr, &m_PipelineLayout) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create pipeline layout");
  }

  // Dynamic State
  std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
                                               VK_DYNAMIC_STATE_SCISSOR};

  VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
      .pDynamicStates = dynamicStates.data(),
  };

  VkGraphicsPipelineCreateInfo pipelineCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .stageCount = 2,
      .pStages = shaderStages,
      .pVertexInputState = &vertexInputStateCreateInfo,
      .pInputAssemblyState = &inputAssemblyStateCreateInfo,
      .pViewportState = &viewportStateCreateInfo,
      .pRasterizationState = &rasterizationStateCreateInfo,
      .pMultisampleState = &mutlisampleStateCreateInfo,
      .pDepthStencilState = nullptr,
      .pColorBlendState = &colorBlendStateCreateInfo,
      .pDynamicState = &dynamicStateCreateInfo,
      .layout = m_PipelineLayout,
      .renderPass = m_RenderPass,
      .subpass = 0,
      .basePipelineHandle = VK_NULL_HANDLE,
      .basePipelineIndex = -1,
  };

  if (vkCreateGraphicsPipelines(m_Context->GetDevice(), VK_NULL_HANDLE, 1,
                                &pipelineCreateInfo, nullptr,
                                &m_GraphicsPipeline) != VK_SUCCESS) {
    throw std::runtime_error("Failed to Create Graphics Pipeline");
  }
}

void Renderer::CreateFramebuffers() {
  m_SwapChainFrameBuffers.resize(m_Context->GetSwapChainImageViews().size());

  for (size_t i = 0; i < m_Context->GetSwapChainImageViews().size(); ++i) {
    VkImageView attachments[] = {m_Context->GetSwapChainImageViews()[i]};

    VkFramebufferCreateInfo frameBufferCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = m_RenderPass,
        .attachmentCount = 1,
        .pAttachments = attachments,
        .width = m_Context->GetSwapChainExtent().width,
        .height = m_Context->GetSwapChainExtent().height,
        .layers = 1,
    };

    if (vkCreateFramebuffer(m_Context->GetDevice(), &frameBufferCreateInfo,
                            nullptr,
                            &m_SwapChainFrameBuffers[i]) != VK_SUCCESS) {
      throw std::runtime_error("Failed to create Framebuffer!");
    }
  }
}

void Renderer::CreateCommandPool() {
  QueueFamilyIndices queueFamilyIndices =
      m_Context->FindQueueFamilies(m_Context->GetPhysicalDevice());

  VkCommandPoolCreateInfo poolCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = queueFamilyIndices.graphicsFamily.value(),
  };

  if (vkCreateCommandPool(m_Context->GetDevice(), &poolCreateInfo, nullptr,
                          &m_CommandPool) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create command pool!");
  }
}

void Renderer::CreateCommandBuffers() {
  m_CommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

  VkCommandBufferAllocateInfo commandBufferAllocateInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = m_CommandPool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = static_cast<uint32_t>(m_CommandBuffers.size()),
  };

  if (vkAllocateCommandBuffers(m_Context->GetDevice(),
                               &commandBufferAllocateInfo,
                               m_CommandBuffers.data()) != VK_SUCCESS) {
    throw std::runtime_error("Failed to Allocate Command Buffers");
  }
}

void Renderer::CreateSyncObjects() {
  m_ImageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
  m_RenderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
  m_InFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

  VkSemaphoreCreateInfo semaphoreCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
  };

  VkFenceCreateInfo fenceCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
      .flags = VK_FENCE_CREATE_SIGNALED_BIT,
  };

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    if (vkCreateSemaphore(m_Context->GetDevice(), &semaphoreCreateInfo, nullptr,
                          &m_ImageAvailableSemaphores[i]) != VK_SUCCESS ||
        vkCreateSemaphore(m_Context->GetDevice(), &semaphoreCreateInfo, nullptr,
                          &m_RenderFinishedSemaphores[i]) != VK_SUCCESS ||
        vkCreateFence(m_Context->GetDevice(), &fenceCreateInfo, nullptr,
                      &m_InFlightFences[i]) != VK_SUCCESS) {
      throw std::runtime_error("Failed to Create Semaphores");
    }
  }
}

void Renderer::RecordCommandBuffer(VkCommandBuffer commandBuffer,
                                   uint32_t imageIndex) {
  VkCommandBufferBeginInfo commandBufferBeginInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = 0,
      .pInheritanceInfo = nullptr,
  };

  if (vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed to Begin Recording Command Buffer");
  }

  VkRenderPassBeginInfo renderPassBeginInfo = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      .renderPass = m_RenderPass,
      .framebuffer = m_SwapChainFrameBuffers[imageIndex],
  };

  renderPassBeginInfo.renderArea.offset = {0, 0};
  renderPassBeginInfo.renderArea.extent = m_Context->GetSwapChainExtent();

  VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
  renderPassBeginInfo.clearValueCount = 1;
  renderPassBeginInfo.pClearValues = &clearColor;

  vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo,
                       VK_SUBPASS_CONTENTS_INLINE);
  vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    m_GraphicsPipeline);

  VkBuffer vertexBuffers[] = {m_VertexBuffer->GetBuffer()};
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

  VkViewport viewport = {
      .x = 0.0f,
      .y = 0.0f,
      .width = static_cast<float>(m_Context->GetSwapChainExtent().width),
      .height = static_cast<float>(m_Context->GetSwapChainExtent().height),
      .minDepth = 0.0f,
      .maxDepth = 1.0f,
  };

  vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

  VkRect2D scissor = {
      .offset = {0, 0},
      .extent = m_Context->GetSwapChainExtent(),
  };

  vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

  vkCmdDraw(commandBuffer, static_cast<uint32_t>(vertices.size()), 1, 0, 0);
  vkCmdEndRenderPass(commandBuffer);
  vkEndCommandBuffer(commandBuffer);
}

void Renderer::RecreateSwapChain() {
  vkDeviceWaitIdle(m_Context->GetDevice());

  for (auto framebuffer : m_SwapChainFrameBuffers) {
    vkDestroyFramebuffer(m_Context->GetDevice(), framebuffer, nullptr);
  }

  m_Context->RecreateSwapChain();
  CreateFramebuffers();
}
} // namespace Inferno
