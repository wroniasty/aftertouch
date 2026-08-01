cmake_minimum_required(VERSION 3.24)
project(aftertouch CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
    # /utf-8: treat sources AND the execution charset as UTF-8. Without this,
    # MSVC reads BOM-less files as the system code page and mangles every
    # non-ASCII literal (the diacritic smoke test in main.cpp becomes mojibake).
    add_compile_options(/W4 /permissive- /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# --- SDL3 -------------------------------------------------------------------
set(SDL_SHARED       ON  CACHE BOOL "" FORCE)
set(SDL_STATIC       OFF CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS        OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES     OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/SDL EXCLUDE_FROM_ALL)

# --- Dear ImGui -------------------------------------------------------------
include(cmake/imgui.cmake)

# --- doctest ----------------------------------------------------------------
add_library(doctest INTERFACE)
target_include_directories(doctest INTERFACE third_party/doctest)

# --- python -----------------------------------------------------------------
# Located before the subdirectories so tests/ can see Python3_FOUND.
find_package(Python3 COMPONENTS Interpreter QUIET)

# --- project ----------------------------------------------------------------
add_subdirectory(src/core)
add_subdirectory(src/assets)
add_subdirectory(src/app)
add_subdirectory(src/tools)

enable_testing()
add_subdirectory(tests)

# --- wall check -------------------------------------------------------------
if(Python3_FOUND)
    add_custom_target(check-walls
        COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_SOURCE_DIR}/tools/check_walls.py
                ${CMAKE_SOURCE_DIR}
        COMMENT "Checking architectural walls")
    add_dependencies(aftertouch check-walls)
endif()
