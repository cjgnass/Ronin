#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <string>
#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

struct Controller {
  float speed{1.0f};
  float xOffset{};
  float yOffset{};
};

struct Engine {
  Engine(int w, int h, std::string t);
  void run();
  void initWindow();

  void initVulkan();
  void createInstance();
  static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(
      [[maybe_unused]] vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
      vk::DebugUtilsMessageTypeFlagsEXT type,
      const vk::DebugUtilsMessengerCallbackDataEXT *pCallbackData,
      [[maybe_unused]] void *pUserData);
  void createSurface();
  void pickPhysicalDevice();
  void createExtent();
  void createDevice();
  void createSwapchain();
  void createImageViews();
  void createPipeline();
  void createCommandPool();
  void createCommandBuffer();
  void createSyncObjects();

  void createController();

  void mainLoop();
  void drawFrame();
  void recordCommandBuffer(int imageIndex);
  void transition_image_layout(uint32_t imageIndex, vk::ImageLayout old_layout,
                               vk::ImageLayout new_layout,
                               vk::AccessFlags2 src_access_mask,
                               vk::AccessFlags2 dst_access_mask,
                               vk::PipelineStageFlags2 src_stage_mask,
                               vk::PipelineStageFlags2 dst_stage_mask);

  void cleanup();

  uint32_t MAX_FRAMES_IN_FLIGHT{2};

  int width;
  int height;
  std::string title;
  GLFWwindow *window;

  Controller controller;

  vk::raii::Instance instance{nullptr};
  vk::raii::Context context;
  vk::raii::DebugUtilsMessengerEXT debugMessenger{nullptr};
  vk::raii::SurfaceKHR surface{nullptr};
  vk::raii::PhysicalDevice physicalDevice{nullptr};
  vk::Extent2D extent;
  std::vector<vk::Image> images;
  std::vector<vk::raii::ImageView> imageViews;
  vk::raii::Device device{nullptr};
  vk::raii::Queue queue{nullptr};
  uint32_t queueIndex{};
  vk::raii::SwapchainKHR swapchain{nullptr};
  vk::SurfaceFormatKHR swapchainSurfaceFormat;
  vk::raii::Pipeline pipeline{nullptr};
  vk::raii::PipelineLayout pipelineLayout{nullptr};
  vk::raii::CommandPool commandPool{nullptr};
  vk::raii::CommandBuffer commandBuffer{nullptr};
  std::vector<vk::raii::CommandBuffer> commandBuffers;
  vk::raii::Semaphore presentCompleteSemaphore{nullptr};
  vk::raii::Semaphore renderFinishedSemaphore{nullptr};
  std::vector<vk::raii::Semaphore> presentCompleteSemaphores;
  std::vector<vk::raii::Semaphore> renderFinishedSemaphores;
  vk::raii::Fence drawFence{nullptr};
  std::vector<vk::raii::Fence> inFlightFences;
};
