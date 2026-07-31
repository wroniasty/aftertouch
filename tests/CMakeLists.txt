add_executable(core_tests core/test_fixed.cpp)
target_link_libraries(core_tests PRIVATE at_core doctest)
add_test(NAME core_tests COMMAND core_tests)
