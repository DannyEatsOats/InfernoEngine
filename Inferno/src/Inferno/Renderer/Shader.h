#pragma once
#include <memory>
#include <string>
#include <vulkan/vulkan_core.h>

typedef unsigned int GLenum;

namespace Inferno {
class Shader {
public:
  Shader(const std::string &name, const VkDevice *device,
         const std::string &vertexSrc, const std::string &fragmentSrc);
  Shader(const std::string &path);
  ~Shader();

  const std::string &GetName() const { return m_Name; }

  const VkShaderModule &GetVertexShaderModule() const {
    return m_VertShaderModule;
  }
  const VkShaderModule &GetFragmentShaderModule() const {
    return m_FragShaderModule;
  }

  static std::shared_ptr<Shader> Create(const std::string &name,
                                        const VkDevice *device,
                                        const std::string &vertexSrc,
                                        const std::string &fragmentSrc);
  static std::shared_ptr<Shader> Create(const std::string &path);

private:
  std::vector<char> ReadFile(const std::string &path);
  VkShaderModule CreateShaderModule(const std::vector<char> &code);

  const VkDevice *m_pDevice = nullptr;

private:
  std::string m_Name;
  VkShaderModule m_VertShaderModule;
  VkShaderModule m_FragShaderModule;
};
} // namespace Inferno
