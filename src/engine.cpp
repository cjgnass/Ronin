#include "engine.hpp"
#include "vulkan/vulkan.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <map>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#include <vector>

using std::vector;

const vector<const char *> validationLayers = {"VK_LAYER_KHRONOS_validation"};

#ifdef NDEBUG
constexpr const bool enableValidationLayers{false};
#else
constexpr bool enableValidationLayers{true};
#endif

Engine::Engine(int w, int h, std::string t) : width(w), height(h), title(t) {}

void Engine::run() {
  initWindow();
  initVulkan();
  mainLoop();
  cleanup();
}

void Engine::initWindow() {
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
  window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
}

void Engine::initVulkan() {
  createInstance();
  createSurface();
  pickPhysicalDevice();
  createExtent();
  createDevice();
  createSwapchain();
  createImageViews();
  createDescriptorSetLayout();
  createPipeline();
  createCommandPool();
  createDepthResources();
  createTextureImage();
  createTextureImageView();
  createTextureSampler();
  loadModel();
  createVertexBuffer();
  createIndexBuffer();
  createUniformBuffers();
  createDescriptorPool();
  createDescriptorSets();
  createCommandBuffers();
  createSyncObjects();
}

void Engine::createInstance() {
  vk::ApplicationInfo appInfo{
      .pApplicationName = title.c_str(),
      .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
      .pEngineName = "Ronin",
      .apiVersion = VK_MAKE_VERSION(1, 4, 0),
  };
  void *instance_info_p_next{nullptr};

  // layers
  vector<const char *> layers{};
  vk::DebugUtilsMessengerCreateInfoEXT debugCreateInfo = {};
  if (enableValidationLayers) {
    vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
    vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags(
        vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
        vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
        vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);
    debugCreateInfo.messageSeverity = severityFlags;
    debugCreateInfo.messageType = messageTypeFlags;
    debugCreateInfo.pfnUserCallback = debugCallback;
    debugCreateInfo.pUserData = nullptr;
    layers.assign(validationLayers.begin(), validationLayers.end());
    instance_info_p_next = &debugCreateInfo;
  }

  // extensions
  vector<const char *> extensions{};
  uint32_t glfwExtensionCount{0};
  const char **glfwExtensions{
      glfwGetRequiredInstanceExtensions(&glfwExtensionCount)};
  for (uint32_t i{0}; i < glfwExtensionCount; i++) {
    extensions.push_back(glfwExtensions[i]);
  }
  if (enableValidationLayers) {
    extensions.push_back(vk::EXTDebugUtilsExtensionName);
  }
  extensions.push_back(vk::KHRPortabilityEnumerationExtensionName);

  // create instance
  vk::InstanceCreateInfo instanceInfo{
      .pNext = instance_info_p_next,
      .flags = vk::InstanceCreateFlags() |
               vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR,
      .pApplicationInfo = &appInfo,
      .enabledLayerCount = static_cast<uint32_t>(layers.size()),
      .ppEnabledLayerNames = layers.data(),
      .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
      .ppEnabledExtensionNames = extensions.data()};
  instance = vk::raii::Instance(context, instanceInfo, nullptr);

  // create debugMessenger
  if (enableValidationLayers) {
    debugMessenger =
        vk::raii::DebugUtilsMessengerEXT{instance, debugCreateInfo};
  }
}

VKAPI_ATTR vk::Bool32 VKAPI_CALL Engine::debugCallback(

    [[maybe_unused]] vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
    vk::DebugUtilsMessageTypeFlagsEXT type,
    const vk::DebugUtilsMessengerCallbackDataEXT *pCallbackData,
    [[maybe_unused]] void *pUserData) {
  std::cerr << "validation layer: type " << to_string(type)
            << " msg: " << pCallbackData->pMessage << std::endl;
  return vk::False;
}

void Engine::createSurface() {
  VkSurfaceKHR rawSurface{};
  glfwCreateWindowSurface(static_cast<VkInstance>(*instance), window, nullptr,
                          &rawSurface);
  surface = vk::raii::SurfaceKHR{instance, rawSurface};
}

void Engine::pickPhysicalDevice() {
  auto physicalDevices{instance.enumeratePhysicalDevices()};
  std::multimap<int, vk::PhysicalDevice> candidates{};
  for (const auto &pd : physicalDevices) {
    int score{0};
    auto properties{pd.getProperties()};

    // minimum requirements
    if (properties.apiVersion < VK_API_VERSION_1_4) {
      continue;
    }
    if (properties.deviceType != vk::PhysicalDeviceType::eDiscreteGpu) {
      score += 10000;
    }
    score += properties.limits.maxImageDimension2D;

    // features
    // auto features{pd.getFeatures()};

    candidates.insert(std::make_pair(score, pd));
  }
  if (candidates.empty()) {
    throw std::runtime_error("pickPhysicalDevice : no suitable GPU found");
  }
  physicalDevice =
      vk::raii::PhysicalDevice{instance, candidates.rbegin()->second};
}

void Engine::createExtent() {
  auto surfaceCapabilities{physicalDevice.getSurfaceCapabilitiesKHR(surface)};
  if (surfaceCapabilities.currentExtent.width !=
      std::numeric_limits<uint32_t>::max()) {
    extent = surfaceCapabilities.currentExtent;
  } else {
    int extentWidth{}, extentHeight{};
    glfwGetFramebufferSize(window, &extentWidth, &extentHeight);
    extent = vk::Extent2D{
        std::clamp<uint32_t>(extentWidth,
                             surfaceCapabilities.minImageExtent.width,
                             surfaceCapabilities.maxImageExtent.width),
        std::clamp<uint32_t>(extentHeight,
                             surfaceCapabilities.minImageExtent.height,
                             surfaceCapabilities.maxImageExtent.height)};
  }
}

void Engine::createSwapchain() {
  auto surfaceCapabilities{physicalDevice.getSurfaceCapabilitiesKHR(surface)};
  // get minImageCount
  uint32_t minImageCount = std::max(3u, surfaceCapabilities.minImageCount);
  if ((0 < surfaceCapabilities.maxImageCount) &&
      (surfaceCapabilities.maxImageCount < minImageCount)) {
    minImageCount = surfaceCapabilities.maxImageCount;
  }
  // get format
  auto availableFormats{physicalDevice.getSurfaceFormatsKHR(surface)};
  if (availableFormats.empty()) {
    throw std::runtime_error("No available surface formats");
  }
  const auto formatIt =
      std::ranges::find_if(availableFormats, [](const auto &format) {
        return format.format == vk::Format::eB8G8R8A8Srgb &&
               format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
      });
  // get presentMode
  vk::PresentModeKHR presentMode;
  auto availablePresentModes{physicalDevice.getSurfacePresentModesKHR(surface)};
  bool supportsMailbox{
      std::ranges::any_of(availablePresentModes, [](const auto &pm) {
        return pm == vk::PresentModeKHR::eMailbox;
      })};
  presentMode = supportsMailbox ? vk::PresentModeKHR::eMailbox
                                : vk::PresentModeKHR::eFifo;
  swapchainSurfaceFormat =
      formatIt != availableFormats.end() ? *formatIt : availableFormats[0];
  // create swapchain
  vk::SwapchainCreateInfoKHR swapchainInfo{
      .surface = *surface,
      .minImageCount = minImageCount,
      .imageFormat = swapchainSurfaceFormat.format,
      .imageColorSpace = swapchainSurfaceFormat.colorSpace,
      .imageExtent = extent,
      .imageArrayLayers = 1,
      .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
      .imageSharingMode = vk::SharingMode::eExclusive,
      .preTransform = surfaceCapabilities.currentTransform,
      .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
      .presentMode = presentMode,
      .clipped = true};
  swapchain = vk::raii::SwapchainKHR{device, swapchainInfo};
}

void Engine::createImageViews() {
  images = swapchain.getImages();
  vk::ImageViewCreateInfo imageViewCreateInfo{
      .viewType = vk::ImageViewType::e2D,
      .format = swapchainSurfaceFormat.format,
      .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}};
  imageViewCreateInfo.components = {
      vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity,
      vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity};
  imageViewCreateInfo.subresourceRange = {.aspectMask =
                                              vk::ImageAspectFlagBits::eColor,
                                          .levelCount = 1,
                                          .layerCount = 1};
  for (auto &image : images) {
    imageViewCreateInfo.image = image;
    imageViews.emplace_back(createImageView(
        image, swapchainSurfaceFormat.format, vk::ImageAspectFlagBits::eColor));
  }
}

void Engine::createDevice() {
  // extension check
  std::vector<const char *> requiredDeviceExtensions = {
      vk::KHRSwapchainExtensionName,
  };
#if defined(__APPLE__)
  requiredDeviceExtensions.push_back("VK_KHR_portability_subset");
#endif
  auto availableDeviceExtensions =
      physicalDevice.enumerateDeviceExtensionProperties();
  bool supportsAllRequiredExtensions = std::ranges::all_of(
      requiredDeviceExtensions,
      [&availableDeviceExtensions](auto const &requiredDeviceExtension) {
        return std::ranges::any_of(
            availableDeviceExtensions,
            [requiredDeviceExtension](auto const &availableDeviceExtension) {
              return strcmp(availableDeviceExtension.extensionName,
                            requiredDeviceExtension) == 0;
            });
      });
  if (!supportsAllRequiredExtensions) {
    throw std::runtime_error("required device extensions not supported");
  }
  // feature check
  auto features = physicalDevice.template getFeatures2<
      vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan11Features,
      vk::PhysicalDeviceVulkan13Features,
      vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
  bool supportsRequiredFeatures =
      features.template get<vk::PhysicalDeviceVulkan11Features>()
          .shaderDrawParameters &&
      features.template get<vk::PhysicalDeviceVulkan13Features>()
          .dynamicRendering &&
      features.template get<vk::PhysicalDeviceVulkan13Features>()
          .synchronization2 &&
      features.template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>()
          .extendedDynamicState;
  if (!supportsRequiredFeatures) {
    throw std::runtime_error("required features not supported");
  }
  std::vector<vk::QueueFamilyProperties> queueFamilyProperties =
      physicalDevice.getQueueFamilyProperties();
  queueIndex = ~0;
  for (uint32_t qfpIndex = 0; qfpIndex < queueFamilyProperties.size();
       qfpIndex++) {
    if ((queueFamilyProperties[qfpIndex].queueFlags &
         vk::QueueFlagBits::eGraphics) &&
        physicalDevice.getSurfaceSupportKHR(qfpIndex, *surface)) {
      queueIndex = qfpIndex;
      break;
    }
  }
  if (queueIndex == static_cast<uint32_t>(~0)) {
    throw std::runtime_error("Could not find a queue for graphics and present");
  }
  // create queue and device
  float queuePriority{0.5f};
  vk::DeviceQueueCreateInfo deviceQueueCreateInfo{
      .queueFamilyIndex = queueIndex,
      .queueCount = 1,
      .pQueuePriorities = &queuePriority};
  vk::StructureChain<vk::PhysicalDeviceFeatures2,
                     vk::PhysicalDeviceVulkan11Features,
                     vk::PhysicalDeviceVulkan13Features,
                     vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>
      featureChain = {{.features = {.samplerAnisotropy = true}},
                      {.shaderDrawParameters = true},
                      {.synchronization2 = true, .dynamicRendering = true},
                      {.extendedDynamicState = true}};

  vk::DeviceCreateInfo deviceCreateInfo{
      .pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
      .queueCreateInfoCount = 1,
      .pQueueCreateInfos = &deviceQueueCreateInfo,
      .enabledExtensionCount =
          static_cast<uint32_t>(requiredDeviceExtensions.size()),
      .ppEnabledExtensionNames = requiredDeviceExtensions.data()};
  device = vk::raii::Device{physicalDevice, deviceCreateInfo};
  queue = vk::raii::Queue{device, queueIndex, 0};
}

void Engine::createDescriptorSetLayout() {
  std::array<vk::DescriptorSetLayoutBinding, 2> bindings{
      {{.binding = 0,
        .descriptorType = vk::DescriptorType::eUniformBuffer,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eVertex},
       {.binding = 1,
        .descriptorType = vk::DescriptorType::eCombinedImageSampler,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eFragment}}};
  vk::DescriptorSetLayoutCreateInfo layoutInfo{
      .bindingCount = static_cast<uint32_t>(bindings.size()),
      .pBindings = bindings.data()};
  descriptorSetLayout = vk::raii::DescriptorSetLayout(device, layoutInfo);
}

void Engine::createPipeline() {
  // read shader file
  std::ifstream shaderFile("./build/shaders/shader.spv",
                           std::ios::ate | std::ios::binary);
  if (!shaderFile.is_open()) {
    throw std::runtime_error("Failed to open shader file");
  }
  std::vector<char> shaderBuffer(shaderFile.tellg());
  shaderFile.seekg(0);
  shaderFile.read(shaderBuffer.data(), shaderBuffer.size());
  shaderFile.close();
  // create graphics pipeline
  vk::ShaderModuleCreateInfo shaderModuleCreateInfo{
      .codeSize = shaderBuffer.size(),
      .pCode = reinterpret_cast<const uint32_t *>(shaderBuffer.data()),
  };
  vk::raii::ShaderModule shaderModule{device, shaderModuleCreateInfo};
  vk::PipelineShaderStageCreateInfo vertShaderStageCreateInfo{
      .stage = vk::ShaderStageFlagBits::eVertex,
      .module = shaderModule,
      .pName = "vertMain",
  };
  vk::PipelineShaderStageCreateInfo fragShaderStageCreateInfo{
      .stage = vk::ShaderStageFlagBits::eFragment,
      .module = shaderModule,
      .pName = "fragMain",
  };
  vk::PipelineShaderStageCreateInfo shaderStages[]{vertShaderStageCreateInfo,
                                                   fragShaderStageCreateInfo};
  auto bindingDescriptions{Vertex::getBindingDescriptions()};
  auto attributeDescriptions{Vertex::getAttributeDescriptions()};
  vk::PipelineVertexInputStateCreateInfo vertexInputInfo{
      // .vertexBindingDescriptionCount =
      // static_cast<uint32_t>(bindingDescriptions.size()),
      .vertexBindingDescriptionCount = 1,
      .pVertexBindingDescriptions = &bindingDescriptions,
      .vertexAttributeDescriptionCount =
          static_cast<uint32_t>(attributeDescriptions.size()),
      .pVertexAttributeDescriptions = attributeDescriptions.data()};
  vk::PipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo{
      .topology = vk::PrimitiveTopology::eTriangleList};

  vk::Viewport viewport{0.0f,
                        0.0f,
                        static_cast<float>(extent.width),
                        static_cast<float>(extent.height),
                        0.0f,
                        1.0f};
  vk::Rect2D scissor{vk::Offset2D{0, 0}, extent};
  vk::PipelineViewportStateCreateInfo viewportStateCreateInfo{
      .viewportCount = 1,
      .pViewports = &viewport,
      .scissorCount = 1,
      .pScissors = &scissor};
  vk::PipelineRasterizationStateCreateInfo rasterizerCreateInfo{
      .depthClampEnable = vk::False,
      .rasterizerDiscardEnable = vk::False,
      .polygonMode = vk::PolygonMode::eFill,
      .cullMode = vk::CullModeFlagBits::eNone,
      .frontFace = vk::FrontFace::eClockwise,
      .depthBiasEnable = vk::False,
      .lineWidth = 1.0f};
  vk::PipelineMultisampleStateCreateInfo multisamplingCreateInfo{
      .rasterizationSamples = vk::SampleCountFlagBits::e1,
      .sampleShadingEnable = vk::False};
  vk::PipelineColorBlendAttachmentState colorBlendAttachment{
      .blendEnable = vk::True,
      .srcColorBlendFactor = vk::BlendFactor::eSrcAlpha,
      .dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha,
      .colorBlendOp = vk::BlendOp::eAdd,
      .srcAlphaBlendFactor = vk::BlendFactor::eOne,
      .dstAlphaBlendFactor = vk::BlendFactor::eZero,
      .alphaBlendOp = vk::BlendOp::eAdd,
      .colorWriteMask =
          vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
          vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA};
  vk::PipelineColorBlendStateCreateInfo colorBlending{
      .logicOpEnable = vk::False,
      .logicOp = vk::LogicOp::eCopy,
      .attachmentCount = 1,
      .pAttachments = &colorBlendAttachment};
  vk::PipelineLayoutCreateInfo pipelineLayoutInfo{
      .setLayoutCount = 1,
      .pSetLayouts = &*descriptorSetLayout,
      .pushConstantRangeCount = 0,
  };
  pipelineLayout = vk::raii::PipelineLayout{device, pipelineLayoutInfo};
  std::vector<vk::DynamicState> dynamicStates = {vk::DynamicState::eViewport,
                                                 vk::DynamicState::eScissor};
  vk::PipelineDynamicStateCreateInfo dynamicState{
      .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
      .pDynamicStates = dynamicStates.data()};
  vk::PipelineDepthStencilStateCreateInfo depthStencil{
      .depthTestEnable = vk::True,
      .depthWriteEnable = vk::True,
      .depthCompareOp = vk::CompareOp::eLess,
      .depthBoundsTestEnable = vk::False,
      .stencilTestEnable = vk::False};
  vk::StructureChain<vk::GraphicsPipelineCreateInfo,
                     vk::PipelineRenderingCreateInfo>
      pipelineCreateInfoChain = {
          {.stageCount = 2,
           .pStages = shaderStages,
           .pVertexInputState = &vertexInputInfo,
           .pInputAssemblyState = &inputAssemblyCreateInfo,
           .pViewportState = &viewportStateCreateInfo,
           .pRasterizationState = &rasterizerCreateInfo,
           .pMultisampleState = &multisamplingCreateInfo,
           .pDepthStencilState = &depthStencil,
           .pColorBlendState = &colorBlending,
           .pDynamicState = &dynamicState,
           .layout = pipelineLayout,
           .renderPass = nullptr},
          {.colorAttachmentCount = 1,
           .pColorAttachmentFormats = &swapchainSurfaceFormat.format,
           .depthAttachmentFormat = findSupportedFormat(
               {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint,
                vk::Format::eD24UnormS8Uint},
               vk::ImageTiling::eOptimal,
               vk::FormatFeatureFlagBits::eDepthStencilAttachment)}};
  pipeline = vk::raii::Pipeline{
      device, nullptr,
      pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>()};
}

void Engine::loadModel() {
  tinyobj::attrib_t attrib;
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;
  std::string warn, err;

  if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err,
                        vikingRoomObj.modelPath.c_str())) {
    throw std::runtime_error(warn + err);
  }
  for (const auto &shape : shapes) {
    for (const auto &index : shape.mesh.indices) {
      Vertex vertex{};
      vertex.position = {attrib.vertices[3 * index.vertex_index + 0],
                         attrib.vertices[3 * index.vertex_index + 1],
                         attrib.vertices[3 * index.vertex_index + 2]};

      vertex.texCoord = {attrib.texcoords[2 * index.texcoord_index + 0],
                         attrib.texcoords[2 * index.texcoord_index + 1]};

      vertex.color = {1.0f, 1.0f, 1.0f};
      vertices.push_back(vertex);
      indices.push_back(indices.size());
    }
  }
}

void Engine::createVertexBuffer() {
  vk::DeviceSize bufferSize{sizeof(vertices[0]) * vertices.size()};
  auto [stagingBuffer, stagingBufferMemory] =
      createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc,
                   vk::MemoryPropertyFlagBits::eHostVisible |
                       vk::MemoryPropertyFlagBits::eHostCoherent);
  void *dataStaging = stagingBufferMemory.mapMemory(0, bufferSize);
  memcpy(dataStaging, vertices.data(), bufferSize);
  stagingBufferMemory.unmapMemory();
  std::tie(vertexBuffer, vertexBufferMemory) =
      createBuffer(bufferSize,
                   vk::BufferUsageFlagBits::eVertexBuffer |
                       vk::BufferUsageFlagBits::eTransferDst,
                   vk::MemoryPropertyFlagBits::eDeviceLocal);
  copyBuffer(stagingBuffer, vertexBuffer, bufferSize);
}

std::pair<vk::raii::Buffer, vk::raii::DeviceMemory>
Engine::createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage,
                     vk::MemoryPropertyFlags properties) {
  vk::BufferCreateInfo bufferInfo{
      .size = size, .usage = usage, .sharingMode = vk::SharingMode::eExclusive};
  vk::raii::Buffer buffer{vk::raii::Buffer(device, bufferInfo)};
  vk::MemoryRequirements memoryRequirements = buffer.getMemoryRequirements();
  vk::MemoryAllocateInfo memoryAllocateInfo{
      .allocationSize = memoryRequirements.size,
      .memoryTypeIndex =
          findMemoryTypeIndex(memoryRequirements.memoryTypeBits, properties)};

  auto bufferMemory{vk::raii::DeviceMemory{device, memoryAllocateInfo}};
  buffer.bindMemory(*bufferMemory, 0);
  return {std::move(buffer), std::move(bufferMemory)};
}

uint32_t Engine::findMemoryTypeIndex(uint32_t typeFilter,
                                     vk::MemoryPropertyFlags properties) {
  vk::PhysicalDeviceMemoryProperties memProperties =
      physicalDevice.getMemoryProperties();

  for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
    if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags &
                                    properties) == properties) {
      return i;
    }
  }

  throw std::runtime_error("failed to find suitable memory type!");
}

void Engine::copyBuffer(vk::raii::Buffer &srcBuffer,
                        vk::raii::Buffer &dstBuffer, vk::DeviceSize size) {
  vk::raii::CommandBuffer commandBuffer{beginSingleTimeCommands()};
  commandBuffer.copyBuffer(srcBuffer, dstBuffer, vk::BufferCopy(0, 0, size));
  endSingleTimeCommands(std::move(commandBuffer));
}

vk::raii::CommandBuffer Engine::beginSingleTimeCommands() {
  vk::CommandBufferAllocateInfo allocInfo{.commandPool = commandPool,
                                          .level =
                                              vk::CommandBufferLevel::ePrimary,
                                          .commandBufferCount = 1};
  vk::raii::CommandBuffer commandBuffer =
      std::move(device.allocateCommandBuffers(allocInfo).front());
  commandBuffer.begin(
      {.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
  return commandBuffer;
}

void Engine::endSingleTimeCommands(vk::raii::CommandBuffer &&commandBuffer) {
  commandBuffer.end();
  queue.submit(vk::SubmitInfo{.commandBufferCount = 1,
                              .pCommandBuffers = &*commandBuffer},
               nullptr);
  queue.waitIdle();
}

void Engine::createIndexBuffer() {
  vk::DeviceSize bufferSize = sizeof(indices[0]) * indices.size();

  auto [stagingBuffer, stagingBufferMemory] =
      createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc,
                   vk::MemoryPropertyFlagBits::eHostVisible |
                       vk::MemoryPropertyFlagBits::eHostCoherent);

  void *data = stagingBufferMemory.mapMemory(0, bufferSize);
  memcpy(data, indices.data(), (size_t)bufferSize);
  stagingBufferMemory.unmapMemory();

  std::tie(indexBuffer, indexBufferMemory) =
      createBuffer(bufferSize,
                   vk::BufferUsageFlagBits::eIndexBuffer |
                       vk::BufferUsageFlagBits::eTransferDst,
                   vk::MemoryPropertyFlagBits::eDeviceLocal);

  copyBuffer(stagingBuffer, indexBuffer, bufferSize);
}

void Engine::createUniformBuffers() {
  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    vk::DeviceSize bufferSize = sizeof(UniformBufferObject);
    auto [buffer, bufferMem] =
        createBuffer(bufferSize, vk::BufferUsageFlagBits::eUniformBuffer,
                     vk::MemoryPropertyFlagBits::eHostVisible |
                         vk::MemoryPropertyFlagBits::eHostCoherent);
    uniformBuffers.emplace_back(std::move(buffer));
    uniformBuffersMemory.emplace_back(std::move(bufferMem));
    uniformBuffersMapped.emplace_back(
        uniformBuffersMemory.back().mapMemory(0, bufferSize));
  }
}

void Engine::createDescriptorPool() {
  std::array<vk::DescriptorPoolSize, 2> poolSize{
      {{.type = vk::DescriptorType::eUniformBuffer,
        .descriptorCount = MAX_FRAMES_IN_FLIGHT},
       {.type = vk::DescriptorType::eCombinedImageSampler,
        .descriptorCount = MAX_FRAMES_IN_FLIGHT}}};
  vk::DescriptorPoolCreateInfo poolInfo{
      .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
      .maxSets = MAX_FRAMES_IN_FLIGHT,
      .poolSizeCount = static_cast<uint32_t>(poolSize.size()),
      .pPoolSizes = poolSize.data()};
  descriptorPool = vk::raii::DescriptorPool(device, poolInfo);
}

void Engine::createDescriptorSets() {
  std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT,
                                               *descriptorSetLayout);
  vk::DescriptorSetAllocateInfo allocInfo{
      .descriptorPool = descriptorPool,
      .descriptorSetCount = static_cast<uint32_t>(layouts.size()),
      .pSetLayouts = layouts.data()};

  descriptorSets = device.allocateDescriptorSets(allocInfo);

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    vk::DescriptorBufferInfo bufferInfo{.buffer = uniformBuffers[i],
                                        .offset = 0,
                                        .range = sizeof(UniformBufferObject)};
    vk::DescriptorImageInfo imageInfo{
        .sampler = textureSampler,
        .imageView = textureImageView,
        .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal};
    std::array<vk::WriteDescriptorSet, 2> descriptorWrites{
        {{.dstSet = descriptorSets[i],
          .dstBinding = 0,
          .dstArrayElement = 0,
          .descriptorCount = 1,
          .descriptorType = vk::DescriptorType::eUniformBuffer,
          .pBufferInfo = &bufferInfo},
         {.dstSet = descriptorSets[i],
          .dstBinding = 1,
          .dstArrayElement = 0,
          .descriptorCount = 1,
          .descriptorType = vk::DescriptorType::eCombinedImageSampler,
          .pImageInfo = &imageInfo}}};
    device.updateDescriptorSets(descriptorWrites, {});
  }
}

void Engine::createCommandPool() {
  vk::CommandPoolCreateInfo commandPoolCreateInfo{
      .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
      .queueFamilyIndex = queueIndex};
  commandPool = vk::raii::CommandPool{device, commandPoolCreateInfo};
}

void Engine::createDepthResources() {
  vk::Format depthFormat =
      findSupportedFormat({vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint,
                           vk::Format::eD24UnormS8Uint},
                          vk::ImageTiling::eOptimal,
                          vk::FormatFeatureFlagBits::eDepthStencilAttachment);
  std::tie(depthImage, depthImageMemory) = createImage(
      extent.width, extent.height, depthFormat, vk::ImageTiling::eOptimal,
      vk::ImageUsageFlagBits::eDepthStencilAttachment,
      vk::MemoryPropertyFlagBits::eDeviceLocal);
  depthImageView =
      createImageView(depthImage, depthFormat, vk::ImageAspectFlagBits::eDepth);
}

vk::Format
Engine::findSupportedFormat(const std::vector<vk::Format> &candidates,
                            vk::ImageTiling tiling,
                            vk::FormatFeatureFlags features) {
  for (const auto format : candidates) {
    vk::FormatProperties props = physicalDevice.getFormatProperties(format);

    if (((tiling == vk::ImageTiling::eLinear) &&
         ((props.linearTilingFeatures & features) == features)) ||
        ((tiling == vk::ImageTiling::eOptimal) &&
         ((props.optimalTilingFeatures & features) == features))) {
      return format;
    }
  }

  throw std::runtime_error("failed to find supported format!");
}

void Engine::createTextureImage() {
  int texWidth, texHeight, texChannels;
  stbi_uc *pixels = stbi_load(vikingRoomObj.texturePath.c_str(), &texWidth,
                              &texHeight, &texChannels, STBI_rgb_alpha);
  vk::DeviceSize imageSize = texWidth * texHeight * 4;

  if (!pixels) {
    throw std::runtime_error("failed to load texture image!");
  }

  auto [stagingBuffer, stagingBufferMemory] =
      createBuffer(imageSize, vk::BufferUsageFlagBits::eTransferSrc,
                   vk::MemoryPropertyFlagBits::eHostVisible |
                       vk::MemoryPropertyFlagBits::eHostCoherent);

  void *data = stagingBufferMemory.mapMemory(0, imageSize);
  memcpy(data, pixels, imageSize);
  stagingBufferMemory.unmapMemory();

  stbi_image_free(pixels);

  std::tie(textureImage, textureImageMemory) = createImage(
      texWidth, texHeight, vk::Format::eR8G8B8A8Srgb, vk::ImageTiling::eOptimal,
      vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
      vk::MemoryPropertyFlagBits::eDeviceLocal);
  vk::raii::CommandBuffer commandBuffer = beginSingleTimeCommands();
  transitionImageLayout(commandBuffer, textureImage,
                        vk::ImageLayout::eUndefined,
                        vk::ImageLayout::eTransferDstOptimal);
  copyBufferToImage(commandBuffer, stagingBuffer, textureImage,
                    static_cast<uint32_t>(texWidth),
                    static_cast<uint32_t>(texHeight));
  transitionImageLayout(commandBuffer, textureImage,
                        vk::ImageLayout::eTransferDstOptimal,
                        vk::ImageLayout::eShaderReadOnlyOptimal);
  endSingleTimeCommands(std::move(commandBuffer));
}

void Engine::transitionImageLayout(vk::raii::CommandBuffer &commandBuffer,
                                   const vk::raii::Image &image,
                                   vk::ImageLayout oldLayout,
                                   vk::ImageLayout newLayout) {
  vk::ImageMemoryBarrier barrier{
      .oldLayout = oldLayout,
      .newLayout = newLayout,
      .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
      .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
      .image = image,
      .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                           .levelCount = 1,
                           .layerCount = 1}};

  vk::PipelineStageFlags sourceStage;
  vk::PipelineStageFlags destinationStage;

  if (oldLayout == vk::ImageLayout::eUndefined &&
      newLayout == vk::ImageLayout::eTransferDstOptimal) {
    barrier.srcAccessMask = {};
    barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

    sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
    destinationStage = vk::PipelineStageFlagBits::eTransfer;
  } else if (oldLayout == vk::ImageLayout::eTransferDstOptimal &&
             newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
    barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

    sourceStage = vk::PipelineStageFlagBits::eTransfer;
    destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
  } else {
    throw std::invalid_argument("unsupported layout transition!");
  }
  commandBuffer.pipelineBarrier(sourceStage, destinationStage, {}, {}, {},
                                barrier);
}

void copyBufferToImage(vk::raii::CommandBuffer &commandBuffer,
                       const vk::raii::Buffer &buffer, vk::raii::Image &image,
                       uint32_t width, uint32_t height) {
  vk::BufferImageCopy region{
      .bufferOffset = 0,
      .bufferRowLength = 0,
      .bufferImageHeight = 0,
      .imageSubresource = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                           .mipLevel = 0,
                           .baseArrayLayer = 0,
                           .layerCount = 1},
      .imageOffset = {0, 0, 0},
      .imageExtent = {width, height, 1}};
  commandBuffer.copyBufferToImage(buffer, image,
                                  vk::ImageLayout::eTransferDstOptimal, region);
}

void Engine::copyBufferToImage(vk::raii::CommandBuffer &commandBuffer,
                               const vk::raii::Buffer &buffer,
                               vk::raii::Image &image, uint32_t width,
                               uint32_t height) {
  vk::BufferImageCopy region{
      .bufferOffset = 0,
      .bufferRowLength = 0,
      .bufferImageHeight = 0,
      .imageSubresource = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                           .mipLevel = 0,
                           .baseArrayLayer = 0,
                           .layerCount = 1},
      .imageOffset = {0, 0, 0},
      .imageExtent = {width, height, 1}};
  commandBuffer.copyBufferToImage(buffer, image,
                                  vk::ImageLayout::eTransferDstOptimal, region);
}

std::pair<vk::raii::Image, vk::raii::DeviceMemory>
Engine::createImage(uint32_t width, uint32_t height, vk::Format format,
                    vk::ImageTiling tiling, vk::ImageUsageFlags usage,
                    vk::MemoryPropertyFlags properties) {
  vk::ImageCreateInfo imageInfo{.imageType = vk::ImageType::e2D,
                                .format = format,
                                .extent = {width, height, 1},
                                .mipLevels = 1,
                                .arrayLayers = 1,
                                .samples = vk::SampleCountFlagBits::e1,
                                .tiling = tiling,
                                .usage = usage,
                                .sharingMode = vk::SharingMode::eExclusive};

  vk::raii::Image image = vk::raii::Image(device, imageInfo);

  vk::MemoryRequirements memRequirements = image.getMemoryRequirements();
  vk::MemoryAllocateInfo allocInfo{
      .allocationSize = memRequirements.size,
      .memoryTypeIndex =
          findMemoryTypeIndex(memRequirements.memoryTypeBits, properties)};
  vk::raii::DeviceMemory imageMemory =
      vk::raii::DeviceMemory(device, allocInfo);
  image.bindMemory(imageMemory, 0);
  return {std::move(image), std::move(imageMemory)};
}

void Engine::createTextureImageView() {
  textureImageView = createImageView(*textureImage, vk::Format::eR8G8B8A8Srgb,
                                     vk::ImageAspectFlagBits::eColor);
}

vk::raii::ImageView Engine::createImageView(vk::Image const &image,
                                            vk::Format format,
                                            vk::ImageAspectFlags aspectFlags) {
  vk::ImageViewCreateInfo viewInfo{
      .image = image,
      .viewType = vk::ImageViewType::e2D,
      .format = format,
      .subresourceRange = {.aspectMask = aspectFlags,
                           .baseMipLevel = 0,
                           .levelCount = 1,
                           .baseArrayLayer = 0,
                           .layerCount = 1}};
  return vk::raii::ImageView(device, viewInfo);
}

void Engine::createTextureSampler() {
  vk::PhysicalDeviceProperties properties = physicalDevice.getProperties();
  vk::SamplerCreateInfo samplerInfo{
      .magFilter = vk::Filter::eLinear,
      .minFilter = vk::Filter::eLinear,
      .mipmapMode = vk::SamplerMipmapMode::eLinear,
      .addressModeU = vk::SamplerAddressMode::eRepeat,
      .addressModeV = vk::SamplerAddressMode::eRepeat,
      .addressModeW = vk::SamplerAddressMode::eRepeat,
      .mipLodBias = 0.0f,
      .anisotropyEnable = vk::True,
      .maxAnisotropy = properties.limits.maxSamplerAnisotropy,
      .compareEnable = vk::False,
      .compareOp = vk::CompareOp::eAlways};
  textureSampler = vk::raii::Sampler(device, samplerInfo);
}

void Engine::createCommandBuffers() {
  vk::CommandBufferAllocateInfo allocInfo{
      .commandPool = commandPool,
      .level = vk::CommandBufferLevel::ePrimary,
      .commandBufferCount = MAX_FRAMES_IN_FLIGHT};
  commandBuffers = vk::raii::CommandBuffers(device, allocInfo);
}

void Engine::createSyncObjects() {
  for (size_t i = 0; i < images.size(); i++) {
    renderFinishedSemaphores.emplace_back(device, vk::SemaphoreCreateInfo());
  }

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    presentCompleteSemaphores.emplace_back(device, vk::SemaphoreCreateInfo());
    inFlightFences.emplace_back(
        device,
        vk::FenceCreateInfo{.flags = vk::FenceCreateFlagBits::eSignaled});
  }
}

void Engine::createController() { controller = Controller{}; }

void Engine::mainLoop() {
  auto previousTime = std::chrono::steady_clock::now();
  while (!glfwWindowShouldClose(window)) {
    const auto currentTime = std::chrono::steady_clock::now();
    const std::chrono::duration<float> elapsed = currentTime - previousTime;
    const float deltaTime = elapsed.count(); // seconds since last frame
    previousTime = currentTime;

    glfwPollEvents();
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
      controller.xOffset -= controller.speed * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
      controller.xOffset += controller.speed * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
      controller.yOffset += controller.speed * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
      controller.yOffset -= controller.speed * deltaTime;
    drawFrame();
  }
}

void Engine::drawFrame() {
  auto fenceResult =
      device.waitForFences(*inFlightFences[frameIndex], vk::True, UINT64_MAX);
  if (fenceResult != vk::Result::eSuccess) {
    throw std::runtime_error("failed to wait for fence!");
  }
  auto [result, imageIndex] = swapchain.acquireNextImage(
      UINT64_MAX, *presentCompleteSemaphores[frameIndex], nullptr);
  device.resetFences(*inFlightFences[frameIndex]);
  updateUniformBuffer(frameIndex);
  commandBuffers[frameIndex].reset();
  recordCommandBuffer(imageIndex);
  vk::PipelineStageFlags waitDestinationStageMask(
      vk::PipelineStageFlagBits::eColorAttachmentOutput);
  const vk::SubmitInfo submitInfo{
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &*presentCompleteSemaphores[frameIndex],
      .pWaitDstStageMask = &waitDestinationStageMask,
      .commandBufferCount = 1,
      .pCommandBuffers = &*commandBuffers[frameIndex],
      .signalSemaphoreCount = 1,
      .pSignalSemaphores = &*renderFinishedSemaphores[imageIndex]};
  queue.submit(submitInfo, *inFlightFences[frameIndex]);
  const vk::PresentInfoKHR presentInfoKHR{
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &*renderFinishedSemaphores[imageIndex],
      .swapchainCount = 1,
      .pSwapchains = &*swapchain,
      .pImageIndices = &imageIndex};
  result = queue.presentKHR(presentInfoKHR);
  switch (result) {
  case vk::Result::eSuccess:
    break;
  case vk::Result::eSuboptimalKHR:
    std::cout
        << "vk::Queue::presentKHR returned vk::Result::eSuboptimalKHR !\n";
    break;
  default:
    break; // an unexpected result is returned!
  }
  frameIndex = (frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Engine::updateUniformBuffer(uint32_t currentImageIndex) {
  // static auto startTime = std::chrono::high_resolution_clock::now();
  // auto currentTime = std::chrono::high_resolution_clock::now();
  // float time = std::chrono::duration<float>(currentTime -
  // startTime).count();

  UniformBufferObject ubo{};
  // ubo.model = rotate(glm::mat4(1.0f), time * glm::radians(90.0f),
  //                    glm::vec3(0.0f, 0.0f, 1.0f));
  ubo.model = glm::mat4(1.0f);
  ubo.view = lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f),
                    glm::vec3(0.0f, 0.0f, 1.0f));
  ubo.projection = glm::perspective(glm::radians(45.0f),
                                    static_cast<float>(extent.width) /
                                        static_cast<float>(extent.height),
                                    0.1f, 10.0f);
  ubo.projection[1][1] *= -1;
  memcpy(uniformBuffersMapped[currentImageIndex], &ubo, sizeof(ubo));
}

void Engine::recordCommandBuffer(int imageIndex) {

  auto &commandBuffer{commandBuffers[frameIndex]};
  commandBuffer.begin({});
  transition_image_layout(images[imageIndex], vk::ImageLayout::eUndefined,
                          vk::ImageLayout::eColorAttachmentOptimal, {},
                          vk::AccessFlagBits2::eColorAttachmentWrite,
                          vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                          vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                          vk::ImageAspectFlagBits::eColor);
  transition_image_layout(*depthImage, vk::ImageLayout::eUndefined,
                          vk::ImageLayout::eDepthAttachmentOptimal,
                          vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
                          vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
                          vk::PipelineStageFlagBits2::eEarlyFragmentTests |
                              vk::PipelineStageFlagBits2::eLateFragmentTests,
                          vk::PipelineStageFlagBits2::eEarlyFragmentTests |
                              vk::PipelineStageFlagBits2::eLateFragmentTests,
                          vk::ImageAspectFlagBits::eDepth);
  vk::ClearValue clearColor{};
  vk::ClearValue clearDepth{
      .depthStencil =
          vk::ClearDepthStencilValue{
              .depth = 1.0f,
              .stencil = 0,
          },
  };
  vk::RenderingAttachmentInfo depthAttachmentInfo = {
      .imageView = depthImageView,
      .imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
      .loadOp = vk::AttachmentLoadOp::eClear,
      .storeOp = vk::AttachmentStoreOp::eDontCare,
      .clearValue = clearDepth};
  vk::RenderingAttachmentInfo attachmentInfo = {
      .imageView = imageViews[imageIndex],
      .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
      .loadOp = vk::AttachmentLoadOp::eClear,
      .storeOp = vk::AttachmentStoreOp::eStore,
      .clearValue = clearColor};
  vk::RenderingInfo renderingInfo = {
      .renderArea = {.offset = {0, 0}, .extent = extent},
      .layerCount = 1,
      .colorAttachmentCount = 1,
      .pColorAttachments = &attachmentInfo,
      .pDepthAttachment = &depthAttachmentInfo};

  commandBuffer.beginRendering(renderingInfo);
  commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);
  commandBuffer.setViewport(
      0, vk::Viewport(0.0f, 0.0f, static_cast<float>(extent.width),
                      static_cast<float>(extent.height), 0.0f, 1.0f));
  commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), extent));
  commandBuffer.bindVertexBuffers(0, *vertexBuffer, {0});
  commandBuffer.bindIndexBuffer(
      *indexBuffer, 0,
      vk::IndexTypeValue<decltype(indices)::value_type>::value);
  commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                   pipelineLayout, 0,
                                   *descriptorSets[frameIndex], nullptr);
  commandBuffer.drawIndexed(static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
  commandBuffer.endRendering();
  transition_image_layout(
      images[imageIndex], vk::ImageLayout::eColorAttachmentOptimal,
      vk::ImageLayout::ePresentSrcKHR,
      vk::AccessFlagBits2::eColorAttachmentWrite,         // srcAccessMask
      {},                                                 // dstAccessMask
      vk::PipelineStageFlagBits2::eColorAttachmentOutput, // srcStage
      vk::PipelineStageFlagBits2::eBottomOfPipe,          // dstStage
      vk::ImageAspectFlagBits::eColor);
  commandBuffer.end();
}

void Engine::transition_image_layout(vk::Image image,
                                     vk::ImageLayout old_layout,
                                     vk::ImageLayout new_layout,
                                     vk::AccessFlags2 src_access_mask,
                                     vk::AccessFlags2 dst_access_mask,
                                     vk::PipelineStageFlags2 src_stage_mask,
                                     vk::PipelineStageFlags2 dst_stage_mask,
                                     vk::ImageAspectFlags image_aspect_flags) {
  vk::ImageMemoryBarrier2 barrier = {
      .srcStageMask = src_stage_mask,
      .srcAccessMask = src_access_mask,
      .dstStageMask = dst_stage_mask,
      .dstAccessMask = dst_access_mask,
      .oldLayout = old_layout,
      .newLayout = new_layout,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = image,
      .subresourceRange = {.aspectMask = image_aspect_flags,
                           .baseMipLevel = 0,
                           .levelCount = 1,
                           .baseArrayLayer = 0,
                           .layerCount = 1}};
  vk::DependencyInfo dependency_info = {.dependencyFlags = {},
                                        .imageMemoryBarrierCount = 1,
                                        .pImageMemoryBarriers = &barrier};
  commandBuffers[frameIndex].pipelineBarrier2(dependency_info);
}

void Engine::cleanup() {
  queue.waitIdle();

  glfwDestroyWindow(window);
  glfwTerminate();

  imageViews.clear();
}
