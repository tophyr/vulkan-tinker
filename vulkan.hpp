#pragma once

#include <stdexcept>

#include "glfw.hpp"
#include "raii.hpp"

#include <vulkan/vulkan.h>

namespace vk {

using Instance = raii::Holder<
  VkInstance,
  [](char const* name) {
    VkApplicationInfo appInfo{
      .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .pApplicationName = name,
      .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
      .pEngineName = "None",
      .apiVersion = VK_API_VERSION_1_0,
    };

    VkInstanceCreateInfo createInfo{
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .pApplicationInfo = &appInfo,
      .enabledLayerCount = 0,
    };

    VkInstance instance{};
    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
      throw std::runtime_error{"failed to create instance"};
    }
    return instance;
  },
  [](VkInstance instance) {
    vkDestroyInstance(instance, nullptr);
  }> ;

}