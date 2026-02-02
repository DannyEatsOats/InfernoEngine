#include "RenderingContext.h"
#include "Inferno/Renderer/Shader.h"
#include "pch.h"

#include "GLFW/glfw3.h"
#include "Inferno/Log.h"
#include <algorithm>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>
#include <vulkan/vulkan_core.h>

const std::vector<const char *> validationLayers = {
    "VK_LAYER_KHRONOS_validation"};

const std::vector<const char *> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME};

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

bool checkValidationlayerSupport() {
  uint32_t layerCount = 0;
  vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

  std::vector<VkLayerProperties> availableLayers(layerCount);
  vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

  for (const auto &layerName : validationLayers) {
    bool layerFound = false;

    for (const auto &layerProperties : availableLayers) {
      if (strcmp(layerName, layerProperties.layerName) == 0) {
        layerFound = true;
        break;
      }
    }

    if (!layerFound) {
      return false;
    }
  }

  return true;
}

namespace Inferno {
RenderingContext::RenderingContext(GLFWwindow *window) : m_Window(window) {}

RenderingContext::~RenderingContext() { Delete(); }

void RenderingContext::Init() {
  CreateInstance();
  CreateSurface();
  PickPhysicalDevice();
  CreateLogicalDevice();
  CreateSwapChain();
  CreateImageViews();
  CreateRenderPass();
  CreateGraphicsPipeline();
  CreateFramebuffers();
  CreateCommandPool();
  CreateCommandBuffer();
  CreateSyncObjects();

  INFERNO_LOG_INFO("Vulkan Initialized");
}

void RenderingContext::Delete() {
  if (m_Device != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(m_Device);
  }

  // Temp
  vkDestroySemaphore(m_Device, m_ImageAvailableSemaphore, nullptr);
  vkDestroySemaphore(m_Device, m_RenderFinishedSemaphore, nullptr);
  vkDestroyFence(m_Device, m_InFlightFence, nullptr);
  vkDestroyCommandPool(m_Device, m_CommandPool, nullptr);

  for (auto framebuffer : m_SwapChainFrameBuffers) {
    vkDestroyFramebuffer(m_Device, framebuffer, nullptr);
  }

  vkDestroyPipeline(m_Device, m_GraphicsPipeline, nullptr);
  vkDestroyPipelineLayout(m_Device, m_PipelineLayout, nullptr);
  vkDestroyRenderPass(m_Device, m_RenderPass, nullptr);

  for (auto imageView : m_SwapChainImageViews) {
    vkDestroyImageView(m_Device, imageView, nullptr);
  }

  CleanUpSwapChain();

  if (m_Device)
    vkDestroyDevice(m_Device, nullptr);

  if (m_Surface)
    vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);

  if (m_Instance)
    vkDestroyInstance(m_Instance, nullptr);

  m_Device = VK_NULL_HANDLE;
  m_Surface = VK_NULL_HANDLE;
  m_Instance = VK_NULL_HANDLE;
}

