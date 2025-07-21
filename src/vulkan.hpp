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

struct Surface : raii::UniqueHandle<Surface, VkSurfaceKHR> {
  Surface(VkInstance instance, GLFWwindow* window)
      : UniqueHandle{[&] {
          VkSurfaceKHR surface{};
          if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
            throw std::runtime_error{"failed to create window surface"};
          }
          return surface;
        }()},
        instance_{instance} {}

 private:
  friend UniqueHandle<Surface, VkSurfaceKHR>;
  void destroy(VkSurfaceKHR surface) {
    vkDestroySurfaceKHR(instance_, surface, nullptr);
  }

  VkInstance instance_;
};

struct Swapchain : raii::UniqueHandle<Swapchain, VkSwapchainKHR> {
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
          auto queueFamilies =
              optalg::vector(std::set{device.graphicsQueue().familyIndex, device.presentQueue().familyIndex});

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
      : UniqueHandle{swapchain}, device_{device}, images_{std::move(images)}, format_{format}, extent_{extent} {}

  friend UniqueHandle<Swapchain, VkSwapchainKHR>;
  void destroy(VkSwapchainKHR swapchain) {
    vkDestroySwapchainKHR(device_, swapchain, nullptr);
  }

  VkDevice device_;
  std::vector<VkImage> images_;
  VkFormat format_;
  VkExtent2D extent_;
};

struct ImageView : raii::UniqueHandle<ImageView, VkImageView> {
  ImageView(VkDevice device, VkImage image, VkFormat format)
      : ImageView{[&] {
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
          return ImageView{device, imageView};
        }()} {}

 private:
  ImageView(VkDevice device, VkImageView imageView) : UniqueHandle{imageView}, device_{device} {}

  friend UniqueHandle<ImageView, VkImageView>;
  void destroy(VkImageView imageView) {
    vkDestroyImageView(device_, imageView, nullptr);
  }

  VkDevice device_;
};

struct ShaderModule : raii::UniqueHandle<ShaderModule, VkShaderModule> {
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
      : ShaderModule{[&] {
          VkShaderModuleCreateInfo createInfo{
              .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
              .codeSize = static_cast<uint32_t>(code.size()),
              .pCode = reinterpret_cast<uint32_t const*>(code.data()),
          };

          VkShaderModule shaderModule;
          if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
            throw std::runtime_error{"failed to create shader module"};
          }
          return ShaderModule{device, shaderModule};
        }()} {}

 private:
  ShaderModule(VkDevice device, VkShaderModule shaderModule) : UniqueHandle{shaderModule}, device_{device} {}

  friend UniqueHandle<ShaderModule, VkShaderModule>;
  void destroy(VkShaderModule shaderModule) {
    vkDestroyShaderModule(device_, shaderModule, nullptr);
  }

  VkDevice device_;
};

struct Pipeline : raii::UniqueHandle<Pipeline, VkPipeline> {
  Pipeline(VkDevice device, ShaderModule const& vertexShader, ShaderModule const& fragmentShader)
      : Pipeline{[&] {
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

          std::vector<VkGraphicsPipelineCreateInfo> createInfos;
          createInfos.emplace_back(VkGraphicsPipelineCreateInfo{
              .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
              .stageCount = static_cast<uint32_t>(stages.size()),
              .pStages = stages.data(),
          });

          VkPipeline pipeline;
          if (vkCreateGraphicsPipelines(
                  device, {}, static_cast<uint32_t>(createInfos.size()), createInfos.data(), nullptr, &pipeline) !=
              VK_SUCCESS) {
            throw std::runtime_error{"failed to create graphics pipelines"};
          }
          return Pipeline{device, pipeline};
        }()} {}

 private:
  Pipeline(VkDevice device, VkPipeline pipeline) : UniqueHandle{pipeline}, device_{device} {}

  friend UniqueHandle<Pipeline, VkPipeline>;
  void destroy(VkPipeline pipeline) {
    vkDestroyPipeline(device_, pipeline, nullptr);
  }

  VkDevice device_;
};

}  // namespace vk