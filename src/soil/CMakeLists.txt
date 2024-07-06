add_executable(soil)

target_sources(soil
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/soil.m.cpp
)

target_include_directories(soil
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(soil
    PRIVATE
        cppext
        imgui_impl
        vkrndr
    PRIVATE
        boost::boost
        EnTT::EnTT
        joltphysics::joltphysics
        glm::glm
        SDL2::SDL2main
        spdlog::spdlog
    PRIVATE
        project-options
)

set_property(TARGET soil 
    PROPERTY 
        VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

