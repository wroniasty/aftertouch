# Runtime asset serving (IAssetSource). No SDL — C1 links this and draws.
add_library(at_asset_source STATIC
    asset_source.cpp
    placeholder_assets.cpp
    imported_assets.cpp
    kit_palette.cpp
)
target_include_directories(at_asset_source PUBLIC ${CMAKE_SOURCE_DIR}/src/app)
target_link_libraries(at_asset_source PUBLIC at_assets at_core)

# The one piece of SDL drawing that is not match-specific: indexed sprite ->
# texture, with the index-0 key colour and the anchor convention. The shell and
# src/tools/sprite_viewer both draw the same art, so they draw it through the
# same function rather than each keeping a copy of those two rules.
add_library(at_render_sdl STATIC indexed_sprite.cpp)
target_include_directories(at_render_sdl PUBLIC ${CMAKE_SOURCE_DIR}/src/app)
target_link_libraries(at_render_sdl PUBLIC at_asset_source SDL3::SDL3)
