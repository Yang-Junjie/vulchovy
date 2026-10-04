cmake_minimum_required(VERSION 3.24)
project(vulchovy VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(VULCHOVY_VALIDATION "Enable Vulkan validation layers" ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Some fetched dependencies still declare cmake_minimum_required below 3.5,
# which CMake 4 rejects outright.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)

# --- vulcao (vendored Vulkan wrapper) ---------------------------------------

set(VULCAO_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(VULCAO_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(VULCAO_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/vulcao)

# --- third party (fetched) ---------------------------------------------------

include(FetchContent)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_X11 ON CACHE BOOL "" FORCE)
FetchContent_Declare(glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
    GIT_SHALLOW TRUE)

set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLM_BUILD_INSTALL OFF CACHE BOOL "" FORCE)
set(GLM_ENABLE_CXX_20 ON CACHE BOOL "" FORCE)
FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
    GIT_SHALLOW TRUE)

FetchContent_MakeAvailable(glfw glm)

# --- vulchovy_core (renderer library) ---------------------------------------

add_library(vulchovy_core STATIC
    src/app/application.cpp
    src/camera/orbit_camera_controller.cpp
    src/camera/perspective_camera.cpp
    src/platform/input.cpp
    src/platform/window.cpp
    src/render/display_pass.cpp
    src/render/integrators/compute/compute_integrator.cpp
    src/render/integrators/compute/whitted.cpp
    src/render/integrators/ray_query/ray_query_integrator.cpp
    src/render/integrators/ray_query/debug_normals.cpp
    src/render/integrators/ray_query/path_tracer.cpp
    src/render/integrators/ray_query/whitted.cpp
    src/scene/gpu_scene.cpp
    src/scene/mesh.cpp
    src/scene/scene.cpp
    src/scene/scene_descriptors.cpp
)

target_include_directories(vulchovy_core PUBLIC src)

target_compile_features(vulchovy_core PUBLIC cxx_std_20)

target_compile_definitions(vulchovy_core PUBLIC
    GLM_FORCE_RADIANS
    GLM_FORCE_DEPTH_ZERO_TO_ONE)

target_compile_definitions(vulchovy_core PRIVATE
    "VULCHOVY_VALIDATION=$<BOOL:${VULCHOVY_VALIDATION}>")

target_link_libraries(vulchovy_core PUBLIC
    vulcao
    glm::glm
)

target_link_libraries(vulchovy_core PRIVATE glfw)

target_compile_options(vulchovy_core PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic>)

include(cmake/Shaders.cmake)
vulchovy_add_slang_shaders(vulchovy_core
    "integrators/ray_query/whitted.slang|computeMain|compute|whitted.comp.spv"
    "integrators/ray_query/debug_normals.slang|computeMain|compute|debug_normals.comp.spv"
    "integrators/ray_query/path_tracer.slang|computeMain|compute|path_tracer.comp.spv"
    "integrators/compute/whitted.slang|computeMain|compute|whitted_compute.comp.spv"
    "display.slang|vertexMain|vertex|display.vert.spv"
    "display.slang|fragmentMain|fragment|display.frag.spv"
)

# --- vulchovy (executable) ---------------------------------------------------

add_executable(vulchovy src/main.cpp)

target_link_libraries(vulchovy PRIVATE vulchovy_core)

target_compile_options(vulchovy PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic>)