void RenderingContext::DrawFrame() {
  vkWaitForFences(m_Device, 1, &m_InFlightFence, VK_TRUE, UINT64_MAX);
  vkResetFences(m_Device, 1, &m_InFlightFence);

  uint32_t imageIndex;
  vkAcquireNextImageKHR(m_Device, m_Swapchain, UINT64_MAX,
                        m_ImageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

  // maybe this could be moved to record command buffer
  vkResetCommandBuffer(m_CommandBuffer, 0);
  RecordCommandBuffer(m_CommandBuffer, imageIndex);

  VkSemaphore waitSemaphores[] = {m_ImageAvailableSemaphore};
  VkPipelineStageFlags waitStages[] = {
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

  VkSemaphore signalSemaphores[] = {m_RenderFinishedSemaphore};

  VkSubmitInfo submitInfo = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = waitSemaphores,
      .pWaitDstStageMask = waitStages,
      .commandBufferCount = 1,
      .pCommandBuffers = &m_CommandBuffer,
      .signalSemaphoreCount = 1,
      .pSignalSemaphores = signalSemaphores,
  };

  if (vkQueueSubmit(m_GrapicsQueue, 1, &submitInfo, m_InFlightFence) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed to Submit Draw Command Buffer");
  }

  VkSwapchainKHR swapChains[] = {m_Swapchain};

  VkPresentInfoKHR presentInfo = {
      .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = signalSemaphores,
      .swapchainCount = 1,
      .pSwapchains = swapChains,
      .pImageIndices = &imageIndex,
      .pResults = nullptr,
  };

  vkQueuePresentKHR(m_PresentQueue, &presentInfo);
  vkQueueWaitIdle(m_PresentQueue);
}

void RenderingContext::CreateInstance() {
  VkApplicationInfo appInfo = {
      .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .pApplicationName = "Inferno Engine",
      .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
      .pEngineName = "No Engine",
      .engineVersion = VK_MAKE_VERSION(1, 0, 0),
      .apiVersion = VK_API_VERSION_1_4,
  };

  uint32_t glfwExtCount = 0;
  const char **glfwExtensions =
      glfwGetRequiredInstanceExtensions(&glfwExtCount);

  if (enableValidationLayers && !checkValidationlayerSupport()) {
    throw std::runtime_error("Validation layers requested, but not available");
  }

  VkInstanceCreateInfo createInfo = {
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .pApplicationInfo = &appInfo,
      .enabledLayerCount = 0,
      .ppEnabledLayerNames = nullptr,
      .enabledExtensionCount = glfwExtCount,
      .ppEnabledExtensionNames = glfwExtensions,
  };

  if (enableValidationLayers) {
    createInfo.enabledLayerCount =
        static_cast<uint32_t>(validationLayers.size());
    createInfo.ppEnabledLayerNames = validationLayers.data();
  }

  if (vkCreateInstance(&createInfo, nullptr, &m_Instance) != VK_SUCCESS) {
    INFERNO_LOG_ERROR("Failed to create VkInstance");
    throw std::runtime_error("Failed to create VkInstance");
  }
}

void RenderingContext::CreateSurface() {
  if (glfwCreateWindowSurface(m_Instance, m_Window, nullptr, &m_Surface) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed to create Vulkan Surface");
  }
}

void RenderingContext::PickPhysicalDevice() {
  uint32_t deviceCount = 0;
  vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr);
  if (deviceCount == 0) {
    throw std::runtime_error("No Vulkan GPUs found");
  }

  std::vector<VkPhysicalDevice> devices(deviceCount);

  vkEnumeratePhysicalDevices(m_Instance, &deviceCount, devices.data());

  for (const auto &device : devices) {
    if (IsDeviceSuitable(device)) {
      m_PhysicalDevice = device;
      break;
    }
  }

  if (m_PhysicalDevice == VK_NULL_HANDLE) {
    throw std::runtime_error("Failed to find suitable GPU");
  }
}

void RenderingContext::CreateLogicalDevice() {
  QueueFamilyIndices indices = FindQueueFamilies(m_PhysicalDevice);

  std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
  std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(),
                                            indices.presentFamily.value()};

  float queuePriority = 1.0f;
  for (uint32_t queueFamily : uniqueQueueFamilies) {
    VkDeviceQueueCreateInfo queueCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = queueFamily,
        .queueCount = 1,
        .pQueuePriorities = &queuePriority,
    };
    queueCreateInfos.push_back(queueCreateInfo);
  }

  VkPhysicalDeviceFeatures deviceFeatures{};

  VkDeviceCreateInfo createInfo = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
      .pQueueCreateInfos = queueCreateInfos.data(),
      .enabledLayerCount = 0,
      .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
      .ppEnabledExtensionNames = deviceExtensions.data(),
      .pEnabledFeatures = &deviceFeatures,
  };

  if (enableValidationLayers) {
    createInfo.enabledLayerCount =
        static_cast<uint32_t>(validationLayers.size());
    createInfo.ppEnabledLayerNames = validationLayers.data();
  }

  if (vkCreateDevice(m_PhysicalDevice, &createInfo, nullptr, &m_Device) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed to create logical deivce");
  }

  vkGetDeviceQueue(m_Device, indices.graphicsFamily.value(), 0,
                   &m_GrapicsQueue);
  vkGetDeviceQueue(m_Device, indices.presentFamily.value(), 0, &m_PresentQueue);
}

