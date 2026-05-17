cmake_minimum_required(VERSION 3.31)
project(vendor LANGUAGES CXX)

add_subdirectory(glm)

# ------------------------
# spdlog
# ------------------------

add_library(spdlog INTERFACE)

target_include_directories(spdlog INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/spdlog/include
)

# ------------------------
# tinyobjloader
# ------------------------

add_library(tinyobjloader INTERFACE)

target_include_directories(tinyobjloader INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/tinyobjloader
)

# ------------------------
# tinygltf
# ------------------------

add_library(tinygltf INTERFACE)

target_include_directories(tinygltf INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/tinygltf
)

# ------------------------
# Vulkan Memory Allocator
# ------------------------

add_library(VMA INTERFACE)

target_include_directories(VMA INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/VMA/include
)
# ------------------------
# KTX
# ------------------------

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

set(KTX_FEATURE_STATIC_LIBRARY ON CACHE BOOL "" FORCE)
set(KTX_FEATURE_TESTS OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_TOOLS OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_GL_UPLOAD OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_VK_UPLOAD ON CACHE BOOL "" FORCE)

add_subdirectory(ktx)

# ------------------------
# ImGui
# ------------------------

add_library(imgui STATIC
    imgui/imgui.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp

    imgui/backends/imgui_impl_glfw.cpp
    imgui/backends/imgui_impl_vulkan.cpp
)

target_include_directories(imgui PUBLIC
    imgui
    imgui/backends
)

target_link_libraries(imgui PUBLIC
    glfw
    Vulkan::Vulkan
)
