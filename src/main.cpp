#include "glfw.hpp"

int main() {
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  {
    glfw::Window window{1920, 1080, "Vulkan Tinker"};

    while (!glfwWindowShouldClose(window)) {
      glfwPollEvents();
    }
  }

  glfwTerminate();

  return 0;
}