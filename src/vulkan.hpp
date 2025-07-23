#pragma once

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <span>
#include <stdexcept>
#include <string_view>
#include <vector>

#include <vulkan/vulkan.h>

#include "glfw.hpp"
#include "raii.hpp"

namespace vk {

inline auto enumerateInstanceLayerProperties() {
  return raii::VecFetcher<VkLayerProperties, vkEnumerateInstanceLayerProperties>();
}

inline auto enumeratePhysicalDevices(VkInstance instance) {
  return raii::VecFetcher<VkPhysicalDevice, vkEnumeratePhysicalDevices>(instance);
}

inline auto enumerateDeviceExtensionProperties(VkPhysicalDevice device) {
  return raii::VecFetcher<VkExtensionProperties, vkEnumerateDeviceExtensionProperties>(device, nullptr);
}

inline auto getPhysicalDeviceProperties(VkPhysicalDevice device) {
  return raii::Fetcher<VkPhysicalDeviceProperties, vkGetPhysicalDeviceProperties>(device);
}

inline auto getPhysicalDeviceFeatures(VkPhysicalDevice device) {
  return raii::Fetcher<VkPhysicalDeviceFeatures, vkGetPhysicalDeviceFeatures>(device);
}

inline auto getPhysicalDeviceQueueFamilyProperties(VkPhysicalDevice device) {
  return raii::VecFetcher<VkQueueFamilyProperties, vkGetPhysicalDeviceQueueFamilyProperties>(device);
}

inline auto getPhysicalDeviceSurfaceSupportKHR(
    VkPhysicalDevice device, uint32_t queueFamilyIndex, VkSurfaceKHR surface) {
  return raii::Fetcher<VkBool32, vkGetPhysicalDeviceSurfaceSupportKHR>(device, queueFamilyIndex, surface);
}

inline auto getDeviceQueue(VkDevice device, uint32_t queueIndex) {
  return raii::Fetcher<VkQueue, vkGetDeviceQueue>(device, queueIndex, 0);
}

inline auto getPhysicalDeviceSurfaceCapabilitiesKHR(VkPhysicalDevice device, VkSurfaceKHR surface) {
  return raii::Fetcher<VkSurfaceCapabilitiesKHR, vkGetPhysicalDeviceSurfaceCapabilitiesKHR>(device, surface);
}

inline auto getPhysicalDeviceSurfaceFormatsKHR(VkPhysicalDevice device, VkSurfaceKHR surface) {
  return raii::VecFetcher<VkSurfaceFormatKHR, vkGetPhysicalDeviceSurfaceFormatsKHR>(device, surface);
}

inline auto getPhysicalDeviceSurfacePresentModesKHR(VkPhysicalDevice device, VkSurfaceKHR surface) {
  return raii::VecFetcher<VkPresentModeKHR, vkGetPhysicalDeviceSurfacePresentModesKHR>(device, surface);
}

inline auto getSwapchainImagesKHR(VkDevice device, VkSwapchainKHR swapchain) {
  return raii::VecFetcher<VkImage, vkGetSwapchainImagesKHR>(device, swapchain);
}

struct Instance : raii::UniqueHandle<Instance, VkInstance> {
  Instance(char const* name, std::span<char const* const> requiredLayers = {})
      : UniqueHandle{[&] {
          if (!std::ranges::all_of(
                  requiredLayers, [availableLayers = enumerateInstanceLayerProperties()](auto const& req) {
                    return std::ranges::any_of(availableLayers, [svReq = std::string_view{req}](auto const& layer) {
                      return svReq == layer.layerName;
                    });
                  })) {
            throw std::runtime_error{"not all required layers available"};
          }

          VkApplicationInfo appInfo{
              .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
              .pApplicationName = name,
              .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
              .apiVersion = VK_API_VERSION_1_0,
          };

          auto glfwExtensions = glfw::getRequiredInstanceExtensions();

          VkInstanceCreateInfo createInfo{
              .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
              .pApplicationInfo = &appInfo,
              .enabledLayerCount = static_cast<uint32_t>(requiredLayers.size()),
              .ppEnabledLayerNames = requiredLayers.data(),
              .enabledExtensionCount = static_cast<uint32_t>(glfwExtensions.size()),
              .ppEnabledExtensionNames = glfwExtensions.data(),
          };

          VkInstance instance{};
          if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
            throw std::runtime_error{"failed to create instance"};
          }
          return instance;
        }()} {}

