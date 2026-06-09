#pragma once

#include "Inferno/Renderer/DeviceContext.h"
#include "Inferno/Resource/Resource.h"
#include <vulkan/vulkan_core.h>

namespace Inferno {
class Shader : public Resource {
public:
  Shader(const std::string &id, const DeviceContext* context)
      : Resource(id), m_Context(context) {}
  ~Shader() = default;

  Shader(const Shader &) = delete;
  Shader &operator=(const Shader &) = delete;

  Shader(Shader &&other);
  Shader &operator=(Shader &&other);

  VkShaderModule GetVertexShaderModule() const { return m_VertexShaderModule; }
  VkShaderModule GetFragmentShaderModule() const { return m_FragmentShaderModule; }

protected:
  bool DoLoad() override;
  bool DoUnLoad() override;

private:
  std::vector<uint32_t> ReadFile(const std::string &filePath);
  VkShaderModule CreateShaderModule(const std::vector<uint32_t> &code);

  void CleanUp();

private:
  const DeviceContext *m_Context = nullptr;

  VkShaderModule m_VertexShaderModule = VK_NULL_HANDLE;
  VkShaderModule m_FragmentShaderModule = VK_NULL_HANDLE;

};
} // namespace Inferno
