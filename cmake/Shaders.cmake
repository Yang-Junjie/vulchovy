# Compiles Slang shaders into SPIR-V and exposes the output directory to C++.
#
# Each shader is given as "<source>|<entry>|<stage>|<output>". Sources are read
# from "<project>/shaders" and written to "<binary dir>/shaders". The target is
# defined with VULCHOVY_SHADER_DIR so the runtime can load them from the build
# tree.

function(vulchovy_find_slangc out_var)
    get_filename_component(vulkan_bin_dir "${Vulkan_GLSLC_EXECUTABLE}" DIRECTORY)
    find_program(VULCHOVY_SLANGC_EXECUTABLE NAMES slangc HINTS "${vulkan_bin_dir}")
    set(${out_var} "${VULCHOVY_SLANGC_EXECUTABLE}" PARENT_SCOPE)
endfunction()

function(vulchovy_add_slang_shaders target)
    vulchovy_find_slangc(SLANGC_EXECUTABLE)
    if(NOT SLANGC_EXECUTABLE)
        message(FATAL_ERROR "slangc not found; install the Vulkan SDK")
    endif()

    set(shader_source_dir "${CMAKE_SOURCE_DIR}/shaders")
    set(shader_output_dir "${CMAKE_CURRENT_BINARY_DIR}/shaders")
    file(MAKE_DIRECTORY "${shader_output_dir}")

    # Any shader file may be included by another, so every one is a dependency.
    file(GLOB_RECURSE shader_headers "${shader_source_dir}/*.slang")

    set(outputs)
    foreach(shader IN LISTS ARGN)
        string(REPLACE "|" ";" fields "${shader}")
        list(GET fields 0 source)
        list(GET fields 1 entry)
        list(GET fields 2 stage)
        list(GET fields 3 output)

        add_custom_command(
            OUTPUT "${shader_output_dir}/${output}"
            COMMAND "${SLANGC_EXECUTABLE}" "${shader_source_dir}/${source}"
                    -target spirv -entry "${entry}" -stage "${stage}"
                    -fvk-use-entrypoint-name
                    -I "${shader_source_dir}"
                    -o "${shader_output_dir}/${output}"
            DEPENDS "${shader_source_dir}/${source}" ${shader_headers}
            COMMENT "Compiling ${source} (${entry})"
            VERBATIM
        )
        list(APPEND outputs "${shader_output_dir}/${output}")
    endforeach()

    add_custom_target(${target}_shaders DEPENDS ${outputs})
    add_dependencies(${target} ${target}_shaders)

    target_compile_definitions(${target} PRIVATE
        "VULCHOVY_SHADER_DIR=\"${shader_output_dir}\"")
endfunction()
