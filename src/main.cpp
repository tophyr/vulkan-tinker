#include <array>

#include "glfw.hpp"
#include "vulkan.hpp"

using namespace std::literals;

constexpr char const* kName{"Vulkan Tinker"};
constexpr std::array kInstanceLayers{"VK_LAYER_KHRONOS_validation"};

int main() {
  glfw::GlobalState glfwState;
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  {
    glfw::Window window{1920, 1080, kName};
    vk::Instance instance{kName, kInstanceLayers};
    vk::Surface surface{instance, window};
    vk::Device device{instance, surface};
    auto gfxQ = device.graphicsQueue();
    auto presQ = device.presentQueue();

    while (!glfwWindowShouldClose(window)) {
      glfwPollEvents();
    }
  }

  return 0;
}