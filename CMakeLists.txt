cmake_minimum_required(VERSION 3.20)
project(TopDownGame CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Vulkan REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLFW REQUIRED glfw3)
pkg_check_modules(GLM REQUIRED glm)

add_executable(topdown
    src/main.cpp
    src/engine/vulkan_core.cpp
    src/engine/renderer.cpp
    src/engine/texture.cpp
    src/game/game.cpp
)

target_include_directories(topdown PRIVATE
    src
    ${CMAKE_CURRENT_BINARY_DIR}
    ${Vulkan_INCLUDE_DIRS}
    ${GLFW_INCLUDE_DIRS}
    ${GLM_INCLUDE_DIRS}
)

target_link_libraries(topdown PRIVATE
    ${Vulkan_LIBRARIES}
    ${GLFW_LIBRARIES}
    ${GLM_LIBRARIES}
    dl
    pthread
)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/sprite.vert.spv
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/sprite.vert
    COMMAND glslc -fshader-stage=vertex ${CMAKE_CURRENT_SOURCE_DIR}/shaders/sprite.vert
        -o ${CMAKE_CURRENT_BINARY_DIR}/sprite.vert.spv
)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/sprite.frag.spv
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/sprite.frag
    COMMAND glslc -fshader-stage=fragment ${CMAKE_CURRENT_SOURCE_DIR}/shaders/sprite.frag
        -o ${CMAKE_CURRENT_BINARY_DIR}/sprite.frag.spv
)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/embedded_shaders.h
    DEPENDS
        ${CMAKE_CURRENT_BINARY_DIR}/sprite.vert.spv
        ${CMAKE_CURRENT_BINARY_DIR}/sprite.frag.spv
    COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_shaders.py
        ${CMAKE_CURRENT_BINARY_DIR}/embedded_shaders.h
        ${CMAKE_CURRENT_BINARY_DIR}/sprite.vert.spv
        ${CMAKE_CURRENT_BINARY_DIR}/sprite.frag.spv
)

add_custom_target(shaders ALL DEPENDS
    ${CMAKE_CURRENT_BINARY_DIR}/sprite.vert.spv
    ${CMAKE_CURRENT_BINARY_DIR}/sprite.frag.spv
    ${CMAKE_CURRENT_BINARY_DIR}/embedded_shaders.h
)

add_dependencies(topdown shaders)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/assets DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
