#pragma once

#include <algorithm>
#include <stdexcept>
#include <span>
#include <string_view>
#include <vector>

#include <vulkan/vulkan.h>

#include "glfw.hpp"
#include "raii.hpp"

namespace vk {

inline auto enumerateInstanceLayerProperties() {
  return raii::SizedVecFetcher<VkLayerProperties, vkEnumerateInstanceLayerProperties>();
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

}  // namespace vk