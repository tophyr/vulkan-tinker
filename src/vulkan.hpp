#pragma once

#include <algorithm>
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

using Instance = raii::MoveOnlyHolder<
    VkInstance,
    [](char const* name, std::span<char const* const> requiredLayers = {}) {
      auto availableLayers = enumerateInstanceLayerProperties();
      for (auto const& layerName : requiredLayers) {
        std::string_view svLayerName{layerName};
        if (!std::ranges::any_of(availableLayers, [&](auto const& layer) { return svLayerName == layer.layerName; })) {
          throw std::runtime_error{std::string{layerName} + " not available"};
        }
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
    },
    [](VkInstance instance) { vkDestroyInstance(instance, nullptr); }>;

struct Device {
  Device(VkInstance instance, VkSurfaceKHR surface) : Device{init(instance, surface)} {}

  operator VkDevice() const {
    return static_cast<VkDevice>(device_);
  }

  VkQueue graphicsQueue() const {
    return graphicsQueue_;
  }

  VkQueue presentQueue() const {
    return presentQueue_;
  }

 private:
  std::tuple<VkDevice, VkQueue, VkQueue> init(VkInstance instance, VkSurfaceKHR surface) {
    auto const& [physDevice, gfxQueueIdx, presentQueueIdx] = [&] {
      for (auto const& physDevice : enumeratePhysicalDevices(instance)) {
        auto queueFamilies = getPhysicalDeviceQueueFamilyProperties(physDevice);
        auto gfxQueue =
            std::ranges::find_if(queueFamilies, [](auto const& qf) { return qf.queueFlags & VK_QUEUE_GRAPHICS_BIT; });
        if (gfxQueue == queueFamilies.cend()) {
          continue;
        }
        for (uint32_t i{}; i < queueFamilies.size(); i++) {
          if (getPhysicalDeviceSurfaceSupportKHR(physDevice, i, surface)) {
            return std::tuple{physDevice, static_cast<uint32_t>(std::distance(queueFamilies.begin(), gfxQueue)), i};
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
        .pEnabledFeatures = &deviceFeatures,
    };

    VkDevice device{};
    if (vkCreateDevice(physDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
      throw std::runtime_error{"failed to create logical device"};
    }

    return {device, getDeviceQueue(device, gfxQueueIdx), getDeviceQueue(device, presentQueueIdx)};
  }

  Device(std::tuple<VkDevice, VkQueue, VkQueue> dat)
      : device_{std::get<0>(dat)}, graphicsQueue_{std::get<1>(dat)}, presentQueue_{std::get<2>(dat)} {}

  raii::MoveOnlyHolder<VkDevice, std::identity{}, [](VkDevice device) { vkDestroyDevice(device, nullptr); }> device_;
  VkQueue graphicsQueue_;
  VkQueue presentQueue_;
};

struct Surface final {
  Surface(VkInstance instance, GLFWwindow* window)
      : instance_{instance}, surface_{[&] {
          VkSurfaceKHR surface{};
          if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
            throw std::runtime_error{"failed to create window surface"};
          }
          return surface;
        }()} {}

  Surface(Surface const&) = delete;
  Surface& operator=(Surface const&) = delete;

  Surface(Surface&& other) noexcept
      : instance_{std::exchange(other.instance_, {})}, surface_{std::exchange(other.surface_, {})} {}
  Surface& operator=(Surface&& other) noexcept {
    if (this != &other) {
      std::destroy_at(this);
      std::construct_at(this, std::move(other));
    }
    return *this;
  }

  operator VkSurfaceKHR() const noexcept {
    return surface_;
  }

  ~Surface() noexcept {
    vkDestroySurfaceKHR(instance_, surface_, nullptr);
  }

 private:
  VkInstance instance_{};
  VkSurfaceKHR surface_{};
};

}  // namespace vk