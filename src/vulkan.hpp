#pragma once

#include <algorithm>
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

using Instance = raii::MoveOnlyHolder<VkInstance,
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

      VkInstanceCreateInfo createInfo{
          .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
          .pApplicationInfo = &appInfo,
          .enabledLayerCount = static_cast<uint32_t>(requiredLayers.size()),
          .ppEnabledLayerNames = requiredLayers.data(),
      };

      VkInstance instance{};
      if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
        throw std::runtime_error{"failed to create instance"};
      }
      return instance;
    },
    [](VkInstance instance) { vkDestroyInstance(instance, nullptr); }>;

struct Device {
  Device(VkInstance instance) : Device{init(instance)} {}

  operator VkDevice() const {
    return static_cast<VkDevice>(device_);
  }

  VkQueue graphicsQueue() const {
    return graphicsQueue_;
  }

 private:
  std::pair<VkDevice, VkQueue> init(VkInstance instance) {
    auto const& [physDevice, queueIndex] = [&] {
      for (auto const& physDevice : enumeratePhysicalDevices(instance)) {
        auto const& queueFamilies = getPhysicalDeviceQueueFamilyProperties(physDevice);
        for (uint32_t i = 0; i < queueFamilies.size(); i++) {
          if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            return std::pair{physDevice, i};
          }
        }
      }
      throw std::runtime_error{"no gpu supporting graphics queue"};
    }();

    float prio{1.0};
    VkDeviceQueueCreateInfo queueCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = queueIndex,
        .queueCount = 1,
        .pQueuePriorities = &prio,
    };

    VkPhysicalDeviceFeatures deviceFeatures{};
    VkDeviceCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queueCreateInfo,
        .pEnabledFeatures = &deviceFeatures,
    };

    VkDevice device{};
    if (vkCreateDevice(physDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
      throw std::runtime_error{"failed to create logical device"};
    }

    VkQueue graphicsQueue{};
    vkGetDeviceQueue(device, queueIndex, 0, &graphicsQueue);
    return {device, graphicsQueue};
  }

  Device(std::pair<VkDevice, VkQueue> dat) : device_{dat.first}, graphicsQueue_{dat.second} {}

  raii::MoveOnlyHolder<VkDevice, std::identity{}, [](VkDevice device) { vkDestroyDevice(device, nullptr); }> device_;
  VkQueue graphicsQueue_;
};

}  // namespace vk