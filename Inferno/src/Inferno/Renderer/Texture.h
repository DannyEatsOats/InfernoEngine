#pragma once

#include <cstdint>
#include <vulkan/vulkan_core.h>

#include "Inferno/Renderer/DeviceContext.h"
#include "Inferno/Renderer/Image.h"
#include "Inferno/Resource/Resource.h"

namespace Inferno {
class Texture : public Resource {
public:
  explicit Texture(const std::string &id, const DeviceContext *context)
      : Resource(id), m_Context(context) {}
  ~Texture() { DoUnLoad(); };

  Texture(Texture &) = delete;
  Texture &operator=(Texture &) = delete;

  Texture(Texture &&other);
  Texture &operator=(Texture &&other);

  VkImage GetImage() const { return m_Image.GetImage(); }
  VkImageView GetImageView() const { return m_Image.GetView(); }
  VkSampler GetSampler() const { return m_Sampler; }

protected:
  bool DoLoad() override;
  bool DoUnLoad() override;

private:
  void CleanUp();

  void LoadFromKTX2(const std::string &filePath);
  void LoadFromFile(const std::string &filePath);
  void FreeImage(unsigned char *data);
  void CreateVulkanImage(unsigned char *data, uint32_t width, uint32_t height,
                         uint32_t channels);

private:
  const DeviceContext *m_Context = nullptr;

  Image m_Image;
  VkSampler m_Sampler = VK_NULL_HANDLE;
};
} // namespace Inferno
