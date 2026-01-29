cmake_minimum_required(VERSION 3.31)
project(Game)

add_executable(Game 
    src/main.cpp
)

target_include_directories(Game
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src 
)

target_link_libraries(Game PRIVATE Inferno)

set_target_properties(Game
        PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${OUTPUT_DIR}/${CMAKE_BUILD_TYPE}/Game
        LIBRARY_OUTPUT_DIRECTORY ${OUTPUT_DIR}/${CMAKE_BUILD_TYPE}/Game
        RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_DIR}/${CMAKE_BUILD_TYPE}/Game)

target_compile_definitions(Game
        PRIVATE 
            $<$<CONFIG:Debug>:INFERNO_DEBUG> $<$<CONFIG:Release>:INFERNO_RELEASE>
        )

add_custom_command(
        TARGET Game POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:Game>/assets
)
