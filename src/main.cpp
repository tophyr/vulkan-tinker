#include <array>
#include <ranges>

#include "glfw.hpp"
#include "vulkan.hpp"

using namespace optalg;

using std::views::transform;

constexpr char const* kName{"Vulkan Tinker"};

void render(
    VkCommandBuffer commandBuffer,
    VkFramebuffer framebuffer,
    VkRenderPass renderPass,
    VkExtent2D extent,
    VkPipeline pipeline) {
  VkCommandBufferBeginInfo beginInfo{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
  };
  if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
    throw std::runtime_error{"failed to begin command buffer"};
  }

  std::array clearValues{VkClearValue{{0, 0, 0, 1}}};
  VkRenderPassBeginInfo renderPassInfo{
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      .renderPass = renderPass,
      .framebuffer = framebuffer,
      .renderArea =
          {
              .offset = {0, 0},
              .extent = extent,
          },
      .clearValueCount = static_cast<uint32_t>(clearValues.size()),
      .pClearValues = clearValues.data(),
  };
  vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

  vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

  std::array viewports{VkViewport{
      .x = 0,
      .y = 0,
      .width = static_cast<float>(extent.width),
      .height = static_cast<float>(extent.height),
      .minDepth = 0,
      .maxDepth = 1,
  }};
  vkCmdSetViewport(commandBuffer, 0, static_cast<uint32_t>(viewports.size()), viewports.data());

  std::array scissors{VkRect2D{
      .extent = extent,
  }};
  vkCmdSetScissor(commandBuffer, 0, static_cast<uint32_t>(scissors.size()), scissors.data());

  vkCmdDraw(commandBuffer, 3, 1, 0, 0);

  vkCmdEndRenderPass(commandBuffer);

  if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
    throw std::runtime_error{"failed to record command buffer"};
  }
}

int main() {
  glfw::GlobalState glfwState;
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  {
    glfw::Window window{1920, 1080, kName};
    vk::Instance instance{kName, std::array{"VK_LAYER_KHRONOS_validation"}};
    vk::Surface surface{instance, window};
    vk::Device device{instance, surface, std::array{VK_KHR_SWAPCHAIN_EXTENSION_NAME}};
    vk::Swapchain swapchain{window, device, surface};
    auto imageViews = swapchain.images() |
                      transform([&](auto const& img) { return vk::ImageView{device, img, swapchain.format()}; }) |
                      to<std::vector>();
    vk::PipelineLayout layout{device};
    vk::RenderPass renderPass{device, swapchain.format()};
    vk::Pipeline pipeline{
        device,
        vk::ShaderModule{device, "main.vert.spv"},
        vk::ShaderModule{device, "main.frag.spv"},
        layout,
        renderPass};
    auto framebuffers =
        imageViews | transform([&](auto const& iv) {
          return vk::Framebuffer{device, std::array{static_cast<VkImageView>(iv)}, renderPass, swapchain.extent()};
        }) |
        to<std::vector>();
    vk::CommandPool commandPool{device, device.graphicsQueue().familyIndex};
    auto commandBuffers = commandPool.allocateBuffers(1);

    vk::Semaphore imageAvailable{device};
    auto renderFinished =
        imageViews | transform([&](auto const&) { return vk::Semaphore{device}; }) | to<std::vector>();
    vk::Fence inflight{device, VK_FENCE_CREATE_SIGNALED_BIT};

    while (!glfwWindowShouldClose(window)) {
      glfwPollEvents();

      inflight.wait();
      inflight.reset();

      auto imgIdx = vk::acquireNextImageKHR(device, swapchain, imageAvailable);

      vkResetCommandBuffer(commandBuffers.front(), {});
      render(commandBuffers.front(), framebuffers[imgIdx], renderPass, swapchain.extent(), pipeline);

      std::array waitStages{VkPipelineStageFlags{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT}};
      std::array submitInfos{VkSubmitInfo{
          .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
          .waitSemaphoreCount = 1,
          .pWaitSemaphores = imageAvailable.ptr(),
          .pWaitDstStageMask = waitStages.data(),
          .commandBufferCount = static_cast<uint32_t>(commandBuffers.size()),
          .pCommandBuffers = commandBuffers.data(),
          .signalSemaphoreCount = 1,
          .pSignalSemaphores = renderFinished[imgIdx].ptr(),
      }};
      if (vkQueueSubmit(
              device.graphicsQueue().queue, static_cast<uint32_t>(submitInfos.size()), submitInfos.data(), inflight) !=
          VK_SUCCESS) {
        throw std::runtime_error{"failed to submit queue"};
      }

      std::array swapchains{static_cast<VkSwapchainKHR>(swapchain)};
      VkPresentInfoKHR presentInfo{
          .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
          .waitSemaphoreCount = 1,
          .pWaitSemaphores = renderFinished[imgIdx].ptr(),
          .swapchainCount = static_cast<uint32_t>(swapchains.size()),
          .pSwapchains = swapchains.data(),
          .pImageIndices = &imgIdx,
      };
      vkQueuePresentKHR(device.presentQueue().queue, &presentInfo);
    }

    vkDeviceWaitIdle(device);
  }

  return 0;
}
