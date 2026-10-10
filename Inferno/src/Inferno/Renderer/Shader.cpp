#include <filesystem>
#include <fstream>
#include <pch.h>
#include <stdexcept>
#include <vector>
#include <volk/volk.h>
#include <vulkan/vulkan_core.h>

#include "Inferno/Resource/Resource.h"
#include "Shader.h"

namespace Inferno {
std::vector<uint32_t> ReadFile(const std::string &filePath) {
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

VkShaderModule CreateShaderModule(const DeviceContext *context,
                                  const std::vector<uint32_t> &code,
                                  const std::string &id) {
  VkShaderModuleCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  createInfo.codeSize = code.size();
  createInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());

  VkShaderModule shaderModule;
  if (vkCreateShaderModule(context->Device, &createInfo, nullptr,
                           &shaderModule) != VK_SUCCESS) {
    throw std::runtime_error("Failed to Create Shader Module: " + id);
  }

  return shaderModule;
}

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

  m_VertexShaderModule =
      CreateShaderModule(m_Context, vertexShaderCode, GetID());
  m_FragmentShaderModule =
      CreateShaderModule(m_Context, fragmentShaderCode, GetID());

  return true;
}

bool Shader::DoUnLoad() {
  if (IsLoaded()) {
    CleanUp();
  }

  return true;
}

void Shader::CleanUp() {
  if (m_VertexShaderModule != VK_NULL_HANDLE) {
    vkDestroyShaderModule(m_Context->Device, m_VertexShaderModule, nullptr);
    m_VertexShaderModule = VK_NULL_HANDLE;
  }
  if (m_FragmentShaderModule != VK_NULL_HANDLE) {
    vkDestroyShaderModule(m_Context->Device, m_FragmentShaderModule, nullptr);
    m_FragmentShaderModule = VK_NULL_HANDLE;
  }
}

// ==================================================================
// Compute Shader
// ==================================================================

ComputeShader::ComputeShader(ComputeShader &&other)
    : Resource(std::move(other)), m_Context(other.m_Context),
      m_ComputeShaderModule(other.m_ComputeShaderModule) {
  other.m_Context = nullptr;
  other.m_ComputeShaderModule = VK_NULL_HANDLE;
}

ComputeShader &ComputeShader::operator=(ComputeShader &&other) {
  if (this == &other) {
    return *this;
  }

  CleanUp();

  Resource::operator=(std::move(other));

  m_Context = other.m_Context;
  m_ComputeShaderModule = other.m_ComputeShaderModule;

  other.m_Context = nullptr;
  other.m_ComputeShaderModule = VK_NULL_HANDLE;

  return *this;
}

bool ComputeShader::DoLoad() {
  std::string extension;

  std::string computeFilePath = "assets/shaders/" + GetID() + ".comp" + ".spv";

  auto computeShaderCode = ReadFile(computeFilePath);

  m_ComputeShaderModule =
      CreateShaderModule(m_Context, computeShaderCode, GetID());

  return true;
}

bool ComputeShader::DoUnLoad() {
  if (IsLoaded()) {
    CleanUp();
  }

  return true;
}

void ComputeShader::CleanUp() {
  if (m_ComputeShaderModule != VK_NULL_HANDLE) {
    vkDestroyShaderModule(m_Context->Device, m_ComputeShaderModule, nullptr);
    m_ComputeShaderModule = VK_NULL_HANDLE;
  }
}
} // namespace Inferno
