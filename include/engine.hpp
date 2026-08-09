#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <string>
#define VULKAN_HPP_NO_CONSTRUCTORS
#include <array>
#include <glm/glm.hpp>
#include <vector>
#include <vulkan/vulkan_raii.hpp>

constexpr uint32_t MAX_FRAMES_IN_FLIGHT{2};

struct Vertex {
  glm::vec2 position;
  glm::vec3 color;

  static vk::VertexInputBindingDescription getBindingDescriptions() {
    return {.binding = 0,
            .stride = sizeof(Vertex),
            .inputRate = vk::VertexInputRate::eVertex};
  }

  static std::array<vk::VertexInputAttributeDescription, 2>
  getAttributeDescriptions() {
    return {{{.location = 0,
              .binding = 0,
              .format = vk::Format::eR32G32Sfloat,
              .offset = offsetof(Vertex, position)},
             {.location = 1,
              .binding = 0,
              .format = vk::Format::eR32G32B32Sfloat,
              .offset = offsetof(Vertex, color)}}};
  }
};
const std::vector<Vertex> vertices = {{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
                                      {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
                                      {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
                                      {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}};

const std::vector<uint16_t> indices = {0, 1, 2, 2, 3, 0};

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
  void createVertexBuffer();
  std::pair<vk::raii::Buffer, vk::raii::DeviceMemory>
  createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage,
               vk::MemoryPropertyFlags properties);
  void copyBuffer(vk::raii::Buffer &srcBuffer, vk::raii::Buffer &dstBuffer,
                  vk::DeviceSize size);
  void createIndexBuffer();
  void createCommandBuffers();
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

  uint32_t frameIndex{0};

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
  vk::raii::Buffer vertexBuffer{nullptr};
  vk::raii::DeviceMemory vertexBufferMemory{nullptr};
  vk::raii::Buffer indexBuffer{nullptr};
  vk::raii::DeviceMemory indexBufferMemory{nullptr};
  vk::raii::CommandPool commandPool{nullptr};
  std::vector<vk::raii::CommandBuffer> commandBuffers;
  std::vector<vk::raii::Semaphore> presentCompleteSemaphores;
  std::vector<vk::raii::Semaphore> renderFinishedSemaphores;
  std::vector<vk::raii::Fence> inFlightFences;
};
