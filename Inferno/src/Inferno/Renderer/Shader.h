#pragma once

#include "Inferno/Resource/Resource.h"
#include <vulkan/vulkan_core.h>

namespace Inferno {
class Shader : public Resource {
public:
  Shader(const std::string &id, VkShaderStageFlagBits shaderStage)
      : Resource(id), m_Stage(shaderStage) {}
  ~Shader() = default;

  Shader(const Shader &) = delete;
  Shader &operator=(const Shader &) = delete;

  Shader(Shader &&other);
  Shader &operator=(Shader &&other);

private:
  VkShaderModule m_ShaderModule = VK_NULL_HANDLE;
  VkShaderStageFlagBits m_Stage;
};
} // namespace Inferno