 private:
  friend UniqueHandle<Instance, VkInstance>;
  void destroy(VkInstance instance) {
    vkDestroyInstance(instance, nullptr);
  }
};

struct Queue {
  Queue(VkDevice device, uint32_t familyIndex) : queue{getDeviceQueue(device, familyIndex)}, familyIndex{familyIndex} {}
  VkQueue const queue;
  uint32_t const familyIndex;
};

struct Device : raii::UniqueHandle<Device, VkDevice> {
  Device(VkInstance instance, VkSurfaceKHR surface, std::span<char const* const> requiredExtensions = {})
      : Device{[&] {
          auto const& [physDevice, gfxQueueIdx, presentQueueIdx] = [&] {
            for (auto const& physDevice : enumeratePhysicalDevices(instance)) {
              if (!std::ranges::all_of(
                      requiredExtensions,
                      [availableExtensions = enumerateDeviceExtensionProperties(physDevice)](auto const& req) {
                        return std::ranges::any_of(
                            availableExtensions,
                            [svReq = std::string_view{req}](auto const& ext) { return svReq == ext.extensionName; });
                      })) {
                continue;
              }

              if (getPhysicalDeviceSurfacePresentModesKHR(physDevice, surface).empty() ||
                  getPhysicalDeviceSurfaceFormatsKHR(physDevice, surface).empty()) {
                continue;
              }

              auto queueFamilies = getPhysicalDeviceQueueFamilyProperties(physDevice);
              auto gfxQueue = std::ranges::find_if(
                  queueFamilies, [](auto const& qf) { return qf.queueFlags & VK_QUEUE_GRAPHICS_BIT; });
              if (gfxQueue == queueFamilies.cend()) {
                continue;
              }
              for (uint32_t i{}; i < queueFamilies.size(); i++) {
                if (getPhysicalDeviceSurfaceSupportKHR(physDevice, i, surface)) {
                  return std::tuple{
                      physDevice, static_cast<uint32_t>(std::distance(queueFamilies.begin(), gfxQueue)), i};
                }
              }
            }
            throw std::runtime_error{"no gpu supporting graphics queue"};
          }();

          float const prio{1.0};
          std::set<uint32_t> qIdxs{gfxQueueIdx, presentQueueIdx};
          std::vector<VkDeviceQueueCreateInfo> queueCreateInfos{};
          std::transform(qIdxs.cbegin(), qIdxs.cend(), std::back_inserter(queueCreateInfos), [&](auto idx) {
            return VkDeviceQueueCreateInfo{
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .queueFamilyIndex = idx,
                .queueCount = 1,
                .pQueuePriorities = &prio,
            };
          });

          VkPhysicalDeviceFeatures deviceFeatures{};
          VkDeviceCreateInfo createInfo{
              .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
              .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
              .pQueueCreateInfos = queueCreateInfos.data(),
              .enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size()),
              .ppEnabledExtensionNames = requiredExtensions.data(),
              .pEnabledFeatures = &deviceFeatures,
          };

          VkDevice device{};
          if (vkCreateDevice(physDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
            throw std::runtime_error{"failed to create logical device"};
          }

          return Device{device, physDevice, Queue{device, gfxQueueIdx}, Queue{device, presentQueueIdx}};
        }()} {}

  VkPhysicalDevice physicalDevice() const {
    return physDevice_;
  }

  Queue graphicsQueue() const {
    return graphicsQueue_;
  }

  Queue presentQueue() const {
    return presentQueue_;
  }

 private:
  friend UniqueHandle<Device, VkDevice>;
  void destroy(VkDevice device) {
    vkDestroyDevice(device, nullptr);
  }

  explicit Device(VkDevice device, VkPhysicalDevice physDevice, Queue graphicsQueue, Queue presentQueue)
      : UniqueHandle{device}, physDevice_{physDevice}, graphicsQueue_{graphicsQueue}, presentQueue_{presentQueue} {}