void RenderingContext::CreateSwapChain() {
  SwapChainSupportDetails swapChainDetails =
      QuerySwapChainSupport(m_PhysicalDevice);

  VkSurfaceFormatKHR surfaceFormat =
      ChooseSwapSurfaceFormat(swapChainDetails.formats);
  VkPresentModeKHR presentMode =
      ChooseSwapPresentMode(swapChainDetails.presentModes);
  VkExtent2D extent = ChooseSwapExtent(swapChainDetails.capabilities);

  uint32_t imageCount = swapChainDetails.capabilities.minImageCount + 1;

  if (swapChainDetails.capabilities.maxImageCount > 0 &&
      imageCount > swapChainDetails.capabilities.maxImageCount) {
    imageCount = swapChainDetails.capabilities.maxImageCount;
  }

  VkSwapchainCreateInfoKHR createInfo{
      .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      .surface = m_Surface,
      .minImageCount = imageCount,
      .imageFormat = surfaceFormat.format,
      .imageColorSpace = surfaceFormat.colorSpace,
      .imageExtent = extent,
      .imageArrayLayers = 1,
      .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
  };

  QueueFamilyIndices indices = FindQueueFamilies(m_PhysicalDevice);
  uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(),
                                   indices.presentFamily.value()};

  if (indices.graphicsFamily != indices.presentFamily) {
    createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    createInfo.queueFamilyIndexCount = 2;
    createInfo.pQueueFamilyIndices = queueFamilyIndices;
  } else {
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.queueFamilyIndexCount = 0;
    createInfo.pQueueFamilyIndices = nullptr;
  }

  createInfo.preTransform = swapChainDetails.capabilities.currentTransform;
  createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  createInfo.presentMode = presentMode;
  createInfo.clipped = VK_TRUE;
  createInfo.oldSwapchain = VK_NULL_HANDLE;

  if (vkCreateSwapchainKHR(m_Device, &createInfo, nullptr, &m_Swapchain)) {
    throw std::runtime_error("Failed to Create SwapChain");
  }

  vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &imageCount, nullptr);
  m_SwapChainImages.resize(imageCount);
  vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &imageCount,
                          m_SwapChainImages.data());

  m_SwapChainImageFormat = surfaceFormat.format;
  m_SwapChainExtent = extent;
}

void RenderingContext::CleanUpSwapChain() {
  vkDestroySwapchainKHR(m_Device, m_Swapchain, nullptr);
}

void RenderingContext::CreateImageViews() {
  m_SwapChainImageViews.resize(m_SwapChainImages.size());

  for (size_t i = 0; i < m_SwapChainImages.size(); ++i) {
    VkImageViewCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = m_SwapChainImages[i],
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = m_SwapChainImageFormat,
    };
    createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    createInfo.subresourceRange.baseMipLevel = 0;
    createInfo.subresourceRange.levelCount = 1;
    createInfo.subresourceRange.baseArrayLayer = 0;
    createInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(m_Device, &createInfo, nullptr,
                          &m_SwapChainImageViews[i]) != VK_SUCCESS) {
      throw std::runtime_error("Failed to create image views");
    }
  }
}

void RenderingContext::CreateRenderPass() {
  VkAttachmentDescription colorAttachment = {
      .format = m_SwapChainImageFormat,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
  };

  VkAttachmentReference colorAttachmentRef = {
      .attachment = 0,
      .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
  };

  VkSubpassDescription subpassDesc = {
      .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
      .colorAttachmentCount = 1,
      .pColorAttachments = &colorAttachmentRef,
  };

  VkSubpassDependency dependency = {
      .srcSubpass = VK_SUBPASS_EXTERNAL,
      .dstSubpass = 0,
      .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      .srcAccessMask = 0,
      .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
  };

  VkRenderPassCreateInfo renderPassCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
      .attachmentCount = 1,
      .pAttachments = &colorAttachment,
      .subpassCount = 1,
      .pSubpasses = &subpassDesc,
      .dependencyCount = 1,
      .pDependencies = &dependency,
  };

  if (vkCreateRenderPass(m_Device, &renderPassCreateInfo, nullptr,
                         &m_RenderPass) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create render pass");
  }
}

