# ---------------------------------------------------------------------------
# Core — wall 1: at_core + doctest only. A6 enforces no SDL in the link line.
# ---------------------------------------------------------------------------
add_executable(core_tests
    core/test_fixed.cpp
    core/test_trace.cpp
    core/test_tracekit.cpp
    core/test_tracediff.cpp
    core/test_tracegen.cpp
    core/test_hil2.cpp
    core/test_corpus.cpp
    core/test_golden_trace.cpp
    core/test_angle.cpp
    core/test_trig_tables.cpp
    core/test_kernel.cpp
    core/test_rng.cpp
    core/test_hash.cpp
    core/test_determinism.cpp
    core/test_match_state.cpp
    core/test_match_clock.cpp
    core/test_game_state.cpp
    core/test_oop_classify.cpp
    core/test_full_match.cpp
    core/test_ball_friction.cpp
    core/test_ball_bounce.cpp
    core/test_ball_barrier.cpp
    core/test_ball_predictor.cpp
    core/test_ball_oop_wire.cpp
    core/test_ball_trajectory.cpp
    core/test_player_speed.cpp
    core/test_player_dest.cpp
    core/test_player_boundary.cpp
    core/test_player_integrate.cpp
    core/test_player_turn_flags.cpp
    core/test_offball_dest.cpp
    core/test_move_players.cpp
)
target_link_libraries(core_tests PRIVATE at_core at_tracekit doctest)
target_compile_definitions(core_tests PRIVATE
    AT_FIXED_CHECKED
    AT_GOLDEN_DIR="${CMAKE_SOURCE_DIR}/tests/golden"
    AT_CORPUS_DIR="${CMAKE_SOURCE_DIR}/tests/corpus"
)
add_test(NAME core_tests COMMAND core_tests)

# Fail configure if someone links SDL into core_tests.
get_target_property(_core_test_libs core_tests LINK_LIBRARIES)
if(_core_test_libs MATCHES "SDL")
    message(FATAL_ERROR
        "core_tests must not link SDL (A6 / PLAN.md wall 1). Linked: ${_core_test_libs}")
endif()

# ---------------------------------------------------------------------------
# Asset + data format suites (no SDL)
# ---------------------------------------------------------------------------
add_executable(asset_tests
    assets/test_asset_pack.cpp
    assets/test_placeholder_parity.cpp
)
target_link_libraries(asset_tests PRIVATE at_assets at_asset_source doctest)
target_compile_definitions(asset_tests PRIVATE
    AT_PLACEHOLDER_DIR="${CMAKE_SOURCE_DIR}/assets/placeholder"
)
add_test(NAME asset_tests COMMAND asset_tests)

# Regen committed placeholder packs: cmake --build --target gen-placeholder
if(Python3_FOUND)
    add_custom_target(gen-placeholder
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/gen_placeholder.py
        COMMENT "Regenerating assets/placeholder/*"
    )
endif()

add_executable(data_tests data/test_game_data.cpp)
target_link_libraries(data_tests PRIVATE at_data doctest)
add_test(NAME data_tests COMMAND data_tests)

# ---------------------------------------------------------------------------
# Harness — null UI, season runner, offscreen helpers (no SDL)
# ---------------------------------------------------------------------------
add_executable(harness_tests
    harness/test_null_ui.cpp
    harness/test_season_runner.cpp
    harness/test_offscreen.cpp
)
target_include_directories(harness_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src/app
    ${CMAKE_SOURCE_DIR}/tests
)
target_link_libraries(harness_tests PRIVATE at_core at_data doctest)
add_test(NAME harness_tests COMMAND harness_tests)

# ---------------------------------------------------------------------------
# Fuzz entry points (deterministic mutation suite; libFuzzer optional via AT_FUZZ)
# ---------------------------------------------------------------------------
add_executable(fuzz_tests fuzz/fuzz_containers.cpp)
target_link_libraries(fuzz_tests PRIVATE at_assets at_data doctest)
add_test(NAME fuzz_tests COMMAND fuzz_tests)

# ---------------------------------------------------------------------------
# Wall check
# ---------------------------------------------------------------------------
if(Python3_FOUND)
    add_test(NAME wall_check_tests
             COMMAND ${Python3_EXECUTABLE}
                     ${CMAKE_SOURCE_DIR}/tools/test_check_walls.py)
endif()

# Regen helper: cmake --build --target gen-golden
add_custom_target(gen-golden
    COMMAND tracegen --ticks 100 --seed 0xA5A50001
            --out ${CMAKE_SOURCE_DIR}/tests/golden/kickoff.attr
    DEPENDS tracegen
    COMMENT "Regenerating tests/golden/kickoff.attr"
)

# Corpus: input logs + engine/reference ATTR + hash chains (stub oracle).
add_custom_target(gen-corpus
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/reference/record_corpus.py
            --tracegen $<TARGET_FILE:tracegen>
    DEPENDS tracegen
    COMMENT "Regenerating tests/corpus/*"
)

if(Python3_FOUND)
    add_test(NAME corpus_python
             COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/test_corpus.py)
endif()