  VkPhysicalDevice physDevice_;
  Queue graphicsQueue_;
  Queue presentQueue_;
};

struct Surface : raii::ParentedUniqueHandle<VkSurfaceKHR, vkDestroySurfaceKHR, VkInstance> {
  Surface(VkInstance instance, GLFWwindow* window)
      : ParentedUniqueHandle{[&] {
          VkSurfaceKHR surface{};
          if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
            throw std::runtime_error{"failed to create window surface"};
          }
          return std::tuple{instance, surface, nullptr};
        }()} {}
};

struct Swapchain : raii::ParentedUniqueHandle<VkSwapchainKHR, vkDestroySwapchainKHR, VkDevice> {
  Swapchain(GLFWwindow* window, Device const& device, VkSurfaceKHR surface)
      : Swapchain{[&] {
          auto surfaceFormat = [&] {
            auto formats = getPhysicalDeviceSurfaceFormatsKHR(device.physicalDevice(), surface);
            return optalg::find_if(
                       formats,
                       [](auto const& format) {
                         return format.format == VK_FORMAT_B8G8R8A8_SRGB &&
                                format.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR;
                       })
                .value_or(formats.front());
          }();
          auto caps = getPhysicalDeviceSurfaceCapabilitiesKHR(device.physicalDevice(), surface);
          auto queueFamilies = std::set{device.graphicsQueue().familyIndex, device.presentQueue().familyIndex} |
                               optalg::to<std::vector>();

          VkSwapchainCreateInfoKHR createInfo{
              .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
              .surface = surface,
              .minImageCount = (caps.minImageCount == caps.maxImageCount) ? caps.maxImageCount : caps.minImageCount + 1,
              .imageFormat = surfaceFormat.format,
              .imageColorSpace = surfaceFormat.colorSpace,
              .imageExtent =
                  [&] {
                    if (caps.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
                      return caps.currentExtent;
                    } else {
                      auto const& [w, h] = glfw::getFramebufferSize(window);
                      return VkExtent2D{
                          std::clamp(static_cast<uint32_t>(w), caps.minImageExtent.width, caps.maxImageExtent.width),
                          std::clamp(static_cast<uint32_t>(h), caps.minImageExtent.height, caps.maxImageExtent.height)};
                    }
                  }(),
              .imageArrayLayers = 1,
              .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
              .imageSharingMode = queueFamilies.size() > 1 ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE,
              .queueFamilyIndexCount = queueFamilies.size() > 1 ? static_cast<uint32_t>(queueFamilies.size()) : 0,
              .pQueueFamilyIndices = queueFamilies.size() > 1 ? queueFamilies.data() : nullptr,
              .preTransform = caps.currentTransform,
              .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
              .presentMode = optalg::find_if(
                                 getPhysicalDeviceSurfacePresentModesKHR(device.physicalDevice(), surface),
                                 [](auto const& mode) { return mode == VK_PRESENT_MODE_MAILBOX_KHR; })
                                 .value_or(VK_PRESENT_MODE_FIFO_KHR),
              .clipped = true,
          };

          VkSwapchainKHR swapchain;
          if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain) != VK_SUCCESS) {
            throw std::runtime_error{"failed to create swapchain"};
          }
          return Swapchain{
              device,
              swapchain,
              getSwapchainImagesKHR(device, swapchain),
              createInfo.imageFormat,
              createInfo.imageExtent};
        }()} {}

  std::vector<VkImage> const& images() const {
    return images_;
  }

  VkFormat format() const {
    return format_;
  }

  VkExtent2D extent() const {
    return extent_;
  }

 private:
  Swapchain(VkDevice device, VkSwapchainKHR swapchain, std::vector<VkImage> images, VkFormat format, VkExtent2D extent)
      : ParentedUniqueHandle{device, swapchain, nullptr},
        images_{std::move(images)},
        format_{format},
        extent_{extent} {}

  std::vector<VkImage> images_;
  VkFormat format_;
  VkExtent2D extent_;
};

