#include "Inferno/Renderer/Texture.h"
#include <array>
#include <cstddef>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/fwd.hpp>
#include <glm/trigonometric.hpp>
#include <pch.h>
#include <stdexcept>
#include <vector>
#include <vulkan/vulkan_core.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <chrono>

#include "Inferno/Renderer/Buffer.h"
#include "Inferno/Renderer/RenderData.h"
#include "Inferno/Renderer/RenderingContext.h"
#include "Renderer.h"

#include "Shader.h"

namespace Inferno {
const std::vector<Vertex> vertices = {
    {{-0.5f, -0.5f}, {0.322f, 0.133f, 0.346f}, {1.0f, 0.0f}},
    {{0.5f, -0.5f}, {0.549f, 0.188f, 0.380f}, {0.0f, 0.0f}},
    {{0.5f, 0.5f}, {0.776f, 0.235f, 0.318f}, {0.0f, 1.0f}},
    {{-0.5f, 0.5f}, {0.676f, 0.245f, 0.348f}, {1.0f, 1.0f}}};

const std::vector<uint16_t> indices = {0, 1, 2, 2, 3, 0};

void Renderer::Init() {
  m_pContext = RenderingContext::GetContext();
  m_pContext->CreateCommandPool();

  m_VertexBuffer =
      VertexBuffer::Create(m_pContext.get(), sizeof(Vertex) * vertices.size());

  m_VertexBuffer->SetLayout({
      {"inPosition", ShaderDataType::Float2},
      {"inColor", ShaderDataType::Float3},
      {"inTexCoord", ShaderDataType::Float2},
  });

  m_IndexBuffer =
      IndexBuffer::Create(m_pContext.get(), sizeof(indices[0]) * indices.size(),
                          VK_INDEX_TYPE_UINT16);

  m_Frames.resize(MAX_FRAMES_IN_FLIGHT);
  for (auto &frame : m_Frames) {
    frame.UniBuffer =
        UniformBuffer::Create(m_pContext.get(), sizeof(UniformBufferOjbect));
  }

  m_Texture = Texture::Create2D(m_pContext.get(), "assets/texture/texture.jpg");

  CreateRenderPass();
  CreateDescriptorSetLayout();
  CreateDescriptorPool();
  CreateDescriptorSets();
  CreateGraphicsPipeline();
  CreateFramebuffers();
  CreateCommandBuffers();
  CreateSyncObjects();

  m_VertexBuffer->Upload(vertices.data());
  m_IndexBuffer->Upload(indices.data());

  // ASSET CREATION
}

void Renderer::ShutDown() {
  if (m_pContext->GetDevice() != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(m_pContext->GetDevice());
  }

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    vkDestroySemaphore(m_pContext->GetDevice(), m_Frames[i].RenderFinished,
                       nullptr);
    vkDestroySemaphore(m_pContext->GetDevice(), m_Frames[i].ImageAvailable,
                       nullptr);
    vkDestroyFence(m_pContext->GetDevice(), m_Frames[i].InFlight, nullptr);
  }
  vkDestroyCommandPool(m_pContext->GetDevice(), m_pContext->GetCommandPool(),
                       nullptr);

  for (auto framebuffer : m_SwapChainFrameBuffers) {
    vkDestroyFramebuffer(m_pContext->GetDevice(), framebuffer, nullptr);
  }

  vkDestroyDescriptorPool(m_pContext->GetDevice(), m_DescriptorPool, nullptr);
  vkDestroyDescriptorSetLayout(m_pContext->GetDevice(), m_DescriptorSetLayout,
                               nullptr);
  vkDestroyPipeline(m_pContext->GetDevice(), m_GraphicsPipeline, nullptr);
  vkDestroyPipelineLayout(m_pContext->GetDevice(), m_PipelineLayout, nullptr);
  vkDestroyRenderPass(m_pContext->GetDevice(), m_RenderPass, nullptr);
}

