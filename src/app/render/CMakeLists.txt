# Runtime asset serving (IAssetSource). No SDL — C1 links this and draws.
add_library(at_asset_source STATIC
    asset_source.cpp
    placeholder_assets.cpp
    imported_assets.cpp
)
target_include_directories(at_asset_source PUBLIC ${CMAKE_SOURCE_DIR}/src/app)
target_link_libraries(at_asset_source PUBLIC at_assets at_core)
