#include <memory>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace glfw {

    struct Window {
        Window(int width, int height, char const* title)
            : window_{ glfwCreateWindow(width, height, title, nullptr, nullptr), glfwDestroyWindow } {
        }

        operator GLFWwindow* () {
            return window_.get();
        }

    private:
        std::unique_ptr<GLFWwindow, void(*)(GLFWwindow*)> window_;
    };

}