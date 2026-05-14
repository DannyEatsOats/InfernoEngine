cmake_minimum_required(VERSION 3.31)
project(Inferno LANGUAGES CXX)

find_package(Vulkan REQUIRED)
find_package(glfw3 CONFIG REQUIRED)

add_library(Inferno
    SHARED
        # MAIN ENGINE COMPONENTS
        src/Inferno/Application.h
        src/Inferno/Application.cpp
        src/Inferno/Log.h
        src/Inferno/Log.cpp
        src/Inferno/Layer.h
        src/Inferno/LayerStack.h
        src/Inferno/LayerStack.cpp
        src/Inferno/Window.h
        src/Inferno/Window.cpp
        src/Inferno/Component.h
        src/Inferno/Entity.h
        src/Inferno/Entity.cpp
        #Renderer
        # EVENTS
        src/Inferno/Events/Event.h
        src/Inferno/Events/ApplicationEvent.h
        src/Inferno/Events/KeyEvent.h
        src/Inferno/Events/MouseEvent.h
        # CORE UTILS
        src/Inferno/Core/DeltaTime.h
        # VENDOR
        vendor/stb_image_impl/stb_image.cpp
)

set_target_properties(Inferno PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

target_include_directories(Inferno
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/stb_image
)

add_subdirectory(vendor)

target_link_libraries(Inferno
    PUBLIC
        Vulkan::Vulkan
        glfw
        glm::glm
        spdlog
        tinyobjloader
        tinygltf
        VMA
        imgui
)

target_compile_definitions(Inferno
    PRIVATE
        GLFW_INCLUDE_NONE
)

set_target_properties(Inferno PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY ${OUTPUT_DIR}/${CMAKE_BUILD_TYPE}/Inferno
        LIBRARY_OUTPUT_DIRECTORY ${OUTPUT_DIR}/${CMAKE_BUILD_TYPE}/Inferno
        RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_DIR}/${CMAKE_BUILD_TYPE}/Inferno
        OUTPUT_NAME Inferno
)

target_precompile_headers(Inferno PRIVATE src/pch.h)