void RenderingContext::CreateGraphicsPipeline() {
  // PIPELINE SHADER STAGE
  auto shader = Shader::Create("shader", &m_Device, "assets/shaders/vert.spv",
                               "assets/shaders/frag.spv");

  VkPipelineShaderStageCreateInfo vertShaderStageInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_VERTEX_BIT,
      .module = shader->GetVertexShaderModule(),
      .pName = "main",
  };

  VkPipelineShaderStageCreateInfo fragShaderStageInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
      .module = shader->GetFragmentShaderModule(),
      .pName = "main",
  };

  VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo,
                                                    fragShaderStageInfo};

  // PIPELINE VERTEX INPUT STATE
  VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .vertexBindingDescriptionCount = 0,
      .pVertexBindingDescriptions = nullptr,
      .vertexAttributeDescriptionCount = 0,
      .pVertexAttributeDescriptions = nullptr,
  };

  // PIPELINE INPUT ASSEMBLY
  VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      .primitiveRestartEnable = VK_FALSE,
  };

  // PIPELINE VIEWPORT
  VkViewport viewport = {
      .x = 0.0f,
      .y = 0.0f,
      .width = static_cast<float>(m_SwapChainExtent.width),
      .height = static_cast<float>(m_SwapChainExtent.height),
      .minDepth = 0.0f,
      .maxDepth = 1.0f,
  };

  VkRect2D scissor = {
      .offset = {0, 0},
      .extent = m_SwapChainExtent,
  };

  VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .viewportCount = 1,
      .pViewports = &viewport,
      .scissorCount = 1,
      .pScissors = &scissor,
  };

  // Rasterizer
  VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .depthClampEnable = VK_FALSE,
      .rasterizerDiscardEnable = VK_FALSE,
      .polygonMode = VK_POLYGON_MODE_FILL,
      .cullMode = VK_CULL_MODE_BACK_BIT,
      .frontFace = VK_FRONT_FACE_CLOCKWISE,
      .depthBiasEnable = VK_FALSE,
      .depthBiasConstantFactor = 0.0f,
      .depthBiasClamp = 0.0f,
      .depthBiasSlopeFactor = 0.0f,
      .lineWidth = 1.0f,
  };

  // Multisampling
  VkPipelineMultisampleStateCreateInfo mutlisampleStateCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
      .sampleShadingEnable = VK_FALSE,
      .minSampleShading = 1.0f,
      .pSampleMask = nullptr,
      .alphaToCoverageEnable = VK_FALSE,
      .alphaToOneEnable = VK_FALSE,
  };

  // TODO: Depth Buffering

  // Color Blending
  VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {
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

  VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .logicOpEnable = VK_FALSE,
      .logicOp = VK_LOGIC_OP_COPY,
      .attachmentCount = 1,
      .pAttachments = &colorBlendAttachmentState,
  };

  // Pipeline Layout
  VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = 0,
      .pSetLayouts = nullptr,
      .pushConstantRangeCount = 0,
      .pPushConstantRanges = nullptr,
  };

  if (vkCreatePipelineLayout(m_Device, &pipelineLayoutCreateInfo, nullptr,
                             &m_PipelineLayout) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create pipeline layout");
  }

  // Dynamic State
  std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
                                               VK_DYNAMIC_STATE_SCISSOR};

  VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
      .pDynamicStates = dynamicStates.data(),
  };

  VkGraphicsPipelineCreateInfo pipelineCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .stageCount = 2,
      .pStages = shaderStages,
      .pVertexInputState = &vertexInputStateCreateInfo,
      .pInputAssemblyState = &inputAssemblyStateCreateInfo,
      .pViewportState = &viewportStateCreateInfo,
      .pRasterizationState = &rasterizationStateCreateInfo,
      .pMultisampleState = &mutlisampleStateCreateInfo,
      .pDepthStencilState = nullptr,
      .pColorBlendState = &colorBlendStateCreateInfo,
      .pDynamicState = &dynamicStateCreateInfo,
      .layout = m_PipelineLayout,
      .renderPass = m_RenderPass,
      .subpass = 0,
      .basePipelineHandle = VK_NULL_HANDLE,
      .basePipelineIndex = -1,
  };

  if (vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1,
                                &pipelineCreateInfo, nullptr,
                                &m_GraphicsPipeline) != VK_SUCCESS) {
    throw std::runtime_error("Failed to Create Graphics Pipeline");
  }
}

