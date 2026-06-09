#include "Inferno/ECS/Entity.h"
#include "glm/ext.hpp"
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
  glm::mat4 mvp;
};

void Renderer::StartUp(ResourceManager *resourceManager) {
  m_RenderGraph = new RenderGraph(m_Context);
  m_CullingSystem = new CullingSystem();
  m_ResourceManager = resourceManager;

  // TODO: SET CAMERA
  CreateTestPipeline();
  SetupDeferredPipeline();
}

void Renderer::ShutDown() {
  if (m_Context && m_Context->Device != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(m_Context->Device);
  }

  if (m_Pipeline != VK_NULL_HANDLE) {
    vkDestroyPipeline(m_Context->Device, m_Pipeline, nullptr);
    m_Pipeline = VK_NULL_HANDLE;
  }

  if (m_PipelineLayout != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(m_Context->Device, m_PipelineLayout, nullptr);
    m_PipelineLayout = VK_NULL_HANDLE;
  }

  delete m_RenderGraph;
  m_RenderGraph = nullptr;

  delete m_CullingSystem;
  m_CullingSystem = nullptr;
}

void Renderer::SetupDeferredPipeline() {
  m_RenderGraph->ImportSwapchainResources(
      "SwapchainOutput", m_Context->Swapchain.Images,
      m_Context->Swapchain.ImageViews, m_Context->Swapchain.Format,
      m_Context->Swapchain.Extent, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
      VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

  // Resources Allocations
  m_RenderGraph->AddResource(
      "GBuffer_Position", VK_FORMAT_R16G16B16A16_SFLOAT,
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

  // ==============================================================
  // TEMPORARY FORWARD PASS (Bypassing G-Buffers)
  // ==============================================================
  m_RenderGraph->AddPass(
      "GeometryPass", {}, {"SwapchainOutput", "Depth"},
      [this](VkCommandBuffer &cmd) {
        VkClearValue clearColor{};
        clearColor.color = {{0.2f, 0.3f, 0.3f, 1.0f}};

        VkClearValue clearDepth{};
        clearDepth.depthStencil = {1.0f, 0};

        VkRenderingAttachmentInfo colorAttachment{};
        colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        colorAttachment.imageView =
            m_RenderGraph->GetActiveImageView("SwapchainOutput");
        colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.clearValue = clearColor;

        VkRenderingAttachmentInfo depthAttachment{};
        depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depthAttachment.imageView = m_RenderGraph->GetActiveImageView("Depth");
        depthAttachment.imageLayout =
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depthAttachment.clearValue = clearDepth;

        VkRenderingInfo renderingInfo{};
        renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderingInfo.renderArea = {{0, 0}, m_Context->Swapchain.Extent};
        renderingInfo.layerCount = 1;
        renderingInfo.colorAttachmentCount = 1; // Only 1 attachment now
        renderingInfo.pColorAttachments = &colorAttachment;
        renderingInfo.pDepthAttachment = &depthAttachment;

        vkCmdBeginRendering(cmd, &renderingInfo);

        // Bind your rendering pipeline
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline);
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

        // Setup mock Camera Matrices for testing (Rotate slightly over time to
        // see depth)
        float time = (float)glfwGetTime();
        glm::mat4 proj =
            glm::perspective(glm::radians(45.0f),
                             (float)m_Context->Swapchain.Extent.width /
                                 (float)m_Context->Swapchain.Extent.height,
                             0.1f, 10.0f);
        proj[1][1] *= -1; // Correct for Vulkan's inverted Y axis

        // Loop through and draw each object
        for (auto *entity : visibleItems) {
          // Assuming entity provides access to your Mesh resource class
          MeshComponent *meshComponent = entity->GetComponent<MeshComponent>();
          if (!meshComponent)
            continue;

          glm::mat4 model = entity->GetComponent<TransformComponent>()->GetTransformmatrix();
          model = glm::rotate(model, time * glm::radians(90.0f) * model[0][0] * 10, 
                              glm::vec3(0.0f, 1.0f, 0.0f));
          glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 1.0f, 3.0f),
                                       glm::vec3(0.0f, 0.0f, 0.0f),
                                       glm::vec3(0.0f, 1.0f, 0.0f));

          MeshPushConstants push{};
          push.mvp = proj * view * model;

          // Push the transformation data instantly to the GPU
          vkCmdPushConstants(cmd, m_PipelineLayout, VK_SHADER_STAGE_VERTEX_BIT,
                             0, sizeof(MeshPushConstants), &push);

          // Bind Vertex Buffers
          // Note: Adjust .GetVulkanHandle() to match whatever method returns
          // your raw VkBuffer from your wrapper
          VkBuffer vertexBuffers[] = {
              meshComponent->GetMesh()->GetVertexBuffer()->Get()};
          VkDeviceSize offsets[] = {0};
          vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);

          // Bind Index Buffer
          vkCmdBindIndexBuffer(
              cmd, meshComponent->GetMesh()->GetIndexBuffer()->Get(), 0,
              VK_INDEX_TYPE_UINT16);

          // Issue Draw Call
          vkCmdDrawIndexed(cmd, meshComponent->GetMesh()->GetIndexCount(), 1, 0,
                           0, 0);
        }
        vkCmdEndRendering(cmd);
      });

  /*
  m_RenderGraph->AddPass(
      "GeometryPass", {},
      {"GBuffer_Position", "GBuffer_Normal", "GBuffer_Albedo", "Depth"},
      [this](VkCommandBuffer &cmd) {
        // Clear value configuration (Black for targets, 1.0 for depth)
        VkClearValue clearColor{};
        clearColor.color = {{0.0f, 0.0f, 0.0f, 0.0f}};

        VkClearValue clearDepth{};
        clearDepth.depthStencil = {1.0f, 0};

        // Populate the 3 color attachments for our G-Buffer
        VkRenderingAttachmentInfo colorAttachments[3]{};
        std::string gBufferNames[3] = {"GBuffer_Position", "GBuffer_Normal",
                                       "GBuffer_Albedo"};

        for (int i = 0; i < 3; ++i) {
          colorAttachments[i].sType =
              VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
          colorAttachments[i].imageView =
              m_RenderGraph->GetActiveImageView(gBufferNames[i]);
          colorAttachments[i].imageLayout =
              VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
          colorAttachments[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
          colorAttachments[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
          colorAttachments[i].clearValue = clearColor;
        }

        // Configure the depth attachment
        VkRenderingAttachmentInfo depthAttachment{};
        depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depthAttachment.imageView = m_RenderGraph->GetActiveImageView("Depth");
        depthAttachment.imageLayout =
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depthAttachment.clearValue = clearDepth;

        // Combine into dynamic rendering context block
        VkRenderingInfo renderingInfo{};
        renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderingInfo.renderArea = {{0, 0}, m_SwapchainExtent};
        renderingInfo.layerCount = 1;
        renderingInfo.colorAttachmentCount = 3;
        renderingInfo.pColorAttachments = colorAttachments;
        renderingInfo.pDepthAttachment = &depthAttachment;

        vkCmdBeginRendering(cmd, &renderingInfo);

        // Fetch scene data filtered by your CPU culling module
        const auto &visibleItems = m_CullingSystem->GetVisibleEntities();

        // Per-Object Drawing Loop
        for (const auto *entity : visibleItems) {
          // 1. vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
          // entity->Pipeline);
          // 2. vkCmdBindDescriptorSets(cmd, ... per-object transformation
          // uniform descriptor updates)
          // 3. vkCmdBindVertexBuffers(cmd, ...) & vkCmdBindIndexBuffer(cmd,
          // ...)
          // 4. vkCmdDrawIndexed(cmd, ...)
        }

        vkCmdEndRendering(cmd);
      });

  m_RenderGraph->AddPass(
      "LightingPass",
      {"GBuffer_Position", "GBuffer_Normal", "GBuffer_Albedo", "Depth"},
      {"SwapchainOutput"}, [this](VkCommandBuffer &cmd) {
        // Target the screen swapchain backbuffer directly
        VkRenderingAttachmentInfo swapchainAttachment{};
        swapchainAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        swapchainAttachment.imageView =
            m_RenderGraph->GetActiveImageView("SwapchainOutput");
        swapchainAttachment.imageLayout =
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        swapchainAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        swapchainAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        swapchainAttachment.clearValue.color = {{0.0f, 0.0f, 0.0f, 1.0f}};

        VkRenderingInfo renderingInfo{};
        renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderingInfo.renderArea = {{0, 0}, m_SwapchainExtent};
        renderingInfo.layerCount = 1;
        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachments = &swapchainAttachment;

        vkCmdBeginRendering(cmd, &renderingInfo);

        // Deferred Composition Rendering Sequence:
        // 1. vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
        // m_LightingPipeline);
        // 2. Bind G-Buffer views as textures/samplers via Descriptor Sets to
        // sample from them
        //    vkCmdBindDescriptorSets(cmd, ...);
        // 3. Draw a single full-screen triangle or quad to evaluate lighting
        // formulas per pixel
        //    vkCmdDraw(cmd, 3, 1, 0, 0);

        vkCmdEndRendering(cmd);
      });
*/

  m_RenderGraph->Compile();
}

void Renderer::Render(const std::vector<Entity *> &entities) {
  // TODO: Enable once we have a Camera
  // m_CullingSystem->CullScene(entities);
  m_VisibleEntites = entities;
  bool success = m_RenderGraph->RenderFrame(m_Context->Swapchain.Handle,
                                            m_Context->GraphicsQueue,
                                            m_Context->PresentQueue);

  if (!success) {
    vkDeviceWaitIdle(m_Context->Device);
    m_Context->RecreateSwapchain();

    m_RenderGraph->UpdateSwapchainResources(
        "SwapchainOutput", m_Context->Swapchain.Images,
        m_Context->Swapchain.ImageViews, m_Context->Swapchain.Extent);

    m_RenderGraph->Resize(m_Context->Swapchain.Extent);
  }
}

void Renderer::CreateTestPipeline() {
  auto testShader = m_ResourceManager->Load<Shader>("test", m_Context);
  // 1. Setup Push Constant Range
  VkPushConstantRange pushConstantRange{};
  pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
  pushConstantRange.offset = 0;
  pushConstantRange.size = sizeof(MeshPushConstants);

  VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.pushConstantRangeCount = 1;
  pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

  if (vkCreatePipelineLayout(m_Context->Device, &pipelineLayoutInfo, nullptr,
                             &m_PipelineLayout) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create pipeline layout!");
  }

  // 2. Map your MeshVertex Layout to Vulkan Input Descriptions
  VkVertexInputBindingDescription bindingDescription{};
  bindingDescription.binding = 0;
  bindingDescription.stride = sizeof(MeshVertex);
  bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};
  // Position
  attributeDescriptions[0].binding = 0;
  attributeDescriptions[0].location = 0;
  attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
  attributeDescriptions[0].offset = offsetof(MeshVertex, Position);
  // Color
  attributeDescriptions[1].binding = 0;
  attributeDescriptions[1].location = 1;
  attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
  attributeDescriptions[1].offset = offsetof(MeshVertex, Color);
  // TexCoord
  attributeDescriptions[2].binding = 0;
  attributeDescriptions[2].location = 2;
  attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
  attributeDescriptions[2].offset = offsetof(MeshVertex, TexCoord);

  VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
  vertexInputInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInputInfo.vertexBindingDescriptionCount = 1;
  vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
  vertexInputInfo.vertexAttributeDescriptionCount =
      static_cast<uint32_t>(attributeDescriptions.size());
  vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

  // 3. Configure Basic Pipeline States
  VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
  inputAssembly.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  VkPipelineViewportStateCreateInfo viewportState{};
  viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportState.viewportCount = 1;
  viewportState.pViewports = nullptr;
  viewportState.scissorCount = 1;
  viewportState.pScissors = nullptr;

  VkPipelineRasterizationStateCreateInfo rasterizer{};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizer.lineWidth = 1.0f;
  rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
  rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

  VkPipelineMultisampleStateCreateInfo multisampling{};
  multisampling.sType =
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineDepthStencilStateCreateInfo depthStencil{};
  depthStencil.sType =
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depthStencil.depthTestEnable = VK_TRUE;
  depthStencil.depthWriteEnable = VK_TRUE;
  depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;

  VkPipelineColorBlendAttachmentState colorBlendAttachment{};
  colorBlendAttachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

  VkPipelineColorBlendStateCreateInfo colorBlending{};
  colorBlending.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlending.attachmentCount = 1;
  colorBlending.pAttachments = &colorBlendAttachment;

  // 4. Hook Up Shaders
  VkPipelineShaderStageCreateInfo shaderStages[2]{};
  shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  shaderStages[0].module = testShader->GetVertexShaderModule();
  shaderStages[0].pName = "main";

  shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  shaderStages[1].module = testShader->GetFragmentShaderModule();
  shaderStages[1].pName = "main";

  // 5. CRITICAL STEP FOR DYNAMIC RENDERING: Tell the pipeline your runtime
  // image formats!
  VkPipelineRenderingCreateInfo renderingCreateInfo{};
  renderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
  renderingCreateInfo.colorAttachmentCount = 1;
  renderingCreateInfo.pColorAttachmentFormats =
      &m_Context->Swapchain.Format; // VK_FORMAT_B8G8R8A8_UNORM
  renderingCreateInfo.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT;

  std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
                                               VK_DYNAMIC_STATE_SCISSOR};

  VkPipelineDynamicStateCreateInfo dynamicState{};
  dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
  dynamicState.pDynamicStates = dynamicStates.data();

  VkGraphicsPipelineCreateInfo pipelineInfo{};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineInfo.pNext =
      &renderingCreateInfo;
  pipelineInfo.stageCount = 2;
  pipelineInfo.pStages = shaderStages;
  pipelineInfo.pVertexInputState = &vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &inputAssembly;
  pipelineInfo.pViewportState = &viewportState;
  pipelineInfo.pRasterizationState = &rasterizer;
  pipelineInfo.pMultisampleState = &multisampling;
  pipelineInfo.pDepthStencilState = &depthStencil;
  pipelineInfo.pColorBlendState = &colorBlending;
  pipelineInfo.layout = m_PipelineLayout;
  pipelineInfo.pDynamicState = &dynamicState;

  if (vkCreateGraphicsPipelines(m_Context->Device, VK_NULL_HANDLE, 1,
                                &pipelineInfo, nullptr,
                                &m_Pipeline) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create graphics pipeline!");
  }
}
} // namespace Inferno
