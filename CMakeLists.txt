cmake_minimum_required(VERSION 3.24)
project(aftertouch CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
    add_compile_options(/W4 /permissive-)
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

# --- project ----------------------------------------------------------------
add_subdirectory(src/core)
add_subdirectory(src/app)

enable_testing()
add_subdirectory(tests)

# --- wall check -------------------------------------------------------------
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_FOUND)
    add_custom_target(check-walls
        COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_SOURCE_DIR}/tools/check_walls.py
                ${CMAKE_SOURCE_DIR}
        COMMENT "Checking architectural walls")
    add_dependencies(aftertouch check-walls)
endif()
