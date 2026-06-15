#include "Inferno/ECS/Entity.h"
#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/vector_float3.hpp"
#include <vector>
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <GLFW/glfw3.h>
#include <pch.h>
#include <stdexcept>
#include <vulkan/vulkan_core.h>

#include "Inferno/ECS/Component.h"
#include "Inferno/Renderer/CullingSystem.h"
#include "Inferno/Renderer/Mesh.h"
#include "Inferno/Renderer/RenderGraph.h"
#include "Inferno/Renderer/Shader.h"
#include "Inferno/Resource/ResourceManager.h"
#include "Renderer.h"

#include <glm/glm.hpp>

namespace Inferno {
struct MeshPushConstants {
  glm::mat4 Mvp;
  glm::mat4 Model;
};

struct LightingDebugPushConstants {
  int ViewMode; // 0 = Fully Lit, 1 = Albedo, 2 = Normals, 3 = Position, 4 =
                // Depth
};

void Renderer::StartUp(ResourceManager *resourceManager) {
  m_RenderGraph = new RenderGraph(m_Context);
  m_CullingSystem = new CullingSystem();
  m_ResourceManager = resourceManager;

  // TODO: SET CAMERA
  CreateLightingDescriptorSet();
  CreateGeometryPipeline();
  CreateLightingPipeline();
  SetupDeferredRendering();

  for (uint32_t i = 0; i < m_RenderGraph->GetGetCurrentFrameIndex(); ++i) {
    UpdateLightingDescriptorSet(i);
  }
}

void Renderer::ShutDown() {
  if (m_Context && m_Context->Device != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(m_Context->Device);
  }

  if (m_GeometryDescriptorSetLayout != VK_NULL_HANDLE) {
    vkDestroyDescriptorSetLayout(m_Context->Device,
                                 m_GeometryDescriptorSetLayout, nullptr);
    m_GeometryDescriptorSetLayout = VK_NULL_HANDLE;
  }

  if (m_GeometryPipeline != VK_NULL_HANDLE) {
    vkDestroyPipeline(m_Context->Device, m_GeometryPipeline, nullptr);
    m_GeometryPipeline = VK_NULL_HANDLE;
  }
  if (m_GeometryPipelineLayout != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(m_Context->Device, m_GeometryPipelineLayout,
                            nullptr);
    m_GeometryPipelineLayout = VK_NULL_HANDLE;
  }

  if (m_LightingPipeline != VK_NULL_HANDLE) {
    vkDestroyPipeline(m_Context->Device, m_LightingPipeline, nullptr);
    m_LightingPipeline = VK_NULL_HANDLE;
  }
  if (m_LightingPipelineLayout != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(m_Context->Device, m_LightingPipelineLayout,
                            nullptr);
    m_LightingPipelineLayout = VK_NULL_HANDLE;
  }

  if (m_TextureDescriptorPool != VK_NULL_HANDLE) {
    vkDestroyDescriptorPool(m_Context->Device, m_TextureDescriptorPool,
                            nullptr);
    m_TextureDescriptorPool = VK_NULL_HANDLE;
  }
  m_TextureDescroptiorSets.clear();

  if (m_LightingDescriptorLayout != VK_NULL_HANDLE) {
    vkDestroyDescriptorSetLayout(m_Context->Device, m_LightingDescriptorLayout,
                                 nullptr);
  }
  if (m_DescriptorPool != VK_NULL_HANDLE) {
    vkDestroyDescriptorPool(m_Context->Device, m_DescriptorPool, nullptr);
  }
  if (m_GBufferSampler != VK_NULL_HANDLE) {
    vkDestroySampler(m_Context->Device, m_GBufferSampler, nullptr);
  }

  delete m_RenderGraph;
  m_RenderGraph = nullptr;

  delete m_CullingSystem;
  m_CullingSystem = nullptr;
}

void Renderer::CreateGeometryPipeline() {
  auto gbufferShader = m_ResourceManager->Load<Shader>("gbuffer", m_Context);

  VkDescriptorSetLayoutBinding samplerLayoutBinding{};
  samplerLayoutBinding.binding = 0;
  samplerLayoutBinding.descriptorType =
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  samplerLayoutBinding.descriptorCount = 1;
  samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  samplerLayoutBinding.pImmutableSamplers = nullptr;

  VkDescriptorSetLayoutCreateInfo layoutInfo{};
  layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layoutInfo.bindingCount = 1;
  layoutInfo.pBindings = &samplerLayoutBinding;

  if (vkCreateDescriptorSetLayout(m_Context->Device, &layoutInfo, nullptr,
                                  &m_GeometryDescriptorSetLayout)) {
    throw std::runtime_error("Failed to create Geometry descriptor set layout");
  }

  VkPushConstantRange pushConstantRange{};
  pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
  pushConstantRange.offset = 0;
  pushConstantRange.size = sizeof(MeshPushConstants);

  VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount = 1;
  pipelineLayoutInfo.pSetLayouts = &m_GeometryDescriptorSetLayout;
  pipelineLayoutInfo.pushConstantRangeCount = 1;
  pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

  if (vkCreatePipelineLayout(m_Context->Device, &pipelineLayoutInfo, nullptr,
                             &m_GeometryPipelineLayout) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create geometry pipeline layout!");
  }

  auto meshVertexLayout = MeshVertex::GetLayout();

  VkVertexInputBindingDescription bindingDescription =
      meshVertexLayout.GetBindingDescription();
  std::vector<VkVertexInputAttributeDescription> attributeDescriptions =
      meshVertexLayout.GetAttributeDescriptions();

  VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
  vertexInputInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInputInfo.vertexBindingDescriptionCount = 1;
  vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
  vertexInputInfo.vertexAttributeDescriptionCount =
      static_cast<uint32_t>(attributeDescriptions.size());
  vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

  VkPipelineInputAssemblyStateCreateInfo inputAssembly{
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, nullptr, 0,
      VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE};
  VkPipelineViewportStateCreateInfo viewportState{
      VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      nullptr,
      0,
      1,
      nullptr,
      1,
      nullptr};

  VkPipelineRasterizationStateCreateInfo rasterizer{};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizer.lineWidth = 1.0f;
  rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
  rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

  VkPipelineMultisampleStateCreateInfo multisampling{
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, nullptr, 0,
      VK_SAMPLE_COUNT_1_BIT};
  VkPipelineDepthStencilStateCreateInfo depthStencil{
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
      nullptr,
      0,
      VK_TRUE,
      VK_TRUE,
      VK_COMPARE_OP_LESS};

  std::array<VkPipelineColorBlendAttachmentState, 3> blendAttachments{};
  for (auto &attachment : blendAttachments) {
    attachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    attachment.blendEnable = VK_FALSE;
  }

  VkPipelineColorBlendStateCreateInfo colorBlending{};
  colorBlending.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlending.attachmentCount =
      static_cast<uint32_t>(blendAttachments.size());
  colorBlending.pAttachments = blendAttachments.data();

  VkPipelineShaderStageCreateInfo shaderStages[2]{};
  shaderStages[0] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                     nullptr,
                     0,
                     VK_SHADER_STAGE_VERTEX_BIT,
                     gbufferShader->GetVertexShaderModule(),
                     "main",
                     nullptr};
  shaderStages[1] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                     nullptr,
                     0,
                     VK_SHADER_STAGE_FRAGMENT_BIT,
                     gbufferShader->GetFragmentShaderModule(),
                     "main",
                     nullptr};

  // Formats mapping layout configuration
  std::array<VkFormat, 3> gbufferFormats = {VK_FORMAT_R32G32B32A32_SFLOAT,
                                            VK_FORMAT_R16G16B16A16_SFLOAT,
                                            VK_FORMAT_R16G16B16A16_SFLOAT};
  VkPipelineRenderingCreateInfo renderingCreateInfo{};
  renderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
  renderingCreateInfo.colorAttachmentCount =
      static_cast<uint32_t>(gbufferFormats.size());
  renderingCreateInfo.pColorAttachmentFormats = gbufferFormats.data();
  renderingCreateInfo.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT;

  std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
                                               VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamicState{
      VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, nullptr, 0,
      static_cast<uint32_t>(dynamicStates.size()), dynamicStates.data()};

  VkGraphicsPipelineCreateInfo pipelineInfo{
      VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, &renderingCreateInfo};
  pipelineInfo.stageCount = 2;
  pipelineInfo.pStages = shaderStages;
  pipelineInfo.pVertexInputState = &vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &inputAssembly;
  pipelineInfo.pViewportState = &viewportState;
  pipelineInfo.pRasterizationState = &rasterizer;
  pipelineInfo.pMultisampleState = &multisampling;
  pipelineInfo.pDepthStencilState = &depthStencil;
  pipelineInfo.pColorBlendState = &colorBlending;
  pipelineInfo.layout = m_GeometryPipelineLayout;
  pipelineInfo.pDynamicState = &dynamicState;

  if (vkCreateGraphicsPipelines(m_Context->Device, VK_NULL_HANDLE, 1,
                                &pipelineInfo, nullptr,
                                &m_GeometryPipeline) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create geometry graphics pipeline!");
  }
}

VkDescriptorSet
Renderer::GetOrCreateTextureDescriptorSet(const Texture *texture) {
  auto it = m_TextureDescroptiorSets.find(texture->GetID());
  if (it != m_TextureDescroptiorSets.end()) {
    return it->second;
  }

  if (m_TextureDescriptorPool == VK_NULL_HANDLE) {
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 100;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 100;

    if (vkCreateDescriptorPool(m_Context->Device, &poolInfo, nullptr,
                               &m_TextureDescriptorPool) != VK_SUCCESS) {
      throw std::runtime_error("Failed to create texture descriptor pool");
    }
  }

  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = m_TextureDescriptorPool;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts = &m_GeometryDescriptorSetLayout;

  VkDescriptorSet descriptorSet;
  if (vkAllocateDescriptorSets(m_Context->Device, &allocInfo, &descriptorSet) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed to allocate texture descriptor set");
  }

  VkDescriptorImageInfo imageInfo{};
  imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  imageInfo.imageView = texture->GetImageView();
  imageInfo.sampler = texture->GetSampler();

  VkWriteDescriptorSet descriptorWrite{};
  descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptorWrite.dstSet = descriptorSet;
  descriptorWrite.dstBinding = 0;
  descriptorWrite.dstArrayElement = 0;
  descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  descriptorWrite.descriptorCount = 1;
  descriptorWrite.pImageInfo = &imageInfo;

  vkUpdateDescriptorSets(m_Context->Device, 1, &descriptorWrite, 0, nullptr);

  m_TextureDescroptiorSets[texture->GetID()] = descriptorSet;
  return descriptorSet;
}

void Renderer::CreateLightingPipeline() {
  auto lightingShader = m_ResourceManager->Load<Shader>("lighting", m_Context);

  VkPushConstantRange lightingDebugPushConstants{};
  lightingDebugPushConstants.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  lightingDebugPushConstants.offset = 0;
  lightingDebugPushConstants.size = sizeof(LightingDebugPushConstants);

  VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount = 1;
  pipelineLayoutInfo.pSetLayouts = &m_LightingDescriptorLayout;
  pipelineLayoutInfo.pushConstantRangeCount = 1;
  pipelineLayoutInfo.pPushConstantRanges = &lightingDebugPushConstants;

  if (vkCreatePipelineLayout(m_Context->Device, &pipelineLayoutInfo, nullptr,
                             &m_LightingPipelineLayout) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create lighting pipeline layout!");
  }

  // Empty vertex input—procedural full screen generation inside vertex index
  // lookups
  VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
  vertexInputInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

  VkPipelineInputAssemblyStateCreateInfo inputAssembly{
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, nullptr, 0,
      VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE};
  VkPipelineViewportStateCreateInfo viewportState{
      VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      nullptr,
      0,
      1,
      nullptr,
      1,
      nullptr};

  VkPipelineRasterizationStateCreateInfo rasterizer{};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizer.lineWidth = 1.0f;
  rasterizer.cullMode = VK_CULL_MODE_NONE;

  VkPipelineMultisampleStateCreateInfo multisampling{
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, nullptr, 0,
      VK_SAMPLE_COUNT_1_BIT};

  VkPipelineDepthStencilStateCreateInfo depthStencil{
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
      nullptr,
      0,
      VK_FALSE,
      VK_FALSE,
      VK_COMPARE_OP_ALWAYS};

  VkPipelineColorBlendAttachmentState colorBlendAttachment{};
  colorBlendAttachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

  VkPipelineColorBlendStateCreateInfo colorBlending{};
  colorBlending.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlending.attachmentCount = 1;
  colorBlending.pAttachments = &colorBlendAttachment;

  VkPipelineShaderStageCreateInfo shaderStages[2]{};
  shaderStages[0] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                     nullptr,
                     0,
                     VK_SHADER_STAGE_VERTEX_BIT,
                     lightingShader->GetVertexShaderModule(),
                     "main",
                     nullptr};
  shaderStages[1] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                     nullptr,
                     0,
                     VK_SHADER_STAGE_FRAGMENT_BIT,
                     lightingShader->GetFragmentShaderModule(),
                     "main",
                     nullptr};

  VkPipelineRenderingCreateInfo renderingCreateInfo{};
  renderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
  renderingCreateInfo.colorAttachmentCount = 1;
  renderingCreateInfo.pColorAttachmentFormats = &m_Context->Swapchain.Format;

  std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
                                               VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamicState{
      VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, nullptr, 0,
      static_cast<uint32_t>(dynamicStates.size()), dynamicStates.data()};

  VkGraphicsPipelineCreateInfo pipelineInfo{
      VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, &renderingCreateInfo};
  pipelineInfo.stageCount = 2;
  pipelineInfo.pStages = shaderStages;
  pipelineInfo.pVertexInputState = &vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &inputAssembly;
  pipelineInfo.pViewportState = &viewportState;
  pipelineInfo.pRasterizationState = &rasterizer;
  pipelineInfo.pMultisampleState = &multisampling;
  pipelineInfo.pDepthStencilState = &depthStencil;
  pipelineInfo.pColorBlendState = &colorBlending;
  pipelineInfo.layout = m_LightingPipelineLayout;
  pipelineInfo.pDynamicState = &dynamicState;

  if (vkCreateGraphicsPipelines(m_Context->Device, VK_NULL_HANDLE, 1,
                                &pipelineInfo, nullptr,
                                &m_LightingPipeline) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create lighting graphics pipeline!");
  }
}

