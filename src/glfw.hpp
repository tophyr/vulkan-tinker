#pragma once

#include <span>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "raii.hpp"

namespace glfw {

inline std::span<char const*> getRequiredInstanceExtensions() {
  uint32_t count{};
  char const** exts = glfwGetRequiredInstanceExtensions(&count);
  return {exts, count};
}

struct GlobalState {
  GlobalState() {
    glfwInit();
  }

  ~GlobalState() noexcept {
    glfwTerminate();
  }
};

using Window = raii::MoveOnlyHolder<
    GLFWwindow*,
    [](int width, int height, char const *title) { return glfwCreateWindow(width, height, title, nullptr, nullptr); },
    glfwDestroyWindow>;

}  // namespace glfw