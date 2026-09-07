#pragma once

#include <vector>
#include <vulkan/vulkan_core.h>

namespace Inferno {
struct PipelineDescription {
  VkShaderModule VertexShader = VK_NULL_HANDLE;
  VkShaderModule FragmentShader = VK_NULL_HANDLE;

  VkVertexInputBindingDescription VertexBinding{};
  std::vector<VkVertexInputAttributeDescription> VertexAttributes{};

  VkCullModeFlags CullMode = VK_CULL_MODE_BACK_BIT;

  VkBool32 DepthTest = VK_TRUE;
  VkBool32 DepthWrite = VK_TRUE;
  VkFormat DepthFormat = VK_FORMAT_UNDEFINED;

  std::vector<VkFormat> ColorFormats{};
  std::vector<VkPipelineColorBlendAttachmentState> BlendAttachments{};

  std::vector<VkDescriptorSetLayout> DescriptorSetLayouts{};
  std::vector<VkPushConstantRange> PushConstantRanges{};
};

struct Pipeline {
  VkPipeline Handle = VK_NULL_HANDLE;
  VkPipelineLayout Layout = VK_NULL_HANDLE;

public:
  void Init(VkDevice device, PipelineDescription &description);
  void Destroy(VkDevice device);
};

} // namespace Inferno
