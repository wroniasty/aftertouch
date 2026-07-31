add_library(at_core STATIC
    src/match_engine.cpp
)

target_include_directories(at_core PUBLIC include)

# WALL 1: this target links nothing and must stay that way.
# If you are about to add target_link_libraries here, stop and reconsider.
