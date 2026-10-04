package engine
import "core:fmt"
import "core:image"
import "core:os"
import "vendor:glfw"
import vk "vendor:vulkan"


Renderer :: struct {
	window:           glfw.WindowHandle,
	device:           vk.Device,
	queue:            vk.Queue,
	extent:           vk.Extent2D,
	swapchain:        vk.SwapchainKHR,
	swapchain_images: []vk.Image,
	swapchain_views:  []vk.ImageView,
	viewport:         vk.Viewport,
	scissor:          vk.Rect2D,
	command_pool:     vk.CommandPool,
	command_buffers:  []vk.CommandBuffer,
	image_semaphores: []vk.Semaphore,
	frame_semaphores: []vk.Semaphore,
	fences:           []vk.Fence,
	pipeline_layout:  vk.PipelineLayout,
	pipeline:         vk.Pipeline,
	frame_index:      u32,
	image_index:      u32,
}


init_renderer :: proc(renderer: ^Renderer) {
	// init window
	if !glfw.Init() {
		panic("glfw.Init() failed")
	}
	glfw.WindowHint(glfw.CLIENT_API, glfw.NO_API)
	glfw.WindowHint(glfw.RESIZABLE, glfw.FALSE)
	renderer.window = glfw.CreateWindow(WIDTH, HEIGHT, TITLE, nil, nil)
	glfw.SetInputMode(renderer.window, glfw.CURSOR, glfw.CURSOR_DISABLED)

	// init instance
	app_info := vk.ApplicationInfo {
		sType              = .APPLICATION_INFO,
		pNext              = nil,
		pApplicationName   = TITLE,
		applicationVersion = vk.MAKE_VERSION(1, 0, 0),
		pEngineName        = TITLE,
		engineVersion      = vk.MAKE_VERSION(1, 0, 0),
		apiVersion         = vk.API_VERSION_1_4,
	}
	// setting instance extensions
	instance_extensions := [dynamic]cstring{}
	defer delete(instance_extensions)
	for glfw_extension in glfw.GetRequiredInstanceExtensions() {
		append(&instance_extensions, glfw_extension)
	}
	when ENABLE_VALIDATION_LAYER {
		append(&instance_extensions, vk.EXT_DEBUG_UTILS_EXTENSION_NAME)
	}

	// setting instance layers
	instance_layers := [dynamic]cstring{}
	defer delete(instance_layers)
	when ENABLE_VALIDATION_LAYER {
		append(&instance_layers, "VK_LAYER_KHRONOS_validation")
	}

	instance_info := vk.InstanceCreateInfo {
		sType                   = .INSTANCE_CREATE_INFO,
		pApplicationInfo        = &app_info,
		enabledLayerCount       = u32(len(instance_layers)),
		ppEnabledLayerNames     = raw_data(instance_layers),
		enabledExtensionCount   = u32(len(instance_extensions)),
		ppEnabledExtensionNames = raw_data(instance_extensions),
	}
	instance: vk.Instance
	vk.load_proc_addresses_global(glfw.GetInstanceProcAddress(nil, "vkGetInstanceProcAddr"))
	result := vk.CreateInstance(&instance_info, nil, &instance)
	if result != .SUCCESS {
		fmt.panicf("vk.CreateInstance failed:", result)
	}
	vk.load_proc_addresses_instance(instance)

	// pick physical device
	pd_count: u32
	result = vk.EnumeratePhysicalDevices(instance, &pd_count, nil)
	if result != .SUCCESS {
		fmt.panicf("vk.EnumeratePhysicalDevices failed:", result)
	}
	pds := make([]vk.PhysicalDevice, pd_count)
	defer delete(pds)
	result = vk.EnumeratePhysicalDevices(instance, &pd_count, raw_data(pds))
	if result != .SUCCESS {
		fmt.panicf("vk.EnumeratePhysicalDevices failed:", result)
	}
	physical_device: vk.PhysicalDevice
	best_pd_score: u32
	for pd in pds {
		properties: vk.PhysicalDeviceProperties
		vk.GetPhysicalDeviceProperties(pd, &properties)
		if properties.apiVersion < vk.API_VERSION_1_4 {
			continue
		}
		pd_score := u32(0)
		pd_score += properties.limits.maxImageDimension2D
		if properties.deviceType == .DISCRETE_GPU {
			pd_score += 1000
		}
		if pd_score > best_pd_score {
			best_pd_score = pd_score
			physical_device = pd
		}
	}

	// init device
	graphics_queue_family_index, graphics_queue_family_index_error := get_queue_family_index(
		physical_device,
		{.GRAPHICS},
	)
	if graphics_queue_family_index_error {
		panic("get_queue_family_index failed")
	}
	graphics_queue_priority := f32(1)
	queue_infos := []vk.DeviceQueueCreateInfo {
		{
			sType = .DEVICE_QUEUE_CREATE_INFO,
			queueFamilyIndex = graphics_queue_family_index,
			queueCount = 1,
			pQueuePriorities = &graphics_queue_priority,
		},
	}
	device_extensions := [dynamic]cstring{}
	defer delete(device_extensions)
	append(&device_extensions, vk.KHR_SWAPCHAIN_EXTENSION_NAME)
	when ODIN_OS == .Darwin {
		append(&device_extensions, "VK_KHR_portability_subset")
	}
	vulkan_1_4_features := vk.PhysicalDeviceVulkan14Features {
		sType = .PHYSICAL_DEVICE_VULKAN_1_4_FEATURES,
	}
	vulkan_1_3_features := vk.PhysicalDeviceVulkan13Features {
		sType            = .PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
		synchronization2 = true,
		dynamicRendering = true,
		pNext            = &vulkan_1_4_features,
	}
	vulkan_1_2_features := vk.PhysicalDeviceVulkan12Features {
		sType = .PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
		pNext = &vulkan_1_3_features,
	}
	vulkan_1_1_features := vk.PhysicalDeviceVulkan11Features {
		sType                = .PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
		shaderDrawParameters = true,
		pNext                = &vulkan_1_2_features,
	}
	device_features := vk.PhysicalDeviceFeatures{}
	device_info := vk.DeviceCreateInfo {
		sType                   = .DEVICE_CREATE_INFO,
		pNext                   = &vulkan_1_1_features,
		queueCreateInfoCount    = u32(len(queue_infos)),
		pQueueCreateInfos       = raw_data(queue_infos),
		enabledExtensionCount   = u32(len(device_extensions)),
		ppEnabledExtensionNames = raw_data(device_extensions),
		pEnabledFeatures        = &device_features,
	}
	result = vk.CreateDevice(physical_device, &device_info, nil, &renderer.device)
	if result != .SUCCESS {
		fmt.panicf("vk.CreateDevice failed", result)
	}
	vk.load_proc_addresses_device(renderer.device)
	vk.GetDeviceQueue(renderer.device, graphics_queue_family_index, 0, &renderer.queue)

	// get surface
	surface: vk.SurfaceKHR
	result = glfw.CreateWindowSurface(instance, renderer.window, nil, &surface)
	if result != .SUCCESS {
		fmt.panicf("glfw.CreateWindowSurface failed", result)
	}
	surface_capabilities: vk.SurfaceCapabilitiesKHR
	result = vk.GetPhysicalDeviceSurfaceCapabilitiesKHR(
		physical_device,
		surface,
		&surface_capabilities,
	)
	if result != .SUCCESS {
		fmt.println("vk.GetPhysicalDeviceSurfaceCapabilitiesKHR failed:", result)
	}

	// init extent
	fb_width, fb_height := glfw.GetFramebufferSize(renderer.window)
	renderer.extent = vk.Extent2D {
		width  = clamp(
			u32(fb_width),
			surface_capabilities.minImageExtent.width,
			surface_capabilities.maxImageExtent.width,
		),
		height = clamp(
			u32(fb_height),
			surface_capabilities.minImageExtent.height,
			surface_capabilities.maxImageExtent.height,
		),
	}

	// get swapchain min image count
	min_image_count := surface_capabilities.minImageCount
	if surface_capabilities.maxImageCount == 0 ||
	   (min_image_count + 1 < surface_capabilities.maxImageCount) {
		min_image_count += 1
	}

	// get surface format
	surface_format_count: u32
	result = vk.GetPhysicalDeviceSurfaceFormatsKHR(
		physical_device,
		surface,
		&surface_format_count,
		nil,
	)
	if result != .SUCCESS {
		fmt.panicf("vk.GetPhysicalDeviceSurfaceFormatsKHR failed", result)
	}

	surface_formats := make([]vk.SurfaceFormatKHR, surface_format_count)
	defer delete(surface_formats)
	result = vk.GetPhysicalDeviceSurfaceFormatsKHR(
		physical_device,
		surface,
		&surface_format_count,
		raw_data(surface_formats),
	)
	if result != .SUCCESS {
		fmt.panicf("vk.GetPhysicalDeviceSurfaceFormatsKHR failed", result)
	}
	surface_format: vk.SurfaceFormatKHR
	found_surface_format := false
	for format in surface_formats {
		if (format.format == .R8G8B8A8_SRGB && format.colorSpace == .SRGB_NONLINEAR) ||
		   (format.format == .B8G8R8A8_SRGB && format.colorSpace == .SRGB_NONLINEAR) {
			surface_format = format
			found_surface_format = true
			break
		}
	}
	if !found_surface_format {
		panic("failed to find suitable surface format")
	}

	// init swapchain
	swapchain_info := vk.SwapchainCreateInfoKHR {
		sType                 = .SWAPCHAIN_CREATE_INFO_KHR,
		surface               = surface,
		minImageCount         = min_image_count,
		imageFormat           = surface_format.format,
		imageColorSpace       = surface_format.colorSpace,
		imageExtent           = renderer.extent,
		imageArrayLayers      = 1,
		imageUsage            = {.COLOR_ATTACHMENT},
		queueFamilyIndexCount = 1,
		pQueueFamilyIndices   = &graphics_queue_family_index,
		preTransform          = {.IDENTITY},
		compositeAlpha        = {.OPAQUE},
		presentMode           = .FIFO,
		clipped               = true,
	}
	result = vk.CreateSwapchainKHR(renderer.device, &swapchain_info, nil, &renderer.swapchain)
	if result != .SUCCESS {
		fmt.panicf("vk.CreateSwapchainKHR failed:", result)
	}

	// init swapchain images
	swapchain_image_count: u32
	result = vk.GetSwapchainImagesKHR(
		renderer.device,
		renderer.swapchain,
		&swapchain_image_count,
		nil,
	)
	if result != .SUCCESS {
		fmt.panicf("vk.GetSwapchainImagesKHR failed:", result)
	}
	renderer.swapchain_images = make([]vk.Image, swapchain_image_count)
	result = vk.GetSwapchainImagesKHR(
		renderer.device,
		renderer.swapchain,
		&swapchain_image_count,
		raw_data(renderer.swapchain_images),
	)
	if result != .SUCCESS {
		fmt.panicf("vk.GetSwapchainImagesKHR failed:", result)
	}

	// init swapchain views
	renderer.swapchain_views = make([]vk.ImageView, swapchain_image_count)
	for i in 0 ..< swapchain_image_count {
		swapchain_view_info := vk.ImageViewCreateInfo {
			sType = .IMAGE_VIEW_CREATE_INFO,
			image = renderer.swapchain_images[i],
			viewType = .D2,
			format = surface_format.format,
			subresourceRange = vk.ImageSubresourceRange {
				aspectMask = {.COLOR},
				baseMipLevel = 0,
				levelCount = 1,
				baseArrayLayer = 0,
				layerCount = 1,
			},
		}
		result = vk.CreateImageView(
			renderer.device,
			&swapchain_view_info,
			nil,
			&renderer.swapchain_views[i],
		)
		if result != .SUCCESS {
			fmt.panicf("vk.CreateImageView failed:", result)
		}
	}

	// init command pool
	command_pool_info := vk.CommandPoolCreateInfo {
		sType            = .COMMAND_POOL_CREATE_INFO,
		flags            = {.RESET_COMMAND_BUFFER},
		queueFamilyIndex = graphics_queue_family_index,
	}
	result = vk.CreateCommandPool(renderer.device, &command_pool_info, nil, &renderer.command_pool)
	if result != .SUCCESS {
		fmt.panicf("vk.CreateCommandPool failed:", result)
	}

	// init command buffers
	command_buffer_info := vk.CommandBufferAllocateInfo {
		sType              = .COMMAND_BUFFER_ALLOCATE_INFO,
		commandPool        = renderer.command_pool,
		commandBufferCount = MAX_FRAMES_IN_FLIGHT,
	}
	renderer.command_buffers = make([]vk.CommandBuffer, MAX_FRAMES_IN_FLIGHT)
	for i in 0 ..< MAX_FRAMES_IN_FLIGHT {
		result = vk.AllocateCommandBuffers(
			renderer.device,
			&command_buffer_info,
			&renderer.command_buffers[i],
		)
		if result != .SUCCESS {
			fmt.panicf("vk.CreateCommandPool failed:", result)
		}
	}

	// init sync objects
	fence_info := vk.FenceCreateInfo {
		sType = .FENCE_CREATE_INFO,
		flags = {.SIGNALED},
	}
	semaphore_info := vk.SemaphoreCreateInfo {
		sType = .SEMAPHORE_CREATE_INFO,
	}
	renderer.fences = make([]vk.Fence, MAX_FRAMES_IN_FLIGHT)
	renderer.frame_semaphores = make([]vk.Semaphore, MAX_FRAMES_IN_FLIGHT)
	for i in 0 ..< MAX_FRAMES_IN_FLIGHT {
		result = vk.CreateFence(renderer.device, &fence_info, nil, &renderer.fences[i])
		if result != .SUCCESS {
			fmt.panicf("vk.CreateFence failed", result)
		}
		result = vk.CreateSemaphore(
			renderer.device,
			&semaphore_info,
			nil,
			&renderer.frame_semaphores[i],
		)
		if result != .SUCCESS {
			fmt.panicf("vk.CreateFence failed", result)
		}
	}
	renderer.image_semaphores = make([]vk.Semaphore, swapchain_image_count)
	for i in 0 ..< swapchain_image_count {
		result = vk.CreateSemaphore(
			renderer.device,
			&semaphore_info,
			nil,
			&renderer.image_semaphores[i],
		)
		if result != .SUCCESS {
			fmt.panicf("vk.CreateFence failed", result)
		}
	}

	// init viewport
	renderer.viewport = vk.Viewport {
		x        = 0.0,
		y        = 0.0,
		width    = f32(renderer.extent.width),
		height   = f32(renderer.extent.height),
		minDepth = 0.0,
		maxDepth = 1.0,
	}

	// init scissor
	renderer.scissor = vk.Rect2D {
		offset = vk.Offset2D{x = 0, y = 0},
		extent = renderer.extent,
	}

	// init pipeline layout
	pipeline_layout_info := vk.PipelineLayoutCreateInfo {
		sType = .PIPELINE_LAYOUT_CREATE_INFO,
	}
	result = vk.CreatePipelineLayout(
		renderer.device,
		&pipeline_layout_info,
		nil,
		&renderer.pipeline_layout,
	)
	if result != .SUCCESS {
		fmt.panicf("vk.CreatePipelineLayout failed", result)
	}


	// init pipeline
	shader_code, err := os.read_entire_file("shader.spv", context.allocator)
	if err != nil {
		fmt.panicf("os.read_entire_file failed:", err)
	}
	shader_module_info := vk.ShaderModuleCreateInfo {
		sType    = .SHADER_MODULE_CREATE_INFO,
		codeSize = len(shader_code),
		pCode    = cast([^]u32)raw_data(shader_code),
	}
	shader_module: vk.ShaderModule
	result = vk.CreateShaderModule(renderer.device, &shader_module_info, nil, &shader_module)
	if result != .SUCCESS {
		fmt.panicf("vk.CreateShaderModule failed", result)
	}
	shader_stages := []vk.PipelineShaderStageCreateInfo {
		{
			sType = .PIPELINE_SHADER_STAGE_CREATE_INFO,
			stage = {.VERTEX},
			module = shader_module,
			pName = "vertex_main",
		},
		{
			sType = .PIPELINE_SHADER_STAGE_CREATE_INFO,
			stage = {.FRAGMENT},
			module = shader_module,
			pName = "fragment_main",
			pSpecializationInfo = nil,
		},
	}
	rendering_info := vk.PipelineRenderingCreateInfo {
		sType                   = .PIPELINE_RENDERING_CREATE_INFO,
		colorAttachmentCount    = 1,
		pColorAttachmentFormats = &surface_format.format,
	}
	vertex_input_info := vk.PipelineVertexInputStateCreateInfo {
		sType = .PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
	}
	input_assembly_info := vk.PipelineInputAssemblyStateCreateInfo {
		sType    = .PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		topology = .TRIANGLE_LIST,
	}
	viewport_info := vk.PipelineViewportStateCreateInfo {
		sType         = .PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		viewportCount = 1,
		pViewports    = &renderer.viewport,
		scissorCount  = 1,
		pScissors     = &renderer.scissor,
	}
	rasterization_info := vk.PipelineRasterizationStateCreateInfo {
		sType     = .PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		frontFace = .COUNTER_CLOCKWISE,
		lineWidth = 1,
	}
	multisampling_info := vk.PipelineMultisampleStateCreateInfo {
		sType                = .PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		rasterizationSamples = {._1},
	}
	color_blend_attachment := vk.PipelineColorBlendAttachmentState {
		blendEnable         = true,
		srcColorBlendFactor = .SRC_COLOR,
		colorWriteMask      = {.R, .G, .B, .A},
	}
	color_blend_info := vk.PipelineColorBlendStateCreateInfo {
		sType           = .PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		attachmentCount = 1,
		pAttachments    = &color_blend_attachment,
	}
	pipeline_info := vk.GraphicsPipelineCreateInfo {
		sType               = .GRAPHICS_PIPELINE_CREATE_INFO,
		pNext               = &rendering_info,
		stageCount          = u32(len(shader_stages)),
		pStages             = raw_data(shader_stages),
		pVertexInputState   = &vertex_input_info,
		pInputAssemblyState = &input_assembly_info,
		pViewportState      = &viewport_info,
		pRasterizationState = &rasterization_info,
		pMultisampleState   = &multisampling_info,
		pColorBlendState    = &color_blend_info,
		layout              = renderer.pipeline_layout,
	}
	result = vk.CreateGraphicsPipelines(
		renderer.device,
		{},
		1,
		&pipeline_info,
		nil,
		&renderer.pipeline,
	)
	if result != .SUCCESS {
		fmt.panicf("vk.CreateGraphicsPipelines failed:", result)
	}
}

