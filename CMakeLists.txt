cmake_minimum_required(VERSION 3.20)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_C_COMPILER clang)
set(CMAKE_BUILD_TYPE Debug)
project(Ronin)
add_executable(main src/main.cpp src/engine.cpp)
target_include_directories(main PUBLIC include)
if (APPLE)
  target_include_directories(main PRIVATE /opt/homebrew/include/glm)
endif()
target_include_directories(main PRIVATE vendor/stb vendor/tinyobjloader)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(vendor/glfw)
find_package(Vulkan REQUIRED)

target_link_libraries(main PRIVATE glfw Vulkan::Vulkan)

find_program(SLANGC_EXECUTABLE NAMES slangc slangc.exe REQUIRED)
function(compile_shader shader)
  add_custom_command(
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/shaders/${shader}.spv"
        COMMAND ${CMAKE_COMMAND} -E make_directory
                "${CMAKE_CURRENT_BINARY_DIR}/shaders"
        COMMAND "${SLANGC_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${shader}.slang"
                -target spirv
                -profile spirv_1_4
                -emit-spirv-directly
                -fvk-use-entrypoint-name
                -entry vertMain
        -entry fragMain
        -o "${CMAKE_CURRENT_BINARY_DIR}/shaders/${shader}.spv"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${shader}.slang"
    )
  add_custom_target(${shader}
      DEPENDS
      "${CMAKE_CURRENT_BINARY_DIR}/shaders/${shader}.spv"
    )
  add_dependencies(main ${shader})
endfunction()
compile_shader(shader)
compile_shader(line_shader)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  if (MSVC)
    target_compile_options(main PRIVATE
          /WX
          /W4
          /permissive-
        )
  else()
    # target_compile_options(main PRIVATE
    #       -Wall
    #       -Wextra
    #       -Werror
    #       -Wpedantic
    #     )
  endif()
endif()
