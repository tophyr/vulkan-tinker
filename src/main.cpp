#include <array>

#include "glfw.hpp"
#include "vulkan.hpp"

constexpr char const* kName{"Vulkan Tinker"};

int main() {
  glfw::GlobalState glfwState;
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  {
    glfw::Window window{1920, 1080, kName};
    vk::Instance instance {kName, std::array{"VK_LAYER_KHRONOS_validation"}};
    vk::Surface surface{instance, window};
    vk::Device device{instance, surface, std::array{VK_KHR_SWAPCHAIN_EXTENSION_NAME}};
    vk::Swapchain swapchain{window, device, surface};


    while (!glfwWindowShouldClose(window)) {
      glfwPollEvents();
    }
  }

  return 0;
}
