add_executable(aftertouch
    main.cpp
    platform/window.cpp
    ui_imgui/imgui_backend.cpp
    ui_imgui/fonts.cpp
    ui_imgui/screens/main_menu.cpp
    render/match_renderer.cpp
)

target_include_directories(aftertouch PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(aftertouch PRIVATE at_core imgui SDL3::SDL3)

# Asset path for development builds. Replace with SDL_GetBasePath()
# and a real install layout when you package anything.
target_compile_definitions(aftertouch PRIVATE
    AT_ASSET_DIR="${CMAKE_SOURCE_DIR}/assets")

# Windows: put SDL3.dll next to the executable.
if(WIN32)
    add_custom_command(TARGET aftertouch POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:aftertouch> $<TARGET_FILE_DIR:aftertouch>
        COMMAND_EXPAND_LISTS)
endif()