void Renderer::CreateLightingDescriptorSet() {
  VkSamplerCreateInfo samplerInfo{};
  samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerInfo.magFilter = VK_FILTER_NEAREST;
  samplerInfo.minFilter = VK_FILTER_NEAREST;
  samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  if (vkCreateSampler(m_Context->Device, &samplerInfo, nullptr,
                      &m_GBufferSampler) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create G-Buffer sampler!");
  }

  std::array<VkDescriptorSetLayoutBinding, 4> bindings{};
  for (uint32_t i = 0; i < 4; ++i) {
    bindings[i].binding = i;
    bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[i].descriptorCount = 1;
    bindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  }

  VkDescriptorSetLayoutCreateInfo layoutInfo{};
  layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
  layoutInfo.pBindings = bindings.data();

  if (vkCreateDescriptorSetLayout(m_Context->Device, &layoutInfo, nullptr,
                                  &m_LightingDescriptorLayout) != VK_SUCCESS) {
    throw std::runtime_error(
        "Failed to create lighting descriptor set layout!");
  }

  VkDescriptorPoolSize poolSize{};
  poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  poolSize.descriptorCount = 8;

  VkDescriptorPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.poolSizeCount = 1;
  poolInfo.pPoolSizes = &poolSize;
  poolInfo.maxSets = 2;

  if (vkCreateDescriptorPool(m_Context->Device, &poolInfo, nullptr,
                             &m_DescriptorPool) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create descriptor pool!");
  }

  m_LightingDescriptorSets.resize(2);
  std::vector<VkDescriptorSetLayout> layouts(2, m_LightingDescriptorLayout);

  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = m_DescriptorPool;
  allocInfo.descriptorSetCount = 2;
  allocInfo.pSetLayouts = layouts.data();

  if (vkAllocateDescriptorSets(m_Context->Device, &allocInfo,
                               m_LightingDescriptorSets.data()) != VK_SUCCESS) {
    throw std::runtime_error("Failed to allocate lighting descriptor set!");
  }
}

