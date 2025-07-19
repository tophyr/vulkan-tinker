#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "raii.hpp"

namespace glfw {

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