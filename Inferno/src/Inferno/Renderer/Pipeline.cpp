#include "Pipeline.h"

#include <pch.h>
#include <volk/volk.h>

namespace Inferno {
void Pipeline::Init(VkDevice device, PipelineDescription &description) {
  // Shader Stage
  VkPipelineShaderStageCreateInfo shaderStages[]{
      {
          .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage = VK_SHADER_STAGE_VERTEX_BIT,
          .module = description.VertexShader,
          .pName = "main",
      },
      {
          .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
          .module = description.FragmentShader,
          .pName = "main",
      },
  };

  bool hasVertexBinding = !description.VertexAttributes.empty();

  // Vertex Input
  VkPipelineVertexInputStateCreateInfo vertexInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .vertexBindingDescriptionCount = (hasVertexBinding ? 1u : 0u),
      .pVertexBindingDescriptions =
          (hasVertexBinding ? &description.VertexBinding : nullptr),
      .vertexAttributeDescriptionCount =
          static_cast<uint32_t>(description.VertexAttributes.size()),
      .pVertexAttributeDescriptions = description.VertexAttributes.data(),
  };

  // Input Assembly Stage
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
      .cullMode = description.CullMode,
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

  // Depth Stencil State
  VkPipelineDepthStencilStateCreateInfo depthInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
      .depthTestEnable = description.DepthTest,
      .depthWriteEnable = description.DepthWrite,
      .depthCompareOp = VK_COMPARE_OP_LESS,
      .stencilTestEnable = VK_FALSE,
  };

  // Color Blend State
  VkPipelineColorBlendStateCreateInfo colorBlendInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .logicOpEnable = VK_FALSE,
      .logicOp = VK_LOGIC_OP_COPY,
      .attachmentCount =
          static_cast<uint32_t>(description.BlendAttachments.size()),
      .pAttachments = description.BlendAttachments.data(),
  };

  // Pipeline Layout
  VkPipelineLayoutCreateInfo layoutInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount =
          static_cast<uint32_t>(description.DescriptorSetLayouts.size()),
      .pSetLayouts = description.DescriptorSetLayouts.data(),
      .pushConstantRangeCount =
          static_cast<uint32_t>(description.PushConstantRanges.size()),
      .pPushConstantRanges = description.PushConstantRanges.data(),
  };

  if (vkCreatePipelineLayout(device, &layoutInfo, nullptr, &Layout) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed To Create Pipeline Layout");
  }

  // Dynamic Rendering
  VkPipelineRenderingCreateInfo renderingInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
      .colorAttachmentCount =
          static_cast<uint32_t>(description.ColorFormats.size()),
      .pColorAttachmentFormats = description.ColorFormats.data(),
      .depthAttachmentFormat =
          description.DepthFormat, // TODO: Query this in startup
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
      .layout = Layout,
      .renderPass = nullptr,
  };

  if (vkCreateGraphicsPipelines(device, nullptr, 1, &pipelineInfo, nullptr,
                                &Handle) != VK_SUCCESS) {
    throw std::runtime_error("Failed To Create Forward Pipeline");
  }
}

void Pipeline::Destroy(VkDevice device) {
  if (Handle != VK_NULL_HANDLE) {
    vkDestroyPipeline(device, Handle, nullptr);
  }
  if (Layout != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(device, Layout, nullptr);
  }
  Handle = VK_NULL_HANDLE;
  Layout = VK_NULL_HANDLE;
}

}; // namespace Inferno
