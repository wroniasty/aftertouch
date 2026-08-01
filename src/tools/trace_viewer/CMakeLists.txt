add_executable(trace_viewer main.cpp)

target_link_libraries(trace_viewer PRIVATE at_core at_tracekit imgui SDL3::SDL3)

if(WIN32)
    add_custom_command(TARGET trace_viewer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:trace_viewer> $<TARGET_FILE_DIR:trace_viewer>
        COMMAND_EXPAND_LISTS)
endif()
