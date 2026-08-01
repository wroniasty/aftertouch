# Shared generate/diff for tracegen, tracediff, and golden-trace tests.
# Header-only over at_core — no SDL (A6 / wall 1).
add_library(at_tracekit INTERFACE)
target_include_directories(at_tracekit INTERFACE include)
target_link_libraries(at_tracekit INTERFACE at_core)
