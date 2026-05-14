cmake_minimum_required(VERSION 3.31)
project(Inferno LANGUAGES CXX)

find_package(Vulkan REQUIRED)
find_package(glfw3 CONFIG REQUIRED)

add_library(Inferno
    SHARED
        # MAIN ENGINE COMPONENTS
        src/Inferno/Core/Application.h
        src/Inferno/Core/Application.cpp
        src/Inferno/Core/Log.h
        src/Inferno/Core/Log.cpp
        src/Inferno/Core/Layer.h
        src/Inferno/Core/LayerStack.h
        src/Inferno/Core/LayerStack.cpp
        src/Inferno/Core/Window.h
        src/Inferno/Core/Window.cpp
        # ENTITY COMPONENTS SYSTEMS
        src/Inferno/ECS/Component.h
        src/Inferno/ECS/Component.cpp
        src/Inferno/ECS/Entity.h
        src/Inferno/ECS/Entity.cpp
        #Renderer
        # EVENTS
        src/Inferno/Events/Event.h
        src/Inferno/Events/ApplicationEvent.h
        src/Inferno/Events/KeyEvent.h
        src/Inferno/Events/MouseEvent.h
        # CORE UTILS
        src/Inferno/Utils/DeltaTime.h
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
