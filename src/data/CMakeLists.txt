# Game data format and the fictional default league (A5).
#
# Header-only and links at_core only for MatchState / PlayerAttrs. Same reason
# at_assets is an INTERFACE library: the format is shared by tests, tools and the
# shell, and belongs to none of them alone. Stays out of match tick code — the
# engine sees the projection ApplyKickoff wrote into MatchState.
add_library(at_data INTERFACE)
target_include_directories(at_data INTERFACE include)
target_link_libraries(at_data INTERFACE at_core)
