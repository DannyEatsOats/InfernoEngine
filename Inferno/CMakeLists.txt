cmake_minimum_required(VERSION 3.31)
project(Inferno LANGUAGES CXX)

find_package(Vulkan REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)

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
        #Renderer
        src/Inferno/Renderer/RenderingContext.h
        src/Inferno/Renderer/RenderingContext.cpp
        src/Inferno/Renderer/Renderer.h
        src/Inferno/Renderer/Renderer.cpp
        src/Inferno/Renderer/Shader.h
        src/Inferno/Renderer/Shader.cpp
        # EVENTS
        src/Inferno/Events/Event.h
        src/Inferno/Events/ApplicationEvent.h
        src/Inferno/Events/KeyEvent.h
        src/Inferno/Events/MouseEvent.h
        # CORE UTILS
        src/Inferno/Core/DeltaTime.h
)

set_target_properties(Inferno PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

target_link_libraries(Inferno
    PUBLIC
        Vulkan::Vulkan
        glfw
        glm::glm
)

target_include_directories(Inferno
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/spdlog/include
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
