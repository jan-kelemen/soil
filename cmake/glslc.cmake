if (NOT GLSLC_EXE)
    find_program(GLSLC_EXE NAMES glslc REQUIRED)
    message(STATUS "glslc found: ${GLSLC_EXE}")
endif()

function(compile_shader)
    set(options)
    set(oneValueArgs SHADER SPIRV)
    set(multiValueArgs)
    cmake_parse_arguments(
        GLSLC_SHADER "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN}
    )

    add_custom_command(
        OUTPUT ${GLSLC_SHADER_SPIRV}
        COMMAND ${GLSLC_EXE} ${GLSLC_SHADER_SHADER} -o ${GLSLC_SHADER_SPIRV}
        DEPENDS ${GLSLC_SHADER_SHADER}
    )
endfunction()
