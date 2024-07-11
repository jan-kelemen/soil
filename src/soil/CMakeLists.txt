add_executable(soil)

target_sources(soil
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/application.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/bullet_adapter.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/bullet_debug_renderer.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/perspective_camera.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/physics_engine.hpp
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/application.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/bullet_adapter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/bullet_debug_renderer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/perspective_camera.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/physics_engine.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/soil.m.cpp
)

target_include_directories(soil
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(soil
    PRIVATE
        cppext
        niku
        vkrndr
    PRIVATE
        boost::boost
        Bullet::Bullet
        EnTT::EnTT
        glm::glm
        SDL2::SDL2main
        spdlog::spdlog
        Vulkan::Loader
    PRIVATE
        project-options
)
add_dependencies(soil shaders)

compile_shader(
    SHADER
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/bullet_debug_line.frag
    SPIRV
        ${CMAKE_CURRENT_BINARY_DIR}/bullet_debug_line.frag.spv
)

compile_shader(
    SHADER
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/bullet_debug_line.vert
    SPIRV
        ${CMAKE_CURRENT_BINARY_DIR}/bullet_debug_line.vert.spv
)

add_custom_target(shaders
    DEPENDS
        ${CMAKE_CURRENT_BINARY_DIR}/bullet_debug_line.frag.spv
        ${CMAKE_CURRENT_BINARY_DIR}/bullet_debug_line.vert.spv
)

set_property(TARGET soil 
    PROPERTY 
        VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

