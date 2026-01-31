#include "Shader.h"
#include <filesystem>
#include <fstream>
#include <memory>
#include <pch.h>
#include <stdexcept>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace Inferno {
Shader::Shader(const std::string &name,const VkDevice* device, const std::string &vertexSrc,
               const std::string &fragmentSrc) : m_Name(name), m_pDevice(device) {
  auto vertexShaderCode = ReadFile(vertexSrc);
  auto fragmentShaderCode = ReadFile(fragmentSrc);

    m_VertShaderModule = CreateShaderModule(vertexShaderCode);
    m_FragShaderModule = CreateShaderModule(fragmentShaderCode);
}

Shader::Shader(const std::string &path) {}

Shader::~Shader() {
    vkDestroyShaderModule(*m_pDevice, m_FragShaderModule, nullptr);
    vkDestroyShaderModule(*m_pDevice, m_VertShaderModule, nullptr);
}

std::shared_ptr<Shader> Shader::Create(const std::string &name,
                                       const VkDevice* device,
                                       const std::string &vertexSrc,
                                       const std::string &fragmentSrc) {
  return std::make_shared<Shader>(name, device, vertexSrc, fragmentSrc);
}

std::shared_ptr<Shader> Shader::Create(const std::string &path) {
  return std::make_shared<Shader>(path);
}

std::vector<char> Shader::ReadFile(const std::string &path) {
  std::filesystem::path exePath =
      std::filesystem::canonical("/proc/self/exe").parent_path();
  std::filesystem::path fullPath = exePath / path;

  std::ifstream file(fullPath, std::ios::ate | std::ios::binary);

  if (!file.is_open()) {
    throw std::runtime_error("Failed to open file: " + path);
  }

  size_t fileSize = static_cast<size_t>(file.tellg());
  std::vector<char> buffer(fileSize);
  file.seekg(0);
  file.read(buffer.data(), fileSize);
  file.close();

  return buffer;
}

VkShaderModule Shader::CreateShaderModule(const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = code.size(),
        .pCode = reinterpret_cast<const uint32_t*>(code.data()),
    };

    VkShaderModule shaderModule;
    if(vkCreateShaderModule(*m_pDevice, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module: " + m_Name);
    }

    return shaderModule;
}
} // namespace Inferno