void Renderer::UpdateLightingDescriptorSet(uint32_t frameIdx) {
  std::array<std::string, 4> targetNames = {
      "GBuffer_Position", "GBuffer_Normal", "GBuffer_Albedo", "Depth"};
  std::array<VkDescriptorImageInfo, 4> imageInfos{};
  std::array<VkWriteDescriptorSet, 4> descriptorWrites{};

  for (uint32_t i = 0; i < 4; ++i) {
    imageInfos[i].sampler = m_GBufferSampler;
    imageInfos[i].imageView =
        m_RenderGraph->GetImageView(targetNames[i], frameIdx);
    imageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    descriptorWrites[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[i].dstSet = m_LightingDescriptorSets[frameIdx];
    descriptorWrites[i].dstBinding = i;
    descriptorWrites[i].dstArrayElement = 0;
    descriptorWrites[i].descriptorType =
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrites[i].descriptorCount = 1;
    descriptorWrites[i].pImageInfo = &imageInfos[i];
  }

  vkUpdateDescriptorSets(m_Context->Device,
                         static_cast<uint32_t>(descriptorWrites.size()),
                         descriptorWrites.data(), 0, nullptr);
}

void Renderer::SetupDeferredRendering() {
  m_RenderGraph->ImportSwapchainResources(
      "SwapchainOutput", m_Context->Swapchain.Images,
      m_Context->Swapchain.ImageViews, m_Context->Swapchain.Format,
      m_Context->Swapchain.Extent, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
      VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

  // Resources Allocations
  m_RenderGraph->AddResource(
      "GBuffer_Position", VK_FORMAT_R32G32B32A32_SFLOAT,
      m_Context->Swapchain.Extent,
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  m_RenderGraph->AddResource(
      "GBuffer_Normal", VK_FORMAT_R16G16B16A16_SFLOAT,
      m_Context->Swapchain.Extent,
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  m_RenderGraph->AddResource(
      "GBuffer_Albedo", VK_FORMAT_R16G16B16A16_SFLOAT,
      m_Context->Swapchain.Extent,
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  m_RenderGraph->AddResource(
      "Depth", VK_FORMAT_D32_SFLOAT, m_Context->Swapchain.Extent,
      VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      VK_IMAGE_LAYOUT_UNDEFINED,
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

  m_RenderGraph->AddPass(
      "GeometryPass", {},
      {"GBuffer_Position", "GBuffer_Normal", "GBuffer_Albedo", "Depth"},
      [this](VkCommandBuffer &cmd) {
        VkClearValue clearColor{};
        clearColor.color = {{0.2f, 0.3f, 0.3f, 1.0f}};

        VkClearValue clearDepth{};
        clearDepth.depthStencil = {1.0f, 0};

        std::array<VkRenderingAttachmentInfo, 3> colorAttachments{};
        std::array<std::string, 3> gBufferNames = {
            "GBuffer_Position", "GBuffer_Normal", "GBuffer_Albedo"};

        uint32_t frameIdx = m_RenderGraph->GetGetCurrentFrameIndex();
        for (int i = 0; i < 3; ++i) {
          colorAttachments[i].sType =
              VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
          colorAttachments[i].imageView =
              m_RenderGraph->GetImageView(gBufferNames[i], frameIdx);
          colorAttachments[i].imageLayout =
              VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
          colorAttachments[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
          colorAttachments[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
          colorAttachments[i].clearValue = clearColor;
        }

        VkRenderingAttachmentInfo depthAttachment{};
        depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depthAttachment.imageView =
            m_RenderGraph->GetImageView("Depth", frameIdx);
        depthAttachment.imageLayout =
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depthAttachment.clearValue = clearDepth;

        VkRenderingInfo renderingInfo{};
        renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderingInfo.renderArea = {{0, 0}, m_Context->Swapchain.Extent};
        renderingInfo.layerCount = 1;
        renderingInfo.colorAttachmentCount =
            static_cast<uint32_t>(colorAttachments.size());
        renderingInfo.pColorAttachments = colorAttachments.data();
        renderingInfo.pDepthAttachment = &depthAttachment;

        vkCmdBeginRendering(cmd, &renderingInfo);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          m_GeometryPipeline);
        VkViewport viewport{0.0f,
                            0.0f,
                            (float)m_Context->Swapchain.Extent.width,
                            (float)m_Context->Swapchain.Extent.height,
                            0.0f,
                            1.0f};
        VkRect2D scissor{{0, 0}, m_Context->Swapchain.Extent};
        vkCmdSetViewport(cmd, 0, 1, &viewport);
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        // const auto &visibleItems = m_CullingSystem->GetVisibleEntities();
        const auto &visibleItems = m_VisibleEntites;

        // Mock Camera
        glm::mat4 proj =
            glm::perspective(glm::radians(45.0f),
                             (float)m_Context->Swapchain.Extent.width /
                                 (float)m_Context->Swapchain.Extent.height,
                             0.1f, 10.0f);
        proj[1][1] *= -1;
        glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 1.0f, 3.0f),
                                     glm::vec3(0.0f, 0.0f, 0.0f),
                                     glm::vec3(0.0f, 1.0f, 0.0f));

        for (auto *entity : visibleItems) {
          MeshComponent *meshComponent = entity->GetComponent<MeshComponent>();
          if (!meshComponent)
            continue;

          glm::mat4 model =
              entity->GetComponent<TransformComponent>()->GetTransformmatrix();

          MeshPushConstants push{};
          push.Mvp = proj * view * model;
          push.Model = model;

          vkCmdPushConstants(cmd, m_GeometryPipelineLayout,
                             VK_SHADER_STAGE_VERTEX_BIT, 0,
                             sizeof(MeshPushConstants), &push);

          VkBuffer vertexBuffers[] = {
              meshComponent->GetMesh()->GetVertexBuffer()->Get()};
          VkDeviceSize offsets[] = {0};
          vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
          vkCmdBindIndexBuffer(
              cmd, meshComponent->GetMesh()->GetIndexBuffer()->Get(), 0,
              meshComponent->GetMesh()->GetIndexBuffer()->GetIndexType());

          const auto *texture = meshComponent->GetTexture();
          if (texture) {
            VkDescriptorSet textureSet =
                GetOrCreateTextureDescriptorSet(texture);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    m_GeometryPipelineLayout, 0, 1, &textureSet,
                                    0, nullptr);
          }

          vkCmdDrawIndexed(cmd, meshComponent->GetMesh()->GetIndexCount(), 1, 0,
                           0, 0);
        }
        vkCmdEndRendering(cmd);
      });

  m_RenderGraph->AddPass(
      "LightingPass",
      {"GBuffer_Position", "GBuffer_Normal", "GBuffer_Albedo", "Depth"},
      {"SwapchainOutput"}, [this](VkCommandBuffer &cmd) {
        // Target the screen swapchain backbuffer directly
        uint32_t frameIdx = m_RenderGraph->GetGetCurrentFrameIndex();

        VkRenderingAttachmentInfo swapchainAttachment{};
        swapchainAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        swapchainAttachment.imageView =
            m_RenderGraph->GetImageView("SwapchainOutput", frameIdx);
        swapchainAttachment.imageLayout =
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        swapchainAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        swapchainAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        swapchainAttachment.clearValue.color = {{0.1f, 0.1f, 0.1f, 1.0f}};

        VkRenderingInfo renderingInfo{};
        renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderingInfo.renderArea = {{0, 0}, m_Context->Swapchain.Extent};
        renderingInfo.layerCount = 1;
        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachments = &swapchainAttachment;

        vkCmdBeginRendering(cmd, &renderingInfo);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          m_LightingPipeline);

        VkViewport viewport{0.0f,
                            0.0f,
                            (float)m_Context->Swapchain.Extent.width,
                            (float)m_Context->Swapchain.Extent.height,
                            0.0f,
                            1.0f};
        VkRect2D scissor{{0, 0}, m_Context->Swapchain.Extent};
        vkCmdSetViewport(cmd, 0, 1, &viewport);
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        vkCmdBindDescriptorSets(
            cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_LightingPipelineLayout, 0,
            1, &m_LightingDescriptorSets[frameIdx], 0, nullptr);

        LightingDebugPushConstants lightinPushConstants{};
        lightinPushConstants.ViewMode = m_LightinDebugMode;

        vkCmdPushConstants(
            cmd, m_LightingPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0,
            sizeof(LightingDebugPushConstants), &lightinPushConstants);

        vkCmdDraw(cmd, 3, 1, 0, 0);

        vkCmdEndRendering(cmd);
      });

  m_RenderGraph->Compile();
}

void Renderer::Render(const std::vector<Entity *> &entities) {
  // TODO: Enable once we have a Camera
  // m_CullingSystem->CullScene(entities);
  m_VisibleEntites = entities;
  bool success = m_RenderGraph->RenderFrame(m_Context->Swapchain.Handle,
                                            m_Context->GraphicsQueue,
                                            m_Context->PresentQueue);

  if (!success || m_Resized) {
    Resize();
  }
}

void Renderer::Resize() {
  vkDeviceWaitIdle(m_Context->Device);
  m_Context->RecreateSwapchain();

  m_RenderGraph->UpdateSwapchainResources(
      "SwapchainOutput", m_Context->Swapchain.Images,
      m_Context->Swapchain.ImageViews, m_Context->Swapchain.Extent);

  m_RenderGraph->Resize(m_Context->Swapchain.Extent);
  for (uint32_t i = 0; i < m_RenderGraph->GetGetCurrentFrameIndex(); ++i) {
    UpdateLightingDescriptorSet(i);
  }
  m_Resized = false;
}

void Renderer::CreateTestPipeline() {}
} // namespace Inferno
