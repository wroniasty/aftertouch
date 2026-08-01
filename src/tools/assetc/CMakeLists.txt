add_executable(assetc
    main.cpp
    swos_sprites.cpp
    swos_import.cpp
    image.cpp
    pitch.cpp
    pack_writer.cpp
)

target_include_directories(assetc PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/third_party/stb)

# at_assets only. No SDL, no ImGui, no at_core: the importer is an offline tool that
# knows about pixels and nothing about the simulation.
target_link_libraries(assetc PRIVATE at_assets)

# stb_image is third-party and warns under /W4; do not let it drown ours.
if(MSVC)
    set_source_files_properties(image.cpp PROPERTIES COMPILE_OPTIONS "/W3")
else()
    set_source_files_properties(image.cpp PROPERTIES
        COMPILE_OPTIONS "-Wno-unused-but-set-variable")
endif()
