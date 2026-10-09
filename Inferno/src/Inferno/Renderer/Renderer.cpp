#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "Inferno/Core/Engine.h"
#include "Inferno/Core/Log.h"
#include "Inferno/Core/Memory.h"
#include "Inferno/ECS/Entity.h"
#include "Inferno/Renderer/Image.h"
#include "Inferno/Renderer/Mesh.h"
#include "Inferno/Renderer/Pipeline.h"
#include "Inferno/Renderer/Vertices.h"
#include "Inferno/Renderer/VulkanUtils.h"
#include "glm/ext/matrix_float4x4.hpp"
#include "glm/matrix.hpp"
#include "tracy/Tracy.hpp"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <pch.h>
#include <stdexcept>
#include <vector>
#include <volk/volk.h>
#include <vulkan/vulkan_core.h>

#include "Inferno/ECS/Component.h"
#include "Inferno/Resource/ResourceManager.h"
#include "PushConstants.h"
#include "Renderer.h"

#include <glm/glm.hpp>

namespace Inferno {
namespace {
VkShaderModule LoadShaderModule(const DeviceContext *context,
                                const std::filesystem::path &relativePath) {
  const std::filesystem::path executableDirectory =
      std::filesystem::canonical("/proc/self/exe").parent_path();
  const std::filesystem::path shaderPath = executableDirectory / relativePath;

  std::ifstream file(shaderPath, std::ios::ate | std::ios::binary);
  if (!file.is_open())
    throw std::runtime_error("Failed to Open File: " + shaderPath.string());

  const size_t fileSize = static_cast<size_t>(file.tellg());
  if (fileSize == 0 || fileSize % sizeof(uint32_t) != 0)
    throw std::runtime_error("Invalid SPIR-V File: " + shaderPath.string());

  std::vector<uint32_t> code(fileSize / sizeof(uint32_t));
  file.seekg(0);
  file.read(reinterpret_cast<char *>(code.data()),
            static_cast<std::streamsize>(fileSize));

  VkShaderModuleCreateInfo createInfo{
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize = fileSize,
      .pCode = code.data(),
  };
  VkShaderModule module = VK_NULL_HANDLE;
  if (vkCreateShaderModule(context->Device, &createInfo, nullptr, &module) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed to Create Compute Shader Module");
  }
  return module;
}
} // namespace

void Renderer::StartUp(ResourceManager *resourceManager, GUISystem *guiSystem) {
  m_ResourceManager = resourceManager;
  m_GUISystem = guiSystem;

  CreateGBufferImages();
  CreateHDRSceneColorImages();
  CreateTextureDescriptorResources();
  CreateForwardPipeline();
  CreateGBufferPipeline();
  CreateLightBuffers();
  CreateTiledLightCullingBuffers();
  CreateGBufferDescriptorResources();
  CreateGBufferDebugPipeline();
  CreateDeferredLightingPipeline();
  CreateTiledLightCullingPipeline();
  CreateToneMappingDescriptorResources();
  CreateToneMappingPipeline();
  CreateGridPipeline();
  CreateOutlineDescriptorResources();
  CreateOutlinePipeline();
  CreateDebugLinePipeline();
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

    vkDestroySampler(m_Context->Device, m_GBufferSampler, nullptr);
    vkDestroyDescriptorSetLayout(m_Context->Device,
                                 m_GBufferDescriptorSetLayout, nullptr);
    vkDestroyDescriptorPool(m_Context->Device,
                            m_GBufferDescriptorPool, nullptr);

    vkDestroySampler(m_Context->Device, m_ToneMappingSampler, nullptr);
    vkDestroyDescriptorSetLayout(m_Context->Device,
                                 m_ToneMappingDescriptorSetLayout, nullptr);
    vkDestroyDescriptorPool(m_Context->Device,
                            m_ToneMappingDescriptorPool, nullptr);

    vkDestroySampler(m_Context->Device, m_OutlineSampler, nullptr);

    vkDestroyDescriptorSetLayout(m_Context->Device,
                                 m_OutlineDescriptorSetLayout, nullptr);
    vkDestroyDescriptorPool(m_Context->Device, m_OutlineDescriptorPool,
                            nullptr);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
      m_Frames[i].GeometryBuffer = GBuffer{};
      m_Frames[i].SceneColor = HDRSceneColor{};
      m_Frames[i].PointLightBuffer.reset();
      m_Frames[i].SpotLightBuffer.reset();
      m_Frames[i].TilePointLightCounts.reset();
      m_Frames[i].TilePointLightIndices.reset();
      m_Frames[i].TileSpotLightCounts.reset();
      m_Frames[i].TileSpotLightIndices.reset();
    }

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
      m_DebugLineVertexBuffers[i].reset();
    }

    m_DebugLinePipeline.Destroy(m_Context->Device);
    m_OutlinePipeline.Destroy(m_Context->Device);
    m_GridPipeline.Destroy(m_Context->Device);
    m_ToneMappingPipeline.Destroy(m_Context->Device);
    m_TiledLightCullingPipeline.Destroy(m_Context->Device);
    m_DeferredLightingPipeline.Destroy(m_Context->Device);
    m_GBufferDebugPipeline.Destroy(m_Context->Device);
    m_GBufferPipeline.Destroy(m_Context->Device);
    m_ForwardPipeline.Destroy(m_Context->Device);
  }
}

void Renderer::Render(const RenderWorld &renderWorld, const RenderView &view) {
  ZoneScopedN("Renderer Render");

  if (m_Resized) {
    Resize();
  }

  m_ActiveCamera = view.Camera;

  bool success = true;

  if (vkWaitForFences(m_Context->Device, 1, &m_Frames[m_FrameIndex].DrawFence,
                      VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
    throw std::runtime_error("Failed to Wait on Draw Fence");
  }

  UpdateLightBuffers(renderWorld);

  if (HasRenderFeature(view.Features, RenderFeature::DEBUG_LINES) &&
      !view.DebugLines.empty()) {
    UpdateDebugLineBuffer(view.DebugLines);
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

  // Draw Frame
  {
    FrameData &frame = BeginFrame();

    if (HasRenderFeature(view.Features, RenderFeature::SCENE)) {
      RecordGBufferPass(frame, renderWorld);

      if (view.GBufferView == GBufferDebugView::LIT) {
        RecordTiledLightCullingPass(frame, renderWorld, view);
        RecordDeferredLightingPass(frame, renderWorld, view);
        RecordToneMappingPass(frame, view);
      } else {
        RecordGBufferDebugPass(frame, view);
      }

      if (HasRenderFeature(view.Features, RenderFeature::GRID))
        RecordGridPass(frame, view);
    }

    if (HasRenderFeature(view.Features, RenderFeature::DEBUG_LINES) &&
        !view.DebugLines.empty())
      RecordDebugLinePass(frame, view);

    if (HasRenderFeature(view.Features, RenderFeature::SELECTION_OUTLINE))
      RecordOutlinePass(frame, view);

    if (HasRenderFeature(view.Features, RenderFeature::IMGUI)) {
      m_GUISystem->RenderGUI(frame.CommandBuffer,
                             m_Context->Swapchain.ImageViews[m_ImageIndex],
                             m_Context->Swapchain.Extent);
    }

    EndFrame(frame);
  }

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

void Renderer::CreateGBufferImages() {
  ImageSpec albedoMetallicSpec{
      .Width = m_Context->Swapchain.Extent.width,
      .Height = m_Context->Swapchain.Extent.height,
      .MipLevels = 1,
      .Format = GBuffer::AlbedoMetallicFormat,
      .Usage =
          VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      .Aspect = VK_IMAGE_ASPECT_COLOR_BIT,
  };

  ImageSpec normalRoughnessSpec{
      .Width = m_Context->Swapchain.Extent.width,
      .Height = m_Context->Swapchain.Extent.height,
      .MipLevels = 1,
      .Format = GBuffer::NormalRoughnessFormat,
      .Usage =
          VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      .Aspect = VK_IMAGE_ASPECT_COLOR_BIT,
  };

  ImageSpec depthImageSpec{
      .Width = m_Context->Swapchain.Extent.width,
      .Height = m_Context->Swapchain.Extent.height,
      .MipLevels = 1,
      .Format = GBuffer::DepthFormat,
      .Usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
               VK_IMAGE_USAGE_SAMPLED_BIT,
      .Aspect = VK_IMAGE_ASPECT_DEPTH_BIT,
  };

  ImageSpec pickingImageSpec{
      .Width = m_Context->Swapchain.Extent.width,
      .Height = m_Context->Swapchain.Extent.height,
      .MipLevels = 1,
      .Format = GBuffer::EntityIDFormat,
      .Usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
               VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      .Aspect = VK_IMAGE_ASPECT_COLOR_BIT,
  };

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    GBuffer &gbuffer = m_Frames[i].GeometryBuffer;
    gbuffer.AlbedoMetallic = Image(m_Context, albedoMetallicSpec);
    gbuffer.NormalRoughness = Image(m_Context, normalRoughnessSpec);
    gbuffer.Depth = Image(m_Context, depthImageSpec);
    gbuffer.EntityID = Image(m_Context, pickingImageSpec);
  }
}

void Renderer::CreateHDRSceneColorImages() {
  ImageSpec sceneColorSpec{
      .Width = m_Context->Swapchain.Extent.width,
      .Height = m_Context->Swapchain.Extent.height,
      .MipLevels = 1,
      .Format = HDRSceneColor::Format,
      .Usage =
          VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      .Aspect = VK_IMAGE_ASPECT_COLOR_BIT,
  };

  for (FrameData &frame : m_Frames)
    frame.SceneColor.Color = Image(m_Context, sceneColorSpec);
}

void Renderer::CreateLightBuffers() {
  constexpr VkDeviceSize pointLightBufferSize =
      MAX_POINT_LIGHTS * sizeof(PointLightData);
  constexpr VkDeviceSize spotLightBufferSize =
      MAX_SPOT_LIGHTS * sizeof(SpotLightData);

  for (FrameData &frame : m_Frames) {
    frame.PointLightBuffer =
        MakeScope<StorageBuffer>(m_Context, pointLightBufferSize);
    frame.SpotLightBuffer =
        MakeScope<StorageBuffer>(m_Context, spotLightBufferSize);
  }
}

void Renderer::CreateTiledLightCullingBuffers() {
  m_LightTileCountX =
      (m_Context->Swapchain.Extent.width + LIGHT_TILE_SIZE - 1) /
      LIGHT_TILE_SIZE;
  m_LightTileCountY =
      (m_Context->Swapchain.Extent.height + LIGHT_TILE_SIZE - 1) /
      LIGHT_TILE_SIZE;

  const VkDeviceSize tileCount =
      static_cast<VkDeviceSize>(m_LightTileCountX) * m_LightTileCountY;
  const VkDeviceSize countBufferSize = tileCount * sizeof(uint32_t);
  const VkDeviceSize indexBufferSize =
      tileCount * MAX_POINT_LIGHTS_PER_TILE * sizeof(uint32_t);
  const VkDeviceSize spotIndexBufferSize =
      tileCount * MAX_SPOT_LIGHTS_PER_TILE * sizeof(uint32_t);

  for (FrameData &frame : m_Frames) {
    frame.TilePointLightCounts = MakeScope<Buffer>(
        m_Context, countBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    frame.TilePointLightIndices = MakeScope<Buffer>(
        m_Context, indexBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    frame.TileSpotLightCounts = MakeScope<Buffer>(
        m_Context, countBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    frame.TileSpotLightIndices = MakeScope<Buffer>(
        m_Context, spotIndexBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  }
}

void Renderer::CreateTextureDescriptorResources() {
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
}

void Renderer::CreateForwardPipeline() {

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

  std::vector<VkFormat> colorFormats = {
      m_Context->Swapchain.Format,
      GBuffer::EntityIDFormat,
  };

  PipelineDescription description{
      .VertexShader = shader->GetVertexShaderModule(),
      .FragmentShader = shader->GetFragmentShaderModule(),
      .VertexBinding = meshVertex.GetBindingDescription(),
      .VertexAttributes = meshVertex.GetAttributeDescriptions(),
      .CullMode = VK_CULL_MODE_BACK_BIT,
      .DepthTest = VK_TRUE,
      .DepthWrite = VK_TRUE,
      .DepthFormat = GBuffer::DepthFormat,
      .ColorFormats = colorFormats,
      .BlendAttachments = blendAttachments,
      .DescriptorSetLayouts = {m_TextureDescriptorSetLayout},
      .PushConstantRanges = {pushConstantRange},
  };

  m_ForwardPipeline.Init(m_Context->Device, description);
}

void Renderer::CreateGBufferPipeline() {
  auto *shader = m_ResourceManager->Load<Shader>("gbuffer");
  auto meshVertex = MeshVertex::GetLayout();

  VkPipelineColorBlendAttachmentState materialBlendAttachment{
      .blendEnable = VK_FALSE,
      .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
  };

  VkPipelineColorBlendAttachmentState entityIDBlendAttachment{
      .blendEnable = VK_FALSE,
      .colorWriteMask = VK_COLOR_COMPONENT_R_BIT,
  };

  VkPushConstantRange pushConstantRange{
      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
      .offset = 0,
      .size = sizeof(MeshPushConstants),
  };

  PipelineDescription description{
      .VertexShader = shader->GetVertexShaderModule(),
      .FragmentShader = shader->GetFragmentShaderModule(),
      .VertexBinding = meshVertex.GetBindingDescription(),
      .VertexAttributes = meshVertex.GetAttributeDescriptions(),
      .CullMode = VK_CULL_MODE_BACK_BIT,
      .DepthTest = VK_TRUE,
      .DepthWrite = VK_TRUE,
      .DepthFormat = GBuffer::DepthFormat,
      .ColorFormats = {
          GBuffer::AlbedoMetallicFormat,
          GBuffer::NormalRoughnessFormat,
          GBuffer::EntityIDFormat,
      },
      .BlendAttachments = {
          materialBlendAttachment,
          materialBlendAttachment,
          entityIDBlendAttachment,
      },
      .DescriptorSetLayouts = {m_TextureDescriptorSetLayout},
      .PushConstantRanges = {pushConstantRange},
  };

  m_GBufferPipeline.Init(m_Context->Device, description);
}

void Renderer::CreateGBufferDescriptorResources() {
  VkSamplerCreateInfo samplerInfo{
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .magFilter = VK_FILTER_NEAREST,
      .minFilter = VK_FILTER_NEAREST,
      .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
  };

  if (vkCreateSampler(m_Context->Device, &samplerInfo, nullptr,
                      &m_GBufferSampler) != VK_SUCCESS) {
    throw std::runtime_error("Failed To Create GBuffer Debug Sampler");
  }

  std::array<VkDescriptorSetLayoutBinding, 10> bindings{};
  for (uint32_t i = 0; i < 4; ++i) {
    bindings[i] = {
        .binding = i,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    };
  }
  bindings[4] = {
      .binding = 4,
      .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
  };
  bindings[5] = {
      .binding = 5,
      .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
  };
  bindings[6] = {
      .binding = 6,
      .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
  };
  bindings[7] = {
      .binding = 7,
      .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
  };
  bindings[8] = {
      .binding = 8,
      .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
  };
  bindings[9] = {
      .binding = 9,
      .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
  };

  VkDescriptorSetLayoutCreateInfo layoutInfo{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = static_cast<uint32_t>(bindings.size()),
      .pBindings = bindings.data(),
  };

  if (vkCreateDescriptorSetLayout(m_Context->Device, &layoutInfo, nullptr,
                                  &m_GBufferDescriptorSetLayout) !=
      VK_SUCCESS) {
    throw std::runtime_error(
        "Failed To Create GBuffer Debug Descriptor Set Layout");
  }

  std::array<VkDescriptorPoolSize, 2> poolSizes{
      VkDescriptorPoolSize{
          .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .descriptorCount = MAX_FRAMES_IN_FLIGHT * 4,
      },
      VkDescriptorPoolSize{
          .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
          .descriptorCount = MAX_FRAMES_IN_FLIGHT * 6,
      },
  };
  VkDescriptorPoolCreateInfo poolInfo{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .maxSets = MAX_FRAMES_IN_FLIGHT,
      .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
      .pPoolSizes = poolSizes.data(),
  };

  if (vkCreateDescriptorPool(m_Context->Device, &poolInfo, nullptr,
                             &m_GBufferDescriptorPool) != VK_SUCCESS) {
    throw std::runtime_error(
        "Failed To Create GBuffer Debug Descriptor Pool");
  }

  std::array<VkDescriptorSetLayout, MAX_FRAMES_IN_FLIGHT> layouts;
  layouts.fill(m_GBufferDescriptorSetLayout);

  VkDescriptorSetAllocateInfo allocInfo{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = m_GBufferDescriptorPool,
      .descriptorSetCount = MAX_FRAMES_IN_FLIGHT,
      .pSetLayouts = layouts.data(),
  };

  if (vkAllocateDescriptorSets(m_Context->Device, &allocInfo,
                               m_GBufferDescriptorSets.data()) !=
      VK_SUCCESS) {
    throw std::runtime_error(
        "Failed To Allocate GBuffer Debug Descriptor Sets");
  }

  UpdateGBufferDescriptorSets();
}

void Renderer::CreateGBufferDebugPipeline() {
  auto *shader = m_ResourceManager->Load<Shader>("gbuffer_debug");

  VkPipelineColorBlendAttachmentState blendAttachment{
      .blendEnable = VK_FALSE,
      .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
  };

  VkPushConstantRange pushConstantRange{
      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
      .offset = 0,
      .size = sizeof(GBufferDebugPushConstants),
  };

  PipelineDescription description{
      .VertexShader = shader->GetVertexShaderModule(),
      .FragmentShader = shader->GetFragmentShaderModule(),
      .CullMode = VK_CULL_MODE_NONE,
      .DepthTest = VK_FALSE,
      .DepthWrite = VK_FALSE,
      .ColorFormats = {m_Context->Swapchain.Format},
      .BlendAttachments = {blendAttachment},
      .DescriptorSetLayouts = {m_GBufferDescriptorSetLayout},
      .PushConstantRanges = {pushConstantRange},
  };

  m_GBufferDebugPipeline.Init(m_Context->Device, description);
}

void Renderer::CreateDeferredLightingPipeline() {
  auto *shader = m_ResourceManager->Load<Shader>("lighting");

  VkPipelineColorBlendAttachmentState blendAttachment{
      .blendEnable = VK_FALSE,
      .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
  };

  VkPushConstantRange pushConstantRange{
      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
      .offset = 0,
      .size = sizeof(DeferredLightingPushConstants),
  };

  PipelineDescription description{
      .VertexShader = shader->GetVertexShaderModule(),
      .FragmentShader = shader->GetFragmentShaderModule(),
      .CullMode = VK_CULL_MODE_NONE,
      .DepthTest = VK_FALSE,
      .DepthWrite = VK_FALSE,
      .ColorFormats = {HDRSceneColor::Format},
      .BlendAttachments = {blendAttachment},
      .DescriptorSetLayouts = {m_GBufferDescriptorSetLayout},
      .PushConstantRanges = {pushConstantRange},
  };

  m_DeferredLightingPipeline.Init(m_Context->Device, description);
}

void Renderer::CreateTiledLightCullingPipeline() {
  VkShaderModule computeShader =
      LoadShaderModule(m_Context, "assets/shaders/light_cull.comp.spv");

  VkPushConstantRange pushConstantRange{
      .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
      .offset = 0,
      .size = sizeof(TiledLightCullingPushConstants),
  };

  try {
    m_TiledLightCullingPipeline.Init(
        m_Context->Device, computeShader, {m_GBufferDescriptorSetLayout},
        {pushConstantRange});
  } catch (...) {
    vkDestroyShaderModule(m_Context->Device, computeShader, nullptr);
    throw;
  }

  vkDestroyShaderModule(m_Context->Device, computeShader, nullptr);
}

void Renderer::CreateToneMappingDescriptorResources() {
  VkSamplerCreateInfo samplerInfo{
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .magFilter = VK_FILTER_LINEAR,
      .minFilter = VK_FILTER_LINEAR,
      .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
  };

  if (vkCreateSampler(m_Context->Device, &samplerInfo, nullptr,
                      &m_ToneMappingSampler) != VK_SUCCESS) {
    throw std::runtime_error("Failed To Create Tone Mapping Sampler");
  }

  VkDescriptorSetLayoutBinding binding{
      .binding = 0,
      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
  };

  VkDescriptorSetLayoutCreateInfo layoutInfo{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = 1,
      .pBindings = &binding,
  };

  if (vkCreateDescriptorSetLayout(m_Context->Device, &layoutInfo, nullptr,
                                  &m_ToneMappingDescriptorSetLayout) !=
      VK_SUCCESS) {
    throw std::runtime_error(
        "Failed To Create Tone Mapping Descriptor Set Layout");
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
                             &m_ToneMappingDescriptorPool) != VK_SUCCESS) {
    throw std::runtime_error("Failed To Create Tone Mapping Descriptor Pool");
  }

  std::array<VkDescriptorSetLayout, MAX_FRAMES_IN_FLIGHT> layouts;
  layouts.fill(m_ToneMappingDescriptorSetLayout);

  VkDescriptorSetAllocateInfo allocInfo{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = m_ToneMappingDescriptorPool,
      .descriptorSetCount = MAX_FRAMES_IN_FLIGHT,
      .pSetLayouts = layouts.data(),
  };

  if (vkAllocateDescriptorSets(m_Context->Device, &allocInfo,
                               m_ToneMappingDescriptorSets.data()) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed To Allocate Tone Mapping Descriptor Sets");
  }

  UpdateToneMappingDescriptorSets();
}

void Renderer::CreateToneMappingPipeline() {
  auto *shader = m_ResourceManager->Load<Shader>("tonemap");

  VkPipelineColorBlendAttachmentState blendAttachment{
      .blendEnable = VK_FALSE,
      .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
  };

  VkPushConstantRange pushConstantRange{
      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
      .offset = 0,
      .size = sizeof(ToneMappingPushConstants),
  };

  PipelineDescription description{
      .VertexShader = shader->GetVertexShaderModule(),
      .FragmentShader = shader->GetFragmentShaderModule(),
      .CullMode = VK_CULL_MODE_NONE,
      .DepthTest = VK_FALSE,
      .DepthWrite = VK_FALSE,
      .ColorFormats = {m_Context->Swapchain.Format},
      .BlendAttachments = {blendAttachment},
      .DescriptorSetLayouts = {m_ToneMappingDescriptorSetLayout},
      .PushConstantRanges = {pushConstantRange},
  };

  m_ToneMappingPipeline.Init(m_Context->Device, description);
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

  // Pipeline Layout
  VkPushConstantRange pushConstantRange{
      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
      .offset = 0,
      .size = sizeof(GridPushConstants),
  };

  PipelineDescription description{
      .VertexShader = shader->GetVertexShaderModule(),
      .FragmentShader = shader->GetFragmentShaderModule(),
      .CullMode = VK_CULL_MODE_NONE,
      .DepthTest = VK_TRUE,
      .DepthWrite = VK_FALSE,
      .DepthFormat = GBuffer::DepthFormat,
      .ColorFormats = {m_Context->Swapchain.Format},
      .BlendAttachments = {colorBlendAttachment},
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

void Renderer::CreateDebugLinePipeline() {
  auto *shader = m_ResourceManager->Load<Shader>("gizmo");

  auto vertex = DebugLineVertex::GetLayout();

  VkPipelineColorBlendAttachmentState blendAttachment{
      .blendEnable = VK_FALSE,
      .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
  };

  VkPushConstantRange pushConstantRange{
      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
      .offset = 0,
      .size = sizeof(DebugLinePushConstants),
  };

  PipelineDescription description{
      .VertexShader = shader->GetVertexShaderModule(),
      .FragmentShader = shader->GetFragmentShaderModule(),
      .VertexBinding = vertex.GetBindingDescription(),
      .VertexAttributes = vertex.GetAttributeDescriptions(),
      .CullMode = VK_CULL_MODE_NONE,
      .DepthTest = VK_FALSE,
      .DepthWrite = VK_FALSE,
      .ColorFormats = {m_Context->Swapchain.Format},
      .BlendAttachments = {blendAttachment},
      .PushConstantRanges = {pushConstantRange},
      .Topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
  };

  m_DebugLinePipeline.Init(m_Context->Device, description);
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

FrameData &Renderer::BeginFrame() {
  VkCommandBufferBeginInfo beginInfo{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };

  FrameData &frameData = m_Frames[m_FrameIndex];

  if (vkBeginCommandBuffer(frameData.CommandBuffer, &beginInfo) != VK_SUCCESS) {
    throw std::runtime_error("Failed To start Forward Pass Command Buffer");
  }

  return frameData;
}

void Renderer::EndFrame(FrameData &frame) {
  TransitionImageLayout(
      frame.CommandBuffer, m_Context->Swapchain.Images[m_ImageIndex],
      VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
      {}, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
      VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT);

  vkEndCommandBuffer(frame.CommandBuffer);
}

void Renderer::RecordGBufferPass(FrameData &frame,
                                 const RenderWorld &renderWorld) {
  ZoneScopedN("Record GBuffer Pass");

  GBuffer &gbuffer = frame.GeometryBuffer;

  TransitionImageLayout(
      frame.CommandBuffer, gbuffer.AlbedoMetallic.GetImage(),
      VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, {},
      VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
      VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);

  TransitionImageLayout(
      frame.CommandBuffer, gbuffer.NormalRoughness.GetImage(),
      VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, {},
      VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
      VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);

  TransitionImageLayout(
      frame.CommandBuffer, gbuffer.EntityID.GetImage(),
      VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, {},
      VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
      VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);

  TransitionImageLayout(
      frame.CommandBuffer, gbuffer.Depth.GetImage(),
      VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, {},
      VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
      VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
      VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT);

  VkClearValue clearAlbedoMetallic{
      .color = {{0.0f, 0.0f, 0.0f, 0.0f}},
  };
  VkClearValue clearNormalRoughness{
      .color = {{0.0f, 0.0f, 1.0f, 1.0f}},
  };
  VkClearValue clearEntityID{
      .color = {.uint32 = {Entity::NULL_ENTITY, 0, 0, 0}},
  };

  std::array<VkRenderingAttachmentInfo, 3> colorAttachments{};
  colorAttachments[0] = {
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
      .imageView = gbuffer.AlbedoMetallic.GetView(),
      .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .clearValue = clearAlbedoMetallic,
  };
  colorAttachments[1] = {
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
      .imageView = gbuffer.NormalRoughness.GetView(),
      .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .clearValue = clearNormalRoughness,
  };
  colorAttachments[2] = {
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
      .imageView = gbuffer.EntityID.GetView(),
      .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .clearValue = clearEntityID,
  };

  VkClearValue clearDepth{
      .depthStencil = {1.0f, 0},
  };
  VkRenderingAttachmentInfo depthAttachment{
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
      .imageView = gbuffer.Depth.GetView(),
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

  vkCmdBeginRendering(frame.CommandBuffer, &renderingInfo);
  vkCmdBindPipeline(frame.CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    m_GBufferPipeline.Handle);

  VkViewport viewport{
      .x = 0.0f,
      .y = 0.0f,
      .width = static_cast<float>(m_Context->Swapchain.Extent.width),
      .height = static_cast<float>(m_Context->Swapchain.Extent.height),
      .minDepth = 0.0f,
      .maxDepth = 1.0f,
  };
  vkCmdSetViewport(frame.CommandBuffer, 0, 1, &viewport);

  VkRect2D scissor{
      .offset = {0, 0},
      .extent = m_Context->Swapchain.Extent,
  };
  vkCmdSetScissor(frame.CommandBuffer, 0, 1, &scissor);

  for (const RenderObject &object : renderWorld.Objects) {
    if (!object.MeshResource || !object.TextureResource)
      continue;

    MeshPushConstants push{
        .Mvp = m_ActiveCamera.Proj * m_ActiveCamera.View * object.Transform,
        .Model = object.Transform,
        .EntityID = object.EntityID,
    };

    vkCmdPushConstants(frame.CommandBuffer, m_GBufferPipeline.Layout,
                       VK_SHADER_STAGE_VERTEX_BIT |
                           VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(MeshPushConstants), &push);

    VkBuffer vertexBuffer = object.MeshResource->GetVertexBuffer()->Get();
    constexpr VkDeviceSize vertexOffset = 0;
    vkCmdBindVertexBuffers(frame.CommandBuffer, 0, 1, &vertexBuffer,
                           &vertexOffset);
    vkCmdBindIndexBuffer(
        frame.CommandBuffer, object.MeshResource->GetIndexBuffer()->Get(), 0,
        object.MeshResource->GetIndexBuffer()->GetIndexType());

    VkDescriptorSet textureSet = object.TextureResource->GetDescriptorSet();
    if (textureSet == VK_NULL_HANDLE) {
      object.TextureResource->CreateDescriptorSet(m_TextureDescriptorPool,
                                                  m_TextureDescriptorSetLayout);
      textureSet = object.TextureResource->GetDescriptorSet();
    }

    vkCmdBindDescriptorSets(frame.CommandBuffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_GBufferPipeline.Layout, 0, 1, &textureSet, 0,
                            nullptr);

    vkCmdDrawIndexed(frame.CommandBuffer, object.MeshResource->GetIndexCount(),
                     1, 0, 0, 0);
  }

  vkCmdEndRendering(frame.CommandBuffer);
}

void Renderer::RecordDeferredLightingPass(FrameData &frame,
                                          const RenderWorld &renderWorld,
                                          const RenderView &view) {
  ZoneScopedN("Record Deferred Lighting Pass");

  GBuffer &gbuffer = frame.GeometryBuffer;

  TransitionImageLayout(
      frame.CommandBuffer, gbuffer.AlbedoMetallic.GetImage(),
      VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_2_SHADER_READ_BIT,
      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
      VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);

  TransitionImageLayout(
      frame.CommandBuffer, gbuffer.NormalRoughness.GetImage(),
      VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_2_SHADER_READ_BIT,
      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
      VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);

  TransitionImageLayout(
      frame.CommandBuffer, gbuffer.Depth.GetImage(),
      VK_IMAGE_ASPECT_DEPTH_BIT,
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
      VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
      VK_ACCESS_2_SHADER_READ_BIT,
      VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
          VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
      VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);

  TransitionImageLayout(
      frame.CommandBuffer, gbuffer.EntityID.GetImage(),
      VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_2_SHADER_READ_BIT,
      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
      VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);

  TransitionImageLayout(
      frame.CommandBuffer, frame.SceneColor.Color.GetImage(),
      VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, {},
      VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
      VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);

  VkClearValue clearColor{
      .color = {{0.14f, 0.14f, 0.14f, 1.0f}},
  };
  VkRenderingAttachmentInfo colorAttachment{
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
      .imageView = frame.SceneColor.Color.GetView(),
      .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .clearValue = clearColor,
  };

  VkRenderingInfo renderingInfo{
      .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
      .renderArea = {.offset = {0, 0}, .extent = m_Context->Swapchain.Extent},
      .layerCount = 1,
      .colorAttachmentCount = 1,
      .pColorAttachments = &colorAttachment,
  };

  vkCmdBeginRendering(frame.CommandBuffer, &renderingInfo);
  vkCmdBindPipeline(frame.CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    m_DeferredLightingPipeline.Handle);

  VkViewport viewport{
      .x = 0.0f,
      .y = 0.0f,
      .width = static_cast<float>(m_Context->Swapchain.Extent.width),
      .height = static_cast<float>(m_Context->Swapchain.Extent.height),
      .minDepth = 0.0f,
      .maxDepth = 1.0f,
  };
  vkCmdSetViewport(frame.CommandBuffer, 0, 1, &viewport);

  VkRect2D scissor{
      .offset = {0, 0},
      .extent = m_Context->Swapchain.Extent,
  };
  vkCmdSetScissor(frame.CommandBuffer, 0, 1, &scissor);

  vkCmdBindDescriptorSets(frame.CommandBuffer,
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          m_DeferredLightingPipeline.Layout, 0, 1,
                          &m_GBufferDescriptorSets[m_FrameIndex], 0, nullptr);

  DeferredLightingPushConstants pushConstants{
      .InverseViewProjection =
          glm::inverse(view.Camera.Proj * view.Camera.View),
      .LightDirection = glm::vec4(0.0f, -1.0f, 0.0f, 0.0f),
      .LightColorIntensity = glm::vec4(0.0f),
      .Settings = glm::vec4(0.15f, 0.0f, 0.0f, 0.0f),
      .LightCounts = glm::uvec4(
          static_cast<uint32_t>(
              std::min(renderWorld.PointLights.size(),
                       static_cast<size_t>(MAX_POINT_LIGHTS))),
          static_cast<uint32_t>(
              std::min(renderWorld.SpotLights.size(),
                       static_cast<size_t>(MAX_SPOT_LIGHTS))),
          m_LightTileCountX, m_LightTileCountY),
  };

  if (!renderWorld.DirectionalLights.empty()) {
    const DirectionalLightData &light = renderWorld.DirectionalLights.front();
    pushConstants.LightDirection = light.Direction;
    pushConstants.LightColorIntensity = light.ColorIntensity;
    pushConstants.Settings.y = 1.0f;
  }

  vkCmdPushConstants(frame.CommandBuffer, m_DeferredLightingPipeline.Layout,
                     VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pushConstants),
                     &pushConstants);

  vkCmdDraw(frame.CommandBuffer, 3, 1, 0, 0);
  vkCmdEndRendering(frame.CommandBuffer);
}

void Renderer::RecordTiledLightCullingPass(FrameData &frame,
                                           const RenderWorld &renderWorld,
                                           const RenderView &view) {
  ZoneScopedN("Record Tiled Light Culling Pass");

  vkCmdBindPipeline(frame.CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                    m_TiledLightCullingPipeline.Handle);
  vkCmdBindDescriptorSets(
      frame.CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
      m_TiledLightCullingPipeline.Layout, 0, 1,
      &m_GBufferDescriptorSets[m_FrameIndex], 0, nullptr);

  const uint32_t pointLightCount = static_cast<uint32_t>(
      std::min(renderWorld.PointLights.size(),
               static_cast<size_t>(MAX_POINT_LIGHTS)));
  const uint32_t spotLightCount = static_cast<uint32_t>(
      std::min(renderWorld.SpotLights.size(),
               static_cast<size_t>(MAX_SPOT_LIGHTS)));
  TiledLightCullingPushConstants pushConstants{
      .ViewProjection = view.Camera.Proj * view.Camera.View,
      .ProjectionScreen =
          glm::vec4(glm::abs(view.Camera.Proj[0][0]),
                    glm::abs(view.Camera.Proj[1][1]),
                    static_cast<float>(m_Context->Swapchain.Extent.width),
                    static_cast<float>(m_Context->Swapchain.Extent.height)),
      .LightGrid = glm::uvec4(pointLightCount, m_LightTileCountX,
                              m_LightTileCountY,
                              MAX_POINT_LIGHTS_PER_TILE),
      .SpotLightGrid =
          glm::uvec4(spotLightCount, MAX_SPOT_LIGHTS_PER_TILE, 0u, 0u),
  };
  vkCmdPushConstants(frame.CommandBuffer, m_TiledLightCullingPipeline.Layout,
                     VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConstants),
                     &pushConstants);

  vkCmdDispatch(frame.CommandBuffer, m_LightTileCountX, m_LightTileCountY, 1);

  std::array<VkBufferMemoryBarrier2, 4> bufferBarriers{
      VkBufferMemoryBarrier2{
          .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
          .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
          .srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
          .dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
          .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
          .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .buffer = frame.TilePointLightCounts->Get(),
          .offset = 0,
          .size = VK_WHOLE_SIZE,
      },
      VkBufferMemoryBarrier2{
          .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
          .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
          .srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
          .dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
          .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
          .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .buffer = frame.TilePointLightIndices->Get(),
          .offset = 0,
          .size = VK_WHOLE_SIZE,
      },
      VkBufferMemoryBarrier2{
          .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
          .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
          .srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
          .dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
          .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
          .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .buffer = frame.TileSpotLightCounts->Get(),
          .offset = 0,
          .size = VK_WHOLE_SIZE,
      },
      VkBufferMemoryBarrier2{
          .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
          .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
          .srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
          .dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
          .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
          .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
          .buffer = frame.TileSpotLightIndices->Get(),
          .offset = 0,
          .size = VK_WHOLE_SIZE,
      },
  };
  VkDependencyInfo dependencyInfo{
      .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
      .bufferMemoryBarrierCount =
          static_cast<uint32_t>(bufferBarriers.size()),
      .pBufferMemoryBarriers = bufferBarriers.data(),
  };
  vkCmdPipelineBarrier2(frame.CommandBuffer, &dependencyInfo);
}

void Renderer::RecordToneMappingPass(FrameData &frame,
                                     const RenderView &view) {
  ZoneScopedN("Record Tone Mapping Pass");

  TransitionImageLayout(
      frame.CommandBuffer, frame.SceneColor.Color.GetImage(),
      VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_2_SHADER_READ_BIT,
      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
      VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);

  TransitionImageLayout(
      frame.CommandBuffer, m_Context->Swapchain.Images[m_ImageIndex],
      VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, {},
      VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);

  VkClearValue clearColor{
      .color = {{0.0f, 0.0f, 0.0f, 1.0f}},
  };
  VkRenderingAttachmentInfo colorAttachment{
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
      .imageView = m_Context->Swapchain.ImageViews[m_ImageIndex],
      .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .clearValue = clearColor,
  };

  VkRenderingInfo renderingInfo{
      .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
      .renderArea = {.offset = {0, 0}, .extent = m_Context->Swapchain.Extent},
      .layerCount = 1,
      .colorAttachmentCount = 1,
      .pColorAttachments = &colorAttachment,
  };

  vkCmdBeginRendering(frame.CommandBuffer, &renderingInfo);
  vkCmdBindPipeline(frame.CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    m_ToneMappingPipeline.Handle);

  VkViewport viewport{
      .x = 0.0f,
      .y = 0.0f,
      .width = static_cast<float>(m_Context->Swapchain.Extent.width),
      .height = static_cast<float>(m_Context->Swapchain.Extent.height),
      .minDepth = 0.0f,
      .maxDepth = 1.0f,
  };
  vkCmdSetViewport(frame.CommandBuffer, 0, 1, &viewport);

  VkRect2D scissor{
      .offset = {0, 0},
      .extent = m_Context->Swapchain.Extent,
  };
  vkCmdSetScissor(frame.CommandBuffer, 0, 1, &scissor);

  vkCmdBindDescriptorSets(
      frame.CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
      m_ToneMappingPipeline.Layout, 0, 1,
      &m_ToneMappingDescriptorSets[m_FrameIndex], 0, nullptr);

  ToneMappingPushConstants pushConstants{
      .Exposure = std::max(view.Exposure, 0.0f),
  };
  vkCmdPushConstants(frame.CommandBuffer, m_ToneMappingPipeline.Layout,
                     VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pushConstants),
                     &pushConstants);

  vkCmdDraw(frame.CommandBuffer, 3, 1, 0, 0);
  vkCmdEndRendering(frame.CommandBuffer);
}

void Renderer::RecordGBufferDebugPass(FrameData &frame,
                                      const RenderView &view) {
  ZoneScopedN("Record GBuffer Debug Pass");

  GBuffer &gbuffer = frame.GeometryBuffer;

  TransitionImageLayout(
      frame.CommandBuffer, gbuffer.AlbedoMetallic.GetImage(),
      VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_2_SHADER_READ_BIT,
      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
      VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);

  TransitionImageLayout(
      frame.CommandBuffer, gbuffer.EntityID.GetImage(),
      VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_2_SHADER_READ_BIT,
      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
      VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);

  TransitionImageLayout(
      frame.CommandBuffer, gbuffer.NormalRoughness.GetImage(),
      VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_2_SHADER_READ_BIT,
      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
      VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);

  TransitionImageLayout(
      frame.CommandBuffer, gbuffer.Depth.GetImage(),
      VK_IMAGE_ASPECT_DEPTH_BIT,
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
      VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
      VK_ACCESS_2_SHADER_READ_BIT,
      VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
          VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
      VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);

  TransitionImageLayout(
      frame.CommandBuffer, m_Context->Swapchain.Images[m_ImageIndex],
      VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, {},
      VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);

  VkClearValue clearColor{
      .color = {{0.0f, 0.0f, 0.0f, 1.0f}},
  };
  VkRenderingAttachmentInfo colorAttachment{
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
      .imageView = m_Context->Swapchain.ImageViews[m_ImageIndex],
      .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .clearValue = clearColor,
  };

  VkRenderingInfo renderingInfo{
      .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
      .renderArea = {.offset = {0, 0}, .extent = m_Context->Swapchain.Extent},
      .layerCount = 1,
      .colorAttachmentCount = 1,
      .pColorAttachments = &colorAttachment,
  };

  vkCmdBeginRendering(frame.CommandBuffer, &renderingInfo);
  vkCmdBindPipeline(frame.CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    m_GBufferDebugPipeline.Handle);

  VkViewport viewport{
      .x = 0.0f,
      .y = 0.0f,
      .width = static_cast<float>(m_Context->Swapchain.Extent.width),
      .height = static_cast<float>(m_Context->Swapchain.Extent.height),
      .minDepth = 0.0f,
      .maxDepth = 1.0f,
  };
  vkCmdSetViewport(frame.CommandBuffer, 0, 1, &viewport);

  VkRect2D scissor{
      .offset = {0, 0},
      .extent = m_Context->Swapchain.Extent,
  };
  vkCmdSetScissor(frame.CommandBuffer, 0, 1, &scissor);

  vkCmdBindDescriptorSets(
      frame.CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
      m_GBufferDebugPipeline.Layout, 0, 1,
      &m_GBufferDescriptorSets[m_FrameIndex], 0, nullptr);

  GBufferDebugPushConstants pushConstants{
      .InverseProjection = glm::inverse(view.Camera.Proj),
      .NearPlane = view.Camera.NearPlane,
      .FarPlane = view.Camera.FarPlane,
      .ViewMode = static_cast<uint32_t>(view.GBufferView),
  };
  vkCmdPushConstants(frame.CommandBuffer, m_GBufferDebugPipeline.Layout,
                     VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pushConstants),
                     &pushConstants);

  vkCmdDraw(frame.CommandBuffer, 3, 1, 0, 0);
  vkCmdEndRendering(frame.CommandBuffer);
}

void Renderer::RecordGridPass(FrameData &frame, const RenderView &view) {
  ZoneScopedN("Record Grid Pass");

  TransitionImageLayout(
      frame.CommandBuffer, frame.GeometryBuffer.Depth.GetImage(),
      VK_IMAGE_ASPECT_DEPTH_BIT,
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
      VK_ACCESS_2_SHADER_READ_BIT,
      VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
      VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
      VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
          VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT);

  VkRenderingAttachmentInfo colorAttachment{
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
      .imageView = m_Context->Swapchain.ImageViews[m_ImageIndex],
      .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
  };

  VkRenderingAttachmentInfo depthAttachment{
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
      .imageView = frame.GeometryBuffer.Depth.GetView(),
      .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
      .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
  };

  VkRenderingInfo renderingInfo{
      .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
      .renderArea = {.offset = {0, 0}, .extent = m_Context->Swapchain.Extent},
      .layerCount = 1,
      .colorAttachmentCount = 1,
      .pColorAttachments = &colorAttachment,
      .pDepthAttachment = &depthAttachment,
  };

  vkCmdBeginRendering(frame.CommandBuffer, &renderingInfo);
  vkCmdBindPipeline(frame.CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    m_GridPipeline.Handle);

  VkViewport viewport{
      .x = 0.0f,
      .y = 0.0f,
      .width = static_cast<float>(m_Context->Swapchain.Extent.width),
      .height = static_cast<float>(m_Context->Swapchain.Extent.height),
      .minDepth = 0.0f,
      .maxDepth = 1.0f,
  };
  vkCmdSetViewport(frame.CommandBuffer, 0, 1, &viewport);

  VkRect2D scissor{
      .offset = {0, 0},
      .extent = m_Context->Swapchain.Extent,
  };
  vkCmdSetScissor(frame.CommandBuffer, 0, 1, &scissor);

  const glm::mat4 viewProjection = view.Camera.Proj * view.Camera.View;
  GridPushConstants pushConstants{
      .ViewProjection = viewProjection,
      .InverseViewProjection = glm::inverse(viewProjection),
  };
  vkCmdPushConstants(frame.CommandBuffer, m_GridPipeline.Layout,
                     VK_SHADER_STAGE_VERTEX_BIT |
                         VK_SHADER_STAGE_FRAGMENT_BIT,
                     0, sizeof(pushConstants), &pushConstants);

  vkCmdDraw(frame.CommandBuffer, 6, 1, 0, 0);
  vkCmdEndRendering(frame.CommandBuffer);
}

void Renderer::RecordForwardPass(FrameData &frame,
                                 const RenderWorld &renderWorld) {
  ZoneScopedN("Record Forward Pass");

  TransitionImageLayout(frame.CommandBuffer,
                        m_Context->Swapchain.Images[m_ImageIndex],
                        VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, {},
                        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);

  TransitionImageLayout(frame.CommandBuffer,
                        frame.GeometryBuffer.Depth.GetImage(),
                        VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, {},
                        VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                        VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
                        VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT);

  TransitionImageLayout(frame.CommandBuffer,
                        frame.GeometryBuffer.EntityID.GetImage(),
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
      .imageView = frame.GeometryBuffer.EntityID.GetView(),
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
      .imageView = frame.GeometryBuffer.Depth.GetView(),
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

  vkCmdBeginRendering(frame.CommandBuffer, &renderingInfo);

  vkCmdBindPipeline(frame.CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    m_ForwardPipeline.Handle);

  VkViewport viewport{
      .x = 0.0f,
      .y = 0.0f,
      .width = static_cast<float>(m_Context->Swapchain.Extent.width),
      .height = static_cast<float>(m_Context->Swapchain.Extent.height),
      .minDepth = 0.0f,
      .maxDepth = 1.0f,
  };
  vkCmdSetViewport(frame.CommandBuffer, 0, 1, &viewport);

  VkRect2D scissor{
      .offset{0, 0},
      .extent = m_Context->Swapchain.Extent,
  };
  vkCmdSetScissor(frame.CommandBuffer, 0, 1, &scissor);

  for (const RenderObject &object : renderWorld.Objects) {
    if (!object.MeshResource || !object.TextureResource)
      continue;

    MeshPushConstants push{
        .Mvp = m_ActiveCamera.Proj * m_ActiveCamera.View * object.Transform,
        .Model = object.Transform,
        .EntityID = object.EntityID,
    };

    vkCmdPushConstants(frame.CommandBuffer, m_ForwardPipeline.Layout,
                       VK_SHADER_STAGE_VERTEX_BIT |
                           VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(MeshPushConstants), &push);

    VkBuffer vertexBuffers[] = {
        object.MeshResource->GetVertexBuffer()->Get()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(frame.CommandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(
        frame.CommandBuffer, object.MeshResource->GetIndexBuffer()->Get(), 0,
        object.MeshResource->GetIndexBuffer()->GetIndexType());

    VkDescriptorSet textureSet = object.TextureResource->GetDescriptorSet();
    // TODO: Fix This On multithreading
    if (textureSet == VK_NULL_HANDLE) {
      object.TextureResource->CreateDescriptorSet(m_TextureDescriptorPool,
                                                  m_TextureDescriptorSetLayout);
      textureSet = object.TextureResource->GetDescriptorSet();
    }
    vkCmdBindDescriptorSets(
        frame.CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_ForwardPipeline.Layout, 0, 1, &textureSet, 0, nullptr);

    vkCmdDrawIndexed(frame.CommandBuffer, object.MeshResource->GetIndexCount(),
                     1, 0, 0, 0);
  }

  // Rendering End

  vkCmdEndRendering(frame.CommandBuffer);
};

void Renderer::RecordOutlinePass(FrameData &frame, const RenderView &view) {
  ZoneScopedN("Record Outline Pass");

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

  vkCmdBeginRendering(frame.CommandBuffer, &renderingInfo);
  const uint32_t selectedEntity = view.SelectedEntity;

  if (selectedEntity != Entity::NULL_ENTITY) {
    vkCmdBindPipeline(frame.CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      m_OutlinePipeline.Handle);

    VkViewport viewport{
        .width = static_cast<float>(m_Context->Swapchain.Extent.width),
        .height = static_cast<float>(m_Context->Swapchain.Extent.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
    vkCmdSetViewport(frame.CommandBuffer, 0, 1, &viewport);

    VkRect2D scissor{
        .extent = m_Context->Swapchain.Extent,
    };
    vkCmdSetScissor(frame.CommandBuffer, 0, 1, &scissor);

    vkCmdBindDescriptorSets(frame.CommandBuffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_OutlinePipeline.Layout, 0, 1,
                            &m_OutlineDescriptorSets[m_FrameIndex], 0, nullptr);

    OutlinePushConstants pushConstants{
        .Color = {1.0f, 0.6f, 0.0f},
        .EntityID = selectedEntity,
        .ThicknessPX = 2,
    };
    vkCmdPushConstants(frame.CommandBuffer, m_OutlinePipeline.Layout,
                       VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pushConstants),
                       &pushConstants);

    vkCmdDraw(frame.CommandBuffer, 3, 1, 0, 0);
  }

  vkCmdEndRendering(frame.CommandBuffer);
}

void Renderer::UpdateOutlineDescriptorSets() {
  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    VkDescriptorImageInfo imageInfo{
        .sampler = m_OutlineSampler,
        .imageView = m_Frames[i].GeometryBuffer.EntityID.GetView(),
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

void Renderer::UpdateGBufferDescriptorSets() {
  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    const GBuffer &gbuffer = m_Frames[i].GeometryBuffer;

    std::array<VkDescriptorImageInfo, 4> imageInfos{
        VkDescriptorImageInfo{
            .sampler = m_GBufferSampler,
            .imageView = gbuffer.AlbedoMetallic.GetView(),
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        },
        VkDescriptorImageInfo{
            .sampler = m_GBufferSampler,
            .imageView = gbuffer.NormalRoughness.GetView(),
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        },
        VkDescriptorImageInfo{
            .sampler = m_GBufferSampler,
            .imageView = gbuffer.Depth.GetView(),
            .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
        },
        VkDescriptorImageInfo{
            .sampler = m_GBufferSampler,
            .imageView = gbuffer.EntityID.GetView(),
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        },
    };

    VkDescriptorBufferInfo pointLightBufferInfo{
        .buffer = m_Frames[i].PointLightBuffer->Get(),
        .offset = 0,
        .range = m_Frames[i].PointLightBuffer->GetSize(),
    };
    VkDescriptorBufferInfo spotLightBufferInfo{
        .buffer = m_Frames[i].SpotLightBuffer->Get(),
        .offset = 0,
        .range = m_Frames[i].SpotLightBuffer->GetSize(),
    };
    VkDescriptorBufferInfo tilePointLightCountBufferInfo{
        .buffer = m_Frames[i].TilePointLightCounts->Get(),
        .offset = 0,
        .range = m_Frames[i].TilePointLightCounts->GetSize(),
    };
    VkDescriptorBufferInfo tilePointLightIndexBufferInfo{
        .buffer = m_Frames[i].TilePointLightIndices->Get(),
        .offset = 0,
        .range = m_Frames[i].TilePointLightIndices->GetSize(),
    };
    VkDescriptorBufferInfo tileSpotLightCountBufferInfo{
        .buffer = m_Frames[i].TileSpotLightCounts->Get(),
        .offset = 0,
        .range = m_Frames[i].TileSpotLightCounts->GetSize(),
    };
    VkDescriptorBufferInfo tileSpotLightIndexBufferInfo{
        .buffer = m_Frames[i].TileSpotLightIndices->Get(),
        .offset = 0,
        .range = m_Frames[i].TileSpotLightIndices->GetSize(),
    };

    std::array<VkWriteDescriptorSet, 10> writes{};
    for (uint32_t binding = 0; binding < imageInfos.size(); ++binding) {
      writes[binding] = {
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstSet = m_GBufferDescriptorSets[i],
          .dstBinding = binding,
          .descriptorCount = 1,
          .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .pImageInfo = &imageInfos[binding],
      };
    }
    writes[4] = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = m_GBufferDescriptorSets[i],
        .dstBinding = 4,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .pBufferInfo = &pointLightBufferInfo,
    };
    writes[5] = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = m_GBufferDescriptorSets[i],
        .dstBinding = 5,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .pBufferInfo = &spotLightBufferInfo,
    };
    writes[6] = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = m_GBufferDescriptorSets[i],
        .dstBinding = 6,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .pBufferInfo = &tilePointLightCountBufferInfo,
    };
    writes[7] = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = m_GBufferDescriptorSets[i],
        .dstBinding = 7,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .pBufferInfo = &tilePointLightIndexBufferInfo,
    };
    writes[8] = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = m_GBufferDescriptorSets[i],
        .dstBinding = 8,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .pBufferInfo = &tileSpotLightCountBufferInfo,
    };
    writes[9] = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = m_GBufferDescriptorSets[i],
        .dstBinding = 9,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .pBufferInfo = &tileSpotLightIndexBufferInfo,
    };

    vkUpdateDescriptorSets(m_Context->Device,
                           static_cast<uint32_t>(writes.size()), writes.data(),
                           0, nullptr);
  }
}

void Renderer::UpdateLightBuffers(const RenderWorld &renderWorld) {
  FrameData &frame = m_Frames[m_FrameIndex];

  const size_t pointLightCount =
      std::min(renderWorld.PointLights.size(),
               static_cast<size_t>(MAX_POINT_LIGHTS));
  const size_t spotLightCount =
      std::min(renderWorld.SpotLights.size(),
               static_cast<size_t>(MAX_SPOT_LIGHTS));

  frame.PointLightBuffer->Update(
      renderWorld.PointLights.data(), pointLightCount * sizeof(PointLightData));
  frame.SpotLightBuffer->Update(
      renderWorld.SpotLights.data(), spotLightCount * sizeof(SpotLightData));
}

void Renderer::UpdateToneMappingDescriptorSets() {
  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    VkDescriptorImageInfo imageInfo{
        .sampler = m_ToneMappingSampler,
        .imageView = m_Frames[i].SceneColor.Color.GetView(),
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    VkWriteDescriptorSet write{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = m_ToneMappingDescriptorSets[i],
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

  CreateGBufferImages();
  CreateHDRSceneColorImages();
  CreateTiledLightCullingBuffers();

  UpdateGBufferDescriptorSets();
  UpdateToneMappingDescriptorSets();
  UpdateOutlineDescriptorSets();

  m_Resized = false;
}

// Event Methods
std::optional<uint32_t> Renderer::PickEntity(int32_t mouseX,
                                             int32_t mouseY) const {
  uint32_t lastFrameIndex =
      (m_FrameIndex + MAX_FRAMES_IN_FLIGHT - 1) % MAX_FRAMES_IN_FLIGHT;
  VkImage pickingImage =
      m_Frames[lastFrameIndex].GeometryBuffer.EntityID.GetImage();

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

void Renderer::UpdateDebugLineBuffer(
    std::span<const DebugLineVertex> debugLines) {
  const VkDeviceSize requiredSize =
      static_cast<VkDeviceSize>(debugLines.size_bytes());
  auto &vertexBuffer = m_DebugLineVertexBuffers[m_FrameIndex];

  if (!vertexBuffer || vertexBuffer->GetSize() < requiredSize) {
    vertexBuffer =
        MakeScope<VertexBuffer<DebugLineVertex>>(m_Context, requiredSize);
  }

  vertexBuffer->Upload(debugLines.data(), requiredSize);
}

void Renderer::RecordDebugLinePass(FrameData &frame,
                                   const RenderView &view) {
  ZoneScopedN("Record Debug Line Pass");

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

  vkCmdBeginRendering(frame.CommandBuffer, &renderingInfo);

  vkCmdBindPipeline(frame.CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    m_DebugLinePipeline.Handle);

  VkViewport viewport{
      .x = 0.0f,
      .y = 0.0f,
      .width = static_cast<float>(m_Context->Swapchain.Extent.width),
      .height = static_cast<float>(m_Context->Swapchain.Extent.height),
      .minDepth = 0.0f,
      .maxDepth = 1.0f,
  };
  vkCmdSetViewport(frame.CommandBuffer, 0, 1, &viewport);

  VkRect2D scissor{
      .offset{0, 0},
      .extent = m_Context->Swapchain.Extent,
  };
  vkCmdSetScissor(frame.CommandBuffer, 0, 1, &scissor);

  DebugLinePushConstants push{
      .View = m_ActiveCamera.View,
      .Proj = m_ActiveCamera.Proj,
  };

  vkCmdPushConstants(frame.CommandBuffer, m_DebugLinePipeline.Layout,
                     VK_SHADER_STAGE_VERTEX_BIT, 0,
                     sizeof(DebugLinePushConstants), &push);

  VkBuffer vertexBuffers[] = {m_DebugLineVertexBuffers[m_FrameIndex]->Get()};
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(frame.CommandBuffer, 0, 1, vertexBuffers, offsets);

  vkCmdDraw(frame.CommandBuffer,
            static_cast<uint32_t>(view.DebugLines.size()), 1, 0, 0);

  vkCmdEndRendering(frame.CommandBuffer);
}
} // namespace Inferno
