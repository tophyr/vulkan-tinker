#include <array>
#include <ranges>

#include "glfw.hpp"
#include "vulkan.hpp"

constexpr char const* kName{"Vulkan Tinker"};

int main() {
  glfw::GlobalState glfwState;
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  {
    glfw::Window window{1920, 1080, kName};
    vk::Instance instance{kName, std::array{"VK_LAYER_KHRONOS_validation"}};
    vk::Surface surface{instance, window};
    vk::Device device{instance, surface, std::array{VK_KHR_SWAPCHAIN_EXTENSION_NAME}};
    vk::Swapchain swapchain{window, device, surface};
    auto imageViews =
        swapchain.images() |
        std::views::transform([&](auto const& img) { return vk::ImageView{device, img, swapchain.format()}; }) |
        optalg::to<std::vector>();
    vk::PipelineLayout layout{device};
    vk::RenderPass renderPass{device, swapchain.format()};
    vk::Pipeline pipeline{
        device,
        vk::ShaderModule{device, "main.vert.spv"},
        vk::ShaderModule{device, "main.frag.spv"},
        layout,
        renderPass};
    auto framebuffers =
        imageViews | std::views::transform([&](auto const& iv) {
          return vk::Framebuffer{device, std::array{static_cast<VkImageView>(iv)}, renderPass, swapchain.extent()};
        }) |
        optalg::to<std::vector>();

    while (!glfwWindowShouldClose(window)) {
      glfwPollEvents();
    }
  }

  return 0;
}
