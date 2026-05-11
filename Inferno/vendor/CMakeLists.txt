cmake_minimum_required(VERSION 3.31)
project(vendor LANGUAGES CXX)

add_subdirectory(glm)

add_library(spdlog INTERFACE)

target_include_directories(spdlog INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/spdlog/include
)


add_library(tinyobjloader INTERFACE)
target_include_directories(tinyobjloader INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/tinyobjloader
)

add_library(tinygltf INTERFACE)
target_include_directories(tinygltf INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/tinygltf
)

add_library(VMA INTERFACE)
target_include_directories(VMA INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/VMA/include
)

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
)