void Renderer::DrawFrame() {
  vkWaitForFences(m_pContext->GetDevice(), 1,
                  &m_Frames[m_CurrentFrame].InFlight, VK_TRUE, UINT64_MAX);

  uint32_t imageIndex;
  VkResult result = vkAcquireNextImageKHR(
      m_pContext->GetDevice(), m_pContext->GetSwapChain(), UINT64_MAX,
      m_Frames[m_CurrentFrame].ImageAvailable, VK_NULL_HANDLE, &imageIndex);

  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    RecreateSwapChain();
    return;
  } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
    throw std::runtime_error("Failed to acquire swapchain image");
  }

  vkResetFences(m_pContext->GetDevice(), 1, &m_Frames[m_CurrentFrame].InFlight);

  // maybe this could be moved to record command buffer
  vkResetCommandBuffer(m_CommandBuffers[m_CurrentFrame], 0);
  RecordCommandBuffer(m_CommandBuffers[m_CurrentFrame], imageIndex);

  VkSemaphore waitSemaphores[] = {m_Frames[m_CurrentFrame].ImageAvailable};
  VkPipelineStageFlags waitStages[] = {
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

  VkSemaphore signalSemaphores[] = {m_Frames[m_CurrentFrame].RenderFinished};

  UpdateUniformBuffer(m_CurrentFrame);

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

  if (vkQueueSubmit(m_pContext->GetGraphicsQueue(), 1, &submitInfo,
                    m_Frames[m_CurrentFrame].InFlight) != VK_SUCCESS) {
    throw std::runtime_error("Failed to Submit Draw Command Buffer");
  }

  VkSwapchainKHR swapChains[] = {m_pContext->GetSwapChain()};

  VkPresentInfoKHR presentInfo = {
      .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = signalSemaphores,
      .swapchainCount = 1,
      .pSwapchains = swapChains,
      .pImageIndices = &imageIndex,
      .pResults = nullptr,
  };

  result = vkQueuePresentKHR(m_pContext->GetPresentQueue(), &presentInfo);
  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
      m_FrameBufferResized) {
    m_FrameBufferResized = false;
    RecreateSwapChain();
    return;
  } else if (result != VK_SUCCESS) {
    throw std::runtime_error("Failed to Present Swap Chain Image");
  }

  vkQueueWaitIdle(m_pContext->GetPresentQueue());

  m_CurrentFrame = (m_CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Renderer::OnWindowResize(uint32_t width, uint32_t height) {
  m_FrameBufferResized = true;
}

void Renderer::CreateRenderPass() {
  VkAttachmentDescription colorAttachment = {
      .format = m_pContext->GetSwapChainImgFormat(),
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

  if (vkCreateRenderPass(m_pContext->GetDevice(), &renderPassCreateInfo,
                         nullptr, &m_RenderPass) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create render pass");
  }
}

void Renderer::CreateDescriptorSetLayout() {
  VkDescriptorSetLayoutBinding uboLayoutBinding = {
      .binding = 0,
      .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
      .pImmutableSamplers = nullptr,
  };

  VkDescriptorSetLayoutBinding samplerLayoutBinding = {
      .binding = 1,
      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
      .pImmutableSamplers = nullptr,
  };

  std::array<VkDescriptorSetLayoutBinding, 2> bindings = {uboLayoutBinding,
                                                          samplerLayoutBinding};

  VkDescriptorSetLayoutCreateInfo layoutInfo = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = static_cast<uint32_t>(bindings.size()),
      .pBindings = bindings.data(),
  };

  if (vkCreateDescriptorSetLayout(m_pContext->GetDevice(), &layoutInfo, nullptr,
                                  &m_DescriptorSetLayout) != VK_SUCCESS) {
    throw std::runtime_error("Failed To Create Descriptor Set Layout");
  };
}

void Renderer::CreateDescriptorPool() {
  std::array<VkDescriptorPoolSize, 2> poolSizes{};
  poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  poolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
  poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  poolSizes[1].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

  VkDescriptorPoolCreateInfo poolInfo = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT),
      .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
      .pPoolSizes = poolSizes.data(),
  };

  if (vkCreateDescriptorPool(m_pContext->GetDevice(), &poolInfo, nullptr,
                             &m_DescriptorPool) != VK_SUCCESS) {
    throw std::runtime_error("Failed To Create Descriptor Pool");
  }
}

void Renderer::CreateDescriptorSets() {
  std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT,
                                             m_DescriptorSetLayout);
  VkDescriptorSetAllocateInfo allocInfo = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = m_DescriptorPool,
      .descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT),
      .pSetLayouts = layouts.data(),
  };

  std::vector<VkDescriptorSet> descriptorSets(MAX_FRAMES_IN_FLIGHT);

  if (vkAllocateDescriptorSets(m_pContext->GetDevice(), &allocInfo,
                               descriptorSets.data()) != VK_SUCCESS) {
    throw std::runtime_error("Failed To Allocate Descriptor Sets");
  }

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    m_Frames[i].DescriptorSet = descriptorSets[i];
  }

  for (auto &frame : m_Frames) {
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = frame.UniBuffer->Get();
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(UniformBufferOjbect);

    std::array<VkWriteDescriptorSet, 2> descriptorWrites{};
    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = frame.DescriptorSet;
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[0].pBufferInfo = &bufferInfo;

    auto imageInfo = m_Texture->GetDescriptorInfo();
    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = frame.DescriptorSet;
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].descriptorType =
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrites[1].pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(m_pContext->GetDevice(),
                           static_cast<uint32_t>(descriptorWrites.size()),
                           descriptorWrites.data(), 0, nullptr);
  }
}