struct ImageView : raii::ParentedUniqueHandle<VkImageView, vkDestroyImageView, VkDevice> {
  ImageView(VkDevice device, VkImage image, VkFormat format)
      : ParentedUniqueHandle{[&] {
          VkImageViewCreateInfo createInfo{
              .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
              .image = image,
              .viewType = VK_IMAGE_VIEW_TYPE_2D,
              .format = format,
              .subresourceRange =
                  {
                      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                      .baseMipLevel = 0,
                      .levelCount = 1,
                      .baseArrayLayer = 0,
                      .layerCount = 1,
                  },
          };

          VkImageView imageView;
          if (vkCreateImageView(device, &createInfo, nullptr, &imageView) != VK_SUCCESS) {
            throw std::runtime_error{"failed to create image view"};
          }
          return std::tuple{device, imageView, nullptr};
        }()} {}
};

struct ShaderModule : raii::ParentedUniqueHandle<VkShaderModule, vkDestroyShaderModule, VkDevice> {
  ShaderModule(VkDevice device, std::filesystem::path shaderPath)
      : ShaderModule{device, std::span<uint8_t const>{[&] {
                       std::ifstream in{shaderPath, std::ios::ate | std::ios::binary};
                       in.exceptions(std::ios::failbit | std::ios::badbit);
                       std::vector<uint8_t> code(in.tellg());
                       in.seekg(0, std::ios::beg);
                       in.read(reinterpret_cast<char*>(code.data()), code.size());
                       return code;
                     }()}} {}
  ShaderModule(VkDevice device, std::span<uint8_t const> const& code)
      : ParentedUniqueHandle{[&] {
          VkShaderModuleCreateInfo createInfo{
              .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
              .codeSize = static_cast<uint32_t>(code.size()),
              .pCode = reinterpret_cast<uint32_t const*>(code.data()),
          };

          VkShaderModule shaderModule;
          if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
            throw std::runtime_error{"failed to create shader module"};
          }
          return std::tuple{device, shaderModule, nullptr};
        }()} {}
};

struct PipelineLayout : raii::ParentedUniqueHandle<VkPipelineLayout, vkDestroyPipelineLayout, VkDevice> {
  explicit PipelineLayout(VkDevice device)
      : ParentedUniqueHandle{[&] {
          VkPipelineLayoutCreateInfo createInfo{
              .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
          };

          VkPipelineLayout layout{};
          if (vkCreatePipelineLayout(device, &createInfo, nullptr, &layout) != VK_SUCCESS) {
            throw std::runtime_error{"failed to create pipeline layout"};
          }
          return std::tuple{device, layout, nullptr};
        }()} {}
};

struct RenderPass : raii::ParentedUniqueHandle<VkRenderPass, vkDestroyRenderPass, VkDevice> {
  explicit RenderPass(VkDevice device, VkFormat swapchainFormat)
      : ParentedUniqueHandle{[&] {
          std::array attachments{VkAttachmentDescription{
              .format = swapchainFormat,
              .samples = VK_SAMPLE_COUNT_1_BIT,
              .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
              .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
              .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
              .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
              .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
              .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
          }};
          std::array attachRefs{VkAttachmentReference{
              .attachment = 0,
              .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
          }};
          std::array subpasses{VkSubpassDescription{
              .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
              .colorAttachmentCount = static_cast<uint32_t>(attachRefs.size()),
              .pColorAttachments = attachRefs.data(),
          }};

          VkRenderPassCreateInfo createInfo{
              .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
              .attachmentCount = static_cast<uint32_t>(attachments.size()),
              .pAttachments = attachments.data(),
              .subpassCount = static_cast<uint32_t>(subpasses.size()),
              .pSubpasses = subpasses.data(),
          };

          VkRenderPass renderPass{};
          if (vkCreateRenderPass(device, &createInfo, nullptr, &renderPass) != VK_SUCCESS) {
            throw std::runtime_error{"failed to create render pass"};
          }
          return std::tuple{device, renderPass, nullptr};
        }()} {}
};

