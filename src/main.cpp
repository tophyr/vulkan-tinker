#include <array>
#include <ranges>

#include "glfw.hpp"
#include "vulkan.hpp"

using namespace optalg;

using std::views::transform;

constexpr char const* kName{"Vulkan Tinker"};

struct SynchronizedCommandBuffer {
  SynchronizedCommandBuffer(VkDevice device, VkCommandBuffer commandBuffer)
      : commandBuffer{commandBuffer},
        imageAvailable{device},
        renderFinished{device},
        cmdBufferReady{device, VK_FENCE_CREATE_SIGNALED_BIT} {}

  VkCommandBuffer commandBuffer;
  vk::Semaphore imageAvailable;
  vk::Semaphore renderFinished;
  vk::Fence cmdBufferReady;
};

using FrameIndex = uint_fast8_t;
struct RenderInfo {
  RenderInfo(
      GLFWwindow* window,
      vk::Device const& device,
      VkSurfaceKHR surface,
      vk::ShaderModule const& vertexShader,
      vk::ShaderModule const& fragmentShader,
      VkPipelineLayout layout,
      VkSwapchainKHR oldSwapchain = {})
      : swapchain{window, device, surface, oldSwapchain},
        imageViews{
            swapchain.images() |
            transform([&](auto const& img) { return vk::ImageView{device, img, swapchain.format()}; }) |
            to<std::vector>()},
        renderPass{device, swapchain.format()},
        pipeline{device, vertexShader, fragmentShader, layout, renderPass},
        framebuffers{
            imageViews | transform([&](auto const& iv) {
              return vk::Framebuffer{device, std::array{static_cast<VkImageView>(iv)}, renderPass, swapchain.extent()};
            }) |
            to<std::vector>()} {}

  vk::Swapchain swapchain;
  std::vector<vk::ImageView> imageViews;
  vk::RenderPass renderPass;
  vk::Pipeline pipeline;
  std::vector<vk::Framebuffer> framebuffers;
};

void render(VkCommandBuffer commandBuffer, RenderInfo const& renderInfo, FrameIndex idx) {
  vkResetCommandBuffer(commandBuffer, {});

  VkCommandBufferBeginInfo beginInfo{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
  };
  if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
    throw std::runtime_error{"failed to begin command buffer"};
  }

  std::array clearValues{VkClearValue{{0, 0, 0, 1}}};
  VkRenderPassBeginInfo renderPassInfo{
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      .renderPass = renderInfo.renderPass,
      .framebuffer = renderInfo.framebuffers[idx],
      .renderArea =
          {
              .offset = {0, 0},
              .extent = renderInfo.swapchain.extent(),
          },
      .clearValueCount = static_cast<uint32_t>(clearValues.size()),
      .pClearValues = clearValues.data(),
  };
  vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

  vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, renderInfo.pipeline);

  std::array viewports{VkViewport{
      .x = 0,
      .y = 0,
      .width = static_cast<float>(renderInfo.swapchain.extent().width),
      .height = static_cast<float>(renderInfo.swapchain.extent().height),
      .minDepth = 0,
      .maxDepth = 1,
  }};
  vkCmdSetViewport(commandBuffer, 0, static_cast<uint32_t>(viewports.size()), viewports.data());

  std::array scissors{VkRect2D{
      .extent = renderInfo.swapchain.extent(),
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
    vk::PipelineLayout shaderLayout{device};
    vk::ShaderModule vertexShader{device, "main.vert.spv"};
    vk::ShaderModule fragmentShader{device, "main.frag.spv"};
    vk::CommandPool commandPool{device, device.graphicsQueue().familyIndex};

    std::optional<RenderInfo> renderInfo{
        std::in_place, window, device, surface, vertexShader, fragmentShader, shaderLayout};
    auto perFrame = commandPool.allocateBuffers(static_cast<uint32_t>(renderInfo->imageViews.size())) |
                    transform([&](auto cb) { return SynchronizedCommandBuffer{device, cb}; }) | to<std::vector>();
    FrameIndex frameIdx = 0;
    while (!glfwWindowShouldClose(window)) {
      glfwPollEvents();

      auto& [cmdBuffer, imageAvailable, renderFinished, cmdBufferReady] = perFrame[frameIdx];

      cmdBufferReady.wait();

      try {
        auto imgIdx = vk::acquireNextImageKHR(device, renderInfo->swapchain, imageAvailable);
        cmdBufferReady.reset();

        render(cmdBuffer, *renderInfo, frameIdx);

        vk::queueSubmit(device, cmdBuffer, imageAvailable, renderFinished, cmdBufferReady);
        vk::presentQueue(device, renderInfo->swapchain, renderFinished, imgIdx);

        if (++frameIdx >= renderInfo->imageViews.size()) {
          frameIdx = 0;
        }
      } catch (vk::OutOfDateError const&) {
        // irritatingly, there is *absolutely* no way (in standard VK) to know when an image has completed
        // presentation so the only way to safely clean up the associated resources (pipeline, semaphore, etc) is to
        // wait until the gpu idles.
        vkDeviceWaitIdle(device);
        renderInfo.emplace(window, device, surface, vertexShader, fragmentShader, shaderLayout);
        frameIdx = 0;
      }
    }

    vkDeviceWaitIdle(device);
  }

  return 0;
}
