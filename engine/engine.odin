package engine
import "vendor:glfw"
import vk "vendor:vulkan"

WIDTH :: 1280
HEIGHT :: 720
TITLE :: "Ronin"
ENABLE_VALIDATION_LAYER :: true
MAX_FRAMES_IN_FLIGHT :: 2

Engine :: struct {
	renderer: Renderer,
}

init_engine :: proc(engine: ^Engine) {
	init_renderer(&engine.renderer)
}

run_engine :: proc(engine: ^Engine) {
	for !glfw.WindowShouldClose(engine.renderer.window) {
		glfw.PollEvents()

		render_frame(&engine.renderer)
	}
}
