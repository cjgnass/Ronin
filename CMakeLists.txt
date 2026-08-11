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
function(add_slang_shader_target target)
  cmake_parse_arguments(SHADER "" "" "SOURCES" ${ARGN})
  if(NOT SHADER_SOURCES)
    message(FATAL_ERROR "${target}: provide SOURCES")
  endif()
  set(shader_output "${CMAKE_CURRENT_BINARY_DIR}/shaders/shader.spv")
  add_custom_command(
        OUTPUT "${shader_output}"
        COMMAND ${CMAKE_COMMAND} -E make_directory
                "${CMAKE_CURRENT_BINARY_DIR}/shaders"
        COMMAND "${SLANGC_EXECUTABLE}"
                ${SHADER_SOURCES}
                -target spirv
                -profile spirv_1_4
                -emit-spirv-directly
                -fvk-use-entrypoint-name
                -entry vertMain
                -entry fragMain
                -o "${shader_output}"
        DEPENDS ${SHADER_SOURCES}
        COMMENT "Compiling Slang shaders"
        VERBATIM
    )
  add_custom_target(${target} DEPENDS "${shader_output}")
endfunction()
add_slang_shader_target(shaders
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/shaders/shader.slang"
)
add_dependencies(main shaders)

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