render_frame :: proc(renderer: ^Renderer) {
	// wait for frame to be ready before we acquire the next image
	fence := renderer.fences[renderer.frame_index]
	result := vk.WaitForFences(renderer.device, 1, &fence, true, max(u64))
	if result != .SUCCESS {
		fmt.panicf("vk.WaitForFences failed:", result)
	}
	frame_semaphore := renderer.frame_semaphores[renderer.frame_index]
	result = vk.AcquireNextImageKHR(
		renderer.device,
		renderer.swapchain,
		max(u64),
		frame_semaphore,
		{},
		&renderer.image_index,
	)
	if result != .SUCCESS {
		fmt.panicf("vk.AcquireNextImageKHR failed:", result)
	}
	result = vk.ResetFences(renderer.device, 1, &fence)
	if result != .SUCCESS {
		fmt.panicf("vk.ResetFences failed:", result)
	}
	command_buffer := renderer.command_buffers[renderer.frame_index]
	command_buffer_begin_info := vk.CommandBufferBeginInfo {
		sType = .COMMAND_BUFFER_BEGIN_INFO,
	}

	// begin recording
	result = vk.BeginCommandBuffer(command_buffer, &command_buffer_begin_info)
	if result != .SUCCESS {
		fmt.panicf("vk.BeginCommandBuffer failed:", result)
	}

	// transition image to be a render target
	swapchain_image := renderer.swapchain_images[renderer.image_index]
	render_image_barrier := vk.ImageMemoryBarrier2 {
		sType = .IMAGE_MEMORY_BARRIER_2,
		dstStageMask = {.COLOR_ATTACHMENT_OUTPUT},
		dstAccessMask = {.COLOR_ATTACHMENT_READ},
		newLayout = .COLOR_ATTACHMENT_OPTIMAL,
		image = swapchain_image,
		subresourceRange = vk.ImageSubresourceRange {
			aspectMask = {.COLOR},
			baseMipLevel = 0,
			levelCount = 1,
			baseArrayLayer = 0,
			layerCount = 1,
		},
	}
	render_dependency_info := vk.DependencyInfo {
		sType                   = .DEPENDENCY_INFO,
		imageMemoryBarrierCount = 1,
		pImageMemoryBarriers    = &render_image_barrier,
	}
	vk.CmdPipelineBarrier2(command_buffer, &render_dependency_info)
	swapchain_view := renderer.swapchain_views[renderer.image_index]
	color_attachment := vk.RenderingAttachmentInfo {
		sType       = .RENDERING_ATTACHMENT_INFO,
		imageView   = swapchain_view,
		imageLayout = .COLOR_ATTACHMENT_OPTIMAL,
	}
	rendering_info := vk.RenderingInfo {
		sType                = .RENDERING_INFO,
		renderArea           = renderer.scissor,
		layerCount           = 1,
		colorAttachmentCount = 1,
		pColorAttachments    = &color_attachment,
	}
	vk.CmdBeginRendering(command_buffer, &rendering_info)
	vk.CmdBindPipeline(command_buffer, nil, renderer.pipeline)
	vk.CmdDraw(command_buffer, 3, 1, 0, 0)
	vk.CmdEndRendering(command_buffer)

	// transition image to present target
	present_image_barrier := vk.ImageMemoryBarrier2 {
		sType = .IMAGE_MEMORY_BARRIER_2,
		srcStageMask = {.COLOR_ATTACHMENT_OUTPUT},
		srcAccessMask = {.COLOR_ATTACHMENT_READ},
		oldLayout = .COLOR_ATTACHMENT_OPTIMAL,
		newLayout = .PRESENT_SRC_KHR,
		image = swapchain_image,
		subresourceRange = vk.ImageSubresourceRange {
			aspectMask = {.COLOR},
			baseMipLevel = 0,
			levelCount = 1,
			baseArrayLayer = 0,
			layerCount = 1,
		},
	}
	present_dependency_info := vk.DependencyInfo {
		sType                   = .DEPENDENCY_INFO,
		imageMemoryBarrierCount = 1,
		pImageMemoryBarriers    = &present_image_barrier,
	}
	vk.CmdPipelineBarrier2(command_buffer, &present_dependency_info)
	result = vk.EndCommandBuffer(command_buffer)
	if result != .SUCCESS {
		fmt.panicf("vk.EndCommandBuffer failed:", result)
	}
	wait_dst_stage_mask: vk.PipelineStageFlags = {.COLOR_ATTACHMENT_OUTPUT}
	image_semaphore := renderer.image_semaphores[renderer.image_index]
	submit_info := vk.SubmitInfo {
		sType                = .SUBMIT_INFO,
		waitSemaphoreCount   = 1,
		pWaitSemaphores      = &frame_semaphore,
		pWaitDstStageMask    = &wait_dst_stage_mask,
		commandBufferCount   = 1,
		pCommandBuffers      = &command_buffer,
		signalSemaphoreCount = 1,
		pSignalSemaphores    = &image_semaphore,
	}
	result = vk.QueueSubmit(renderer.queue, 1, &submit_info, fence)
	if result != .SUCCESS {
		fmt.panicf("vk.QueueSubmit failed:", result)
	}
	present_info := vk.PresentInfoKHR {
		sType              = .PRESENT_INFO_KHR,
		waitSemaphoreCount = 1,
		pWaitSemaphores    = &image_semaphore,
		swapchainCount     = 1,
		pSwapchains        = &renderer.swapchain,
		pImageIndices      = &renderer.image_index,
		pResults           = &result,
	}
	result = vk.QueuePresentKHR(renderer.queue, &present_info)
	if result != .SUCCESS {
		fmt.panicf("vk.QueuePresentKHR failed:", result)
	}
	renderer.frame_index = (renderer.frame_index + 1) % MAX_FRAMES_IN_FLIGHT
}

get_queue_family_index :: proc(
	physical_device: vk.PhysicalDevice,
	queue_flags: vk.QueueFlags,
) -> (
	queue_family_index: u32,
	err: bool,
) {
	queue_family_count: u32
	vk.GetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, nil)
	queue_families := make([]vk.QueueFamilyProperties, queue_family_count)
	vk.GetPhysicalDeviceQueueFamilyProperties(
		physical_device,
		&queue_family_count,
		raw_data(queue_families),
	)
	for i in 0 ..< queue_family_count {
		queue_family := queue_families[i]
		suitable_queue_family := true
		for queue_flag in queue_flags {
			if !(queue_flag in queue_family.queueFlags) {
				suitable_queue_family = false
				break
			}
		}
		if suitable_queue_family {
			return i, false
		}
	}
	return 0, true
}