void Renderer::CreateGraphicsPipeline() {
  // PIPELINE SHADER STAGE
  auto shader =
      Shader::Create("shader", &m_pContext->GetDevice(),
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
      .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
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
      .setLayoutCount = 1,
      .pSetLayouts = &m_DescriptorSetLayout,
      .pushConstantRangeCount = 0,
      .pPushConstantRanges = nullptr,
  };

  if (vkCreatePipelineLayout(m_pContext->GetDevice(), &pipelineLayoutCreateInfo,
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

  if (vkCreateGraphicsPipelines(m_pContext->GetDevice(), VK_NULL_HANDLE, 1,
                                &pipelineCreateInfo, nullptr,
                                &m_GraphicsPipeline) != VK_SUCCESS) {
    throw std::runtime_error("Failed to Create Graphics Pipeline");
  }
}

void Renderer::CreateFramebuffers() {
  m_SwapChainFrameBuffers.resize(m_pContext->GetSwapChainImageViews().size());

  for (size_t i = 0; i < m_pContext->GetSwapChainImageViews().size(); ++i) {
    VkImageView attachments[] = {m_pContext->GetSwapChainImageViews()[i]};

    VkFramebufferCreateInfo frameBufferCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = m_RenderPass,
        .attachmentCount = 1,
        .pAttachments = attachments,
        .width = m_pContext->GetSwapChainExtent().width,
        .height = m_pContext->GetSwapChainExtent().height,
        .layers = 1,
    };

    if (vkCreateFramebuffer(m_pContext->GetDevice(), &frameBufferCreateInfo,
                            nullptr,
                            &m_SwapChainFrameBuffers[i]) != VK_SUCCESS) {
      throw std::runtime_error("Failed to create Framebuffer!");
    }
  }
}

void Renderer::CreateCommandBuffers() {
  m_CommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

  VkCommandBufferAllocateInfo commandBufferAllocateInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = m_pContext->GetCommandPool(),
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = static_cast<uint32_t>(m_CommandBuffers.size()),
  };

  if (vkAllocateCommandBuffers(m_pContext->GetDevice(),
                               &commandBufferAllocateInfo,
                               m_CommandBuffers.data()) != VK_SUCCESS) {
    throw std::runtime_error("Failed to Allocate Command Buffers");
  }
}

void Renderer::CreateSyncObjects() {
  VkSemaphoreCreateInfo semaphoreCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
  };

  VkFenceCreateInfo fenceCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
      .flags = VK_FENCE_CREATE_SIGNALED_BIT,
  };

  for (auto &frame : m_Frames) {
    if (vkCreateSemaphore(m_pContext->GetDevice(), &semaphoreCreateInfo,
                          nullptr, &frame.ImageAvailable) != VK_SUCCESS ||
        vkCreateSemaphore(m_pContext->GetDevice(), &semaphoreCreateInfo,
                          nullptr, &frame.RenderFinished) != VK_SUCCESS ||
        vkCreateFence(m_pContext->GetDevice(), &fenceCreateInfo, nullptr,
                      &frame.InFlight) != VK_SUCCESS) {
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
  renderPassBeginInfo.renderArea.extent = m_pContext->GetSwapChainExtent();

  VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
  renderPassBeginInfo.clearValueCount = 1;
  renderPassBeginInfo.pClearValues = &clearColor;

  vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo,
                       VK_SUBPASS_CONTENTS_INLINE);
  vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    m_GraphicsPipeline);

  VkBuffer vertexBuffers[] = {m_VertexBuffer->Get()};
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

  vkCmdBindIndexBuffer(commandBuffer, m_IndexBuffer->Get(), 0,
                       m_IndexBuffer->GetIndexType());

  VkViewport viewport = {
      .x = 0.0f,
      .y = 0.0f,
      .width = static_cast<float>(m_pContext->GetSwapChainExtent().width),
      .height = static_cast<float>(m_pContext->GetSwapChainExtent().height),
      .minDepth = 0.0f,
      .maxDepth = 1.0f,
  };

  vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

  VkRect2D scissor = {
      .offset = {0, 0},
      .extent = m_pContext->GetSwapChainExtent(),
  };

  vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

  vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          m_PipelineLayout, 0, 1,
                          &m_Frames[m_CurrentFrame].DescriptorSet, 0, nullptr);

  vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indices.size()), 1, 0,
                   0, 0);
  vkCmdEndRenderPass(commandBuffer);
  vkEndCommandBuffer(commandBuffer);
}

void Renderer::RecreateSwapChain() {
  vkDeviceWaitIdle(m_pContext->GetDevice());

  for (auto framebuffer : m_SwapChainFrameBuffers) {
    vkDestroyFramebuffer(m_pContext->GetDevice(), framebuffer, nullptr);
  }

  m_pContext->RecreateSwapChain();
  CreateFramebuffers();
}

void Renderer::UpdateUniformBuffer(uint32_t currentImage) {
  static auto startTime = std::chrono::high_resolution_clock::now();

  auto currentTime = std::chrono::high_resolution_clock::now();
  float time = std::chrono::duration<float, std::chrono::seconds::period>(
                   currentTime - startTime)
                   .count();

  UniformBufferOjbect ubo{};
  ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f),
                          glm::vec3(0.0f, 0.0f, 1.0f));
  ubo.view =
      glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f),
                  glm::vec3(0.0f, 0.0f, 1.0f));
  ubo.proj =
      glm::perspective(glm::radians(45.0f),
                       m_pContext->GetSwapChainExtent().width /
                           (float)m_pContext->GetSwapChainExtent().height,
                       0.1f, 10.0f);

  ubo.proj[1][1] *= -1;

  m_Frames[currentImage].UniBuffer->Update(&ubo, sizeof(ubo));
}
} // namespace Inferno