void RenderingContext::CreateFramebuffers() {
  m_SwapChainFrameBuffers.resize(m_SwapChainImageViews.size());

  for (size_t i = 0; i < m_SwapChainImageViews.size(); ++i) {
    VkImageView attachments[] = {m_SwapChainImageViews[i]};

    VkFramebufferCreateInfo frameBufferCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = m_RenderPass,
        .attachmentCount = 1,
        .pAttachments = attachments,
        .width = m_SwapChainExtent.width,
        .height = m_SwapChainExtent.height,
        .layers = 1,
    };

    if (vkCreateFramebuffer(m_Device, &frameBufferCreateInfo, nullptr,
                            &m_SwapChainFrameBuffers[i]) != VK_SUCCESS) {
      throw std::runtime_error("Failed to create Framebuffer!");
    }
  }
}

void RenderingContext::CreateCommandPool() {
  QueueFamilyIndices queueFamilyIndices = FindQueueFamilies(m_PhysicalDevice);

  VkCommandPoolCreateInfo poolCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = queueFamilyIndices.graphicsFamily.value(),
  };

  if (vkCreateCommandPool(m_Device, &poolCreateInfo, nullptr, &m_CommandPool) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed to create command pool!");
  }
}

void RenderingContext::CreateCommandBuffer() {
  VkCommandBufferAllocateInfo commandBufferAllocateInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = m_CommandPool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1,
  };

  if (vkAllocateCommandBuffers(m_Device, &commandBufferAllocateInfo,
                               &m_CommandBuffer) != VK_SUCCESS) {
    throw std::runtime_error("Failed to Allocate Command Buffers");
  }
}

void RenderingContext::CreateSyncObjects() {
  VkSemaphoreCreateInfo semaphoreCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
  };

  VkFenceCreateInfo fenceCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
      .flags = VK_FENCE_CREATE_SIGNALED_BIT,
  };

  if (vkCreateSemaphore(m_Device, &semaphoreCreateInfo, nullptr,
                        &m_ImageAvailableSemaphore) != VK_SUCCESS ||
      vkCreateSemaphore(m_Device, &semaphoreCreateInfo, nullptr,
                        &m_RenderFinishedSemaphore) != VK_SUCCESS ||
      vkCreateFence(m_Device, &fenceCreateInfo, nullptr, &m_InFlightFence) !=
          VK_SUCCESS) {
    throw std::runtime_error("Failed to Create Semaphores");
  }
}

void RenderingContext::RecordCommandBuffer(VkCommandBuffer commandBuffer,
                                           uint32_t imageIndex) {
  VkCommandBufferBeginInfo commandBufferBeginInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = 0,
      .pInheritanceInfo = nullptr,
  };

  if (vkBeginCommandBuffer(m_CommandBuffer, &commandBufferBeginInfo) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed to Begin Recording Command Buffer");
  }

  VkRenderPassBeginInfo renderPassBeginInfo = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      .renderPass = m_RenderPass,
      .framebuffer = m_SwapChainFrameBuffers[imageIndex],
  };

  renderPassBeginInfo.renderArea.offset = {0, 0};
  renderPassBeginInfo.renderArea.extent = m_SwapChainExtent;

  VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
  renderPassBeginInfo.clearValueCount = 1;
  renderPassBeginInfo.pClearValues = &clearColor;

  vkCmdBeginRenderPass(m_CommandBuffer, &renderPassBeginInfo,
                       VK_SUBPASS_CONTENTS_INLINE);
  vkCmdBindPipeline(m_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    m_GraphicsPipeline);

  VkViewport viewport = {
      .x = 0.0f,
      .y = 0.0f,
      .width = static_cast<float>(m_SwapChainExtent.width),
      .height = static_cast<float>(m_SwapChainExtent.height),
      .minDepth = 0.0f,
      .maxDepth = 1.0f,
  };

  vkCmdSetViewport(m_CommandBuffer, 0, 1, &viewport);

  VkRect2D scissor = {
      .offset = {0, 0},
      .extent = m_SwapChainExtent,
  };

  vkCmdSetScissor(m_CommandBuffer, 0, 1, &scissor);

  vkCmdDraw(m_CommandBuffer, 3, 1, 0, 0);
  vkCmdEndRenderPass(m_CommandBuffer);
  vkEndCommandBuffer(m_CommandBuffer);
}

