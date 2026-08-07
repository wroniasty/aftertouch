add_executable(sprite_viewer main.cpp)

target_include_directories(sprite_viewer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(sprite_viewer PRIVATE
    at_core at_asset_source at_render_sdl imgui SDL3::SDL3)

target_compile_definitions(sprite_viewer PRIVATE
    AT_ASSET_DIR="${CMAKE_SOURCE_DIR}/assets")

if(WIN32)
    add_custom_command(TARGET sprite_viewer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:sprite_viewer> $<TARGET_FILE_DIR:sprite_viewer>
        COMMAND_EXPAND_LISTS)
endif()
