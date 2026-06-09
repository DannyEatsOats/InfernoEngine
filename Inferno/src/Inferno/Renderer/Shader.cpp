#include <filesystem>
#include <fstream>
#include <pch.h>
#include <stdexcept>
#include <vector>
#include <vulkan/vulkan_core.h>

#include "Inferno/Resource/Resource.h"
#include "Shader.h"

namespace Inferno {
Shader::Shader(Shader &&other)
    : Resource(std::move(other)), m_Context(other.m_Context),
      m_VertexShaderModule(other.m_VertexShaderModule),
      m_FragmentShaderModule(other.m_FragmentShaderModule) {
  other.m_Context = nullptr;
  other.m_VertexShaderModule = VK_NULL_HANDLE;
  other.m_FragmentShaderModule = VK_NULL_HANDLE;
}

Shader &Shader::operator=(Shader &&other) {
  if (this == &other) {
    return *this;
  }

  CleanUp();

  Resource::operator=(std::move(other));

  m_Context = other.m_Context;
  m_VertexShaderModule = other.m_VertexShaderModule;
  m_FragmentShaderModule = other.m_FragmentShaderModule;

  other.m_Context = nullptr;
  other.m_VertexShaderModule = VK_NULL_HANDLE;
  other.m_FragmentShaderModule = VK_NULL_HANDLE;

  return *this;
}
bool Shader::DoLoad() {
  std::string extension;

  std::string vertexFilePath = "assets/shaders/" + GetID() + ".vert" + ".spv";
  std::string fragmentFilePath = "assets/shaders/" + GetID() + ".frag" + ".spv";

  auto vertexShaderCode = ReadFile(vertexFilePath);

  auto fragmentShaderCode = ReadFile(fragmentFilePath);

  m_VertexShaderModule = CreateShaderModule(vertexShaderCode);
  m_FragmentShaderModule = CreateShaderModule(fragmentShaderCode);

  return true;
}

bool Shader::DoUnLoad() {
  if (IsLoaded()) {
    CleanUp();
  }

  return true;
}

std::vector<uint32_t> Shader::ReadFile(const std::string &filePath) {
  std::filesystem::path exePath =
      std::filesystem::canonical("/proc/self/exe").parent_path();
  std::filesystem::path fullPath = exePath / filePath;

  std::ifstream file(fullPath, std::ios::ate | std::ios::binary);

  if (!file.is_open()) {
    std::string msg = fullPath;
    throw std::runtime_error("Failed to Open File: " + msg);
  }

  size_t fileSize = static_cast<size_t>(file.tellg());
  std::vector<uint32_t> buffer(fileSize);
  file.seekg(0);
  file.read(reinterpret_cast<char *>(buffer.data()), fileSize);
  file.close();

  return buffer;
}

VkShaderModule Shader::CreateShaderModule(const std::vector<uint32_t> &code) {
  VkShaderModuleCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  createInfo.codeSize = code.size();
  createInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());

  VkShaderModule shaderModule;
  if (vkCreateShaderModule(m_Context->Device, &createInfo, nullptr,
                           &shaderModule) != VK_SUCCESS) {
    throw std::runtime_error("Failed to Create Shader Module: " + GetID());
  }

  return shaderModule;
}

void Shader::CleanUp() {
  vkDestroyShaderModule(m_Context->Device, m_VertexShaderModule, nullptr);
  vkDestroyShaderModule(m_Context->Device, m_FragmentShaderModule, nullptr);
}
} // namespace Inferno