QueueFamilyIndices
RenderingContext::FindQueueFamilies(VkPhysicalDevice device) {
  QueueFamilyIndices indices;

  uint32_t queueFamilyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

  std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount,
                                           queueFamilies.data());

  int i = 0;
  for (const auto &queueFamily : queueFamilies) {
    if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      indices.graphicsFamily = i;
    }

    VkBool32 presentSupported = false;
    vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_Surface,
                                         &presentSupported);

    if (presentSupported) {
      indices.presentFamily = i;
    }

    if (indices.IsComplete()) {
      break;
    }

    i++;
  }

  return indices;
}

SwapChainSupportDetails
RenderingContext::QuerySwapChainSupport(VkPhysicalDevice device) {
  SwapChainSupportDetails details;

  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_Surface,
                                            &details.capabilities);

  uint32_t formatCount = 0;

  vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_Surface, &formatCount,
                                       nullptr);

  if (formatCount != 0) {
    details.formats.resize(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_Surface, &formatCount,
                                         details.formats.data());
  }

  uint32_t presentModeCount = 0;
  vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_Surface,
                                            &presentModeCount, nullptr);

  if (presentModeCount != 0) {
    details.presentModes.resize(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        device, m_Surface, &presentModeCount, details.presentModes.data());
  }

  return details;
}

VkSurfaceFormatKHR RenderingContext::ChooseSwapSurfaceFormat(
    const std::vector<VkSurfaceFormatKHR> &availableFormats) {

  for (const auto &availableFormat : availableFormats) {
    if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
        availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      return availableFormat;
    }
  }
  return availableFormats[0];
}

VkPresentModeKHR RenderingContext::ChooseSwapPresentMode(
    const std::vector<VkPresentModeKHR> &availablePresentationModes) {

  for (const auto &availablePresentMode : availablePresentationModes) {
    if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
      return availablePresentMode;
    }
  }

  return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D RenderingContext::ChooseSwapExtent(
    const VkSurfaceCapabilitiesKHR &capabilities) {
  if (capabilities.currentExtent.width !=
      std::numeric_limits<uint32_t>::max()) {
    return capabilities.currentExtent;
  } else {
    int width, height;
    glfwGetFramebufferSize(m_Window, &width, &height);

    VkExtent2D actualExtent = {
        .width = static_cast<uint32_t>(width),
        .height = static_cast<uint32_t>(height),
    };

    actualExtent.width =
        std::clamp(actualExtent.width, capabilities.minImageExtent.width,
                   capabilities.maxImageExtent.width);
    actualExtent.height =
        std::clamp(actualExtent.height, capabilities.minImageExtent.height,
                   capabilities.maxImageExtent.height);

    return actualExtent;
  }
}

bool RenderingContext::IsDeviceSuitable(VkPhysicalDevice device) {
  QueueFamilyIndices indices = FindQueueFamilies(device);
  bool extensionsSupported = CheckDeviceExtensionSupport(device);

  bool swapChainAdequate = false;
  if (extensionsSupported) {
    SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(device);
    swapChainAdequate = !swapChainSupport.formats.empty() &&
                        !swapChainSupport.presentModes.empty();
  }

  return indices.IsComplete() && extensionsSupported && swapChainAdequate;
}

bool RenderingContext::CheckDeviceExtensionSupport(VkPhysicalDevice device) {
  uint32_t extensionCount;
  vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount,
                                       nullptr);

  std::vector<VkExtensionProperties> availableExtensions(extensionCount);
  vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount,
                                       availableExtensions.data());

  std::set<std::string> requiredExtensions(deviceExtensions.begin(),
                                           deviceExtensions.end());

  for (const auto &extension : availableExtensions) {
    requiredExtensions.erase(extension.extensionName);
  }

  return requiredExtensions.empty();
}

} // namespace Inferno
