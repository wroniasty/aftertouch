add_executable(core_tests
    core/test_fixed.cpp
    core/test_trace.cpp
)
target_link_libraries(core_tests PRIVATE at_core doctest)

# Test builds turn on the fixed-point overflow assertions. Release builds wrap
# instead, identically on every platform -- see core/fixed.hpp.
target_compile_definitions(core_tests PRIVATE AT_FIXED_CHECKED)

add_test(NAME core_tests COMMAND core_tests)

# Asset container format. Links at_assets only -- like core_tests, it builds and runs
# on a machine with no graphics stack.
add_executable(asset_tests assets/test_asset_pack.cpp)
target_link_libraries(asset_tests PRIVATE at_assets doctest)
add_test(NAME asset_tests COMMAND asset_tests)

# The wall check has its own tests. A check that has never been seen to fail is
# indistinguishable from one that cannot fail.
if(Python3_FOUND)
    add_test(NAME wall_check_tests
             COMMAND ${Python3_EXECUTABLE}
                     ${CMAKE_SOURCE_DIR}/tools/test_check_walls.py)
endif()
