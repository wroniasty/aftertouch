# ---------------------------------------------------------------------------
# Core — wall 1: at_core + doctest only. A6 enforces no SDL in the link line.
# ---------------------------------------------------------------------------
add_executable(core_tests
    core/test_fixed.cpp
    core/test_trace.cpp
    core/test_tracekit.cpp
    core/test_golden_trace.cpp
)
target_link_libraries(core_tests PRIVATE at_core at_tracekit doctest)
target_compile_definitions(core_tests PRIVATE
    AT_FIXED_CHECKED
    AT_GOLDEN_DIR="${CMAKE_SOURCE_DIR}/tests/golden"
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
add_executable(asset_tests assets/test_asset_pack.cpp)
target_link_libraries(asset_tests PRIVATE at_assets doctest)
add_test(NAME asset_tests COMMAND asset_tests)

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