struct Pipeline : raii::ParentedUniqueHandle<VkPipeline, vkDestroyPipeline, VkDevice> {
  Pipeline(
      VkDevice device,
      ShaderModule const& vertexShader,
      ShaderModule const& fragmentShader,
      VkPipelineLayout layout,
      VkRenderPass renderPass)
      : ParentedUniqueHandle{[&] {
          std::vector<VkPipelineShaderStageCreateInfo> stages{
              VkPipelineShaderStageCreateInfo{
                  .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                  .stage = VK_SHADER_STAGE_VERTEX_BIT,
                  .module = vertexShader,
                  .pName = "main",
              },
              VkPipelineShaderStageCreateInfo{
                  .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                  .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                  .module = fragmentShader,
                  .pName = "main",
              },
          };

          VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo{
              .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
              .vertexBindingDescriptionCount = 0,
              .vertexAttributeDescriptionCount = 0,
          };

          VkPipelineInputAssemblyStateCreateInfo inputAssembyStateCreateInfo{
              .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
              .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
              .primitiveRestartEnable = false,
          };

          std::array dynamicStates{VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_VIEWPORT};
          VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo{
              .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
              .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
              .pDynamicStates = dynamicStates.data(),
          };

          VkPipelineViewportStateCreateInfo viewportStateCreateInfo{
              .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
              .viewportCount = 1,
              .scissorCount = 1,
          };

          VkPipelineRasterizationStateCreateInfo rasterizationCreateInfo{
              .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
              .polygonMode = VK_POLYGON_MODE_FILL,
              .cullMode = VK_CULL_MODE_BACK_BIT,
              .frontFace = VK_FRONT_FACE_CLOCKWISE,
              .lineWidth = 1.0f,
          };

          VkPipelineMultisampleStateCreateInfo multisampleCreateInfo{
              .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
              .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
          };

          std::array colorBlendAttachments{VkPipelineColorBlendAttachmentState{}};
          VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo{
              .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
              .attachmentCount = static_cast<uint32_t>(colorBlendAttachments.size()),
              .pAttachments = colorBlendAttachments.data(),
          };

          std::vector<VkGraphicsPipelineCreateInfo> createInfos;
          createInfos.emplace_back(VkGraphicsPipelineCreateInfo{
              .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
              .stageCount = static_cast<uint32_t>(stages.size()),
              .pStages = stages.data(),
              .pVertexInputState = &vertexInputCreateInfo,
              .pInputAssemblyState = &inputAssembyStateCreateInfo,
              .pViewportState = &viewportStateCreateInfo,
              .pRasterizationState = &rasterizationCreateInfo,
              .pMultisampleState = &multisampleCreateInfo,
              .pColorBlendState = &colorBlendStateCreateInfo,
              .pDynamicState = &dynamicStateCreateInfo,
              .layout = layout,
              .renderPass = renderPass,
          });

          VkPipeline pipeline;
          if (vkCreateGraphicsPipelines(
                  device, {}, static_cast<uint32_t>(createInfos.size()), createInfos.data(), nullptr, &pipeline) !=
              VK_SUCCESS) {
            throw std::runtime_error{"failed to create graphics pipelines"};
          }
          return std::tuple{device, pipeline, nullptr};
        }()} {}
};

struct Framebuffer : raii::ParentedUniqueHandle<VkFramebuffer, vkDestroyFramebuffer, VkDevice> {
  Framebuffer(VkDevice device, std::span<VkImageView const> attachments, VkRenderPass renderPass, VkExtent2D extent)
      : ParentedUniqueHandle{[&] {
          VkFramebufferCreateInfo createInfo{
              .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
              .renderPass = renderPass,
              .attachmentCount = static_cast<uint32_t>(attachments.size()),
              .pAttachments = attachments.data(),
              .width = extent.width,
              .height = extent.height,
              .layers = 1,  // determine from the swapchain somehow?
          };

          VkFramebuffer framebuffer{};
          if (vkCreateFramebuffer(device, &createInfo, nullptr, &framebuffer) != VK_SUCCESS) {
            throw std::runtime_error{"failed to create framebuffer"};
          }
          return std::tuple{device, framebuffer, nullptr};
        }()} {}
};

}  // namespace vk