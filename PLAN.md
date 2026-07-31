# PLAN.md

Project codename: **aftertouch**

A top-down football game with a deliberately faithful arcade match engine and a
deep management layer. This document covers bootstrapping only: toolchain, layout,
architectural rules, and the first runnable milestone.

Rename the codename before you get attached to it. Do not use "SWOS" or "Sensible"
anywhere in the repo, the binary, or the assets.

---

## 0. Non-negotiables

These three rules shape every decision below. Violating any of them is expensive to
undo later, so they are stated first.

### Rule 1: the simulation knows nothing about the outside world

`src/core/` is pure C++. No SDL. No ImGui. No file I/O. No time. No randomness that
is not seeded and passed in. No floating point.

It must be compilable, testable, and runnable headless with zero graphics
dependencies. This is what lets you diff traces against a reference implementation,
unit test the physics, and later simulate every match in every league at speed
instead of faking results with a statistical model.

### Rule 2: the UI toolkit lives behind one door

ImGui is a bootstrapping decision, not a commitment. `#include "imgui.h"` may appear
in exactly one directory: `src/app/ui_imgui/`. Nowhere else.

The abstraction is at **screen** granularity, not widget granularity. Do not write
`IUiBackend::Button()`. That produces an ImGui-shaped interface that leaks ImGui's
model everywhere and buys you nothing. Write `IUiBackend::DrawScreen(ScreenId, AppModel&)
-> Intent` and let each backend do whatever it likes inside.

Replacing the UI later means writing a second implementation of one interface with
about six methods, in a new directory, and flipping a factory. That is the whole
migration path, and it stays open for free as long as you respect the wall.

### Rule 3: fixed timestep from commit one

The match engine steps at exactly 50 Hz, driven by an accumulator, decoupled from
render rate. This is in place before the engine does anything at all. Retrofitting a
fixed timestep onto a variable-step codebase is a rewrite.

All three rules are enforced by a script in section 7. Wire it up early.

---

## 1. Toolchain setup

### Windows

1. **Visual Studio 2022** with the "Desktop development with C++" workload.
   This gives you MSVC, the Windows SDK, CMake and Ninja.
2. **Git for Windows**: https://git-scm.com/download/win
3. Verify from a "Developer PowerShell for VS 2022" prompt:
   ```
   cmake --version    # want 3.24+
   ninja --version
   git --version
   cl
   ```

If you prefer working outside Visual Studio, CLion and VS Code both drive CMake
presets fine. VS Code needs the C/C++ and CMake Tools extensions.

### macOS

1. Xcode command line tools:
   ```
   xcode-select --install
   ```
2. Homebrew, then:
   ```
   brew install cmake ninja git
   ```
3. Verify:
   ```
   cmake --version    # want 3.24+
   clang++ --version
   ```

### Shared

Both machines need the same CMake presets and the same submodule commits, so the
build is reproducible across them. Do not let the two diverge. Commit
`CMakePresets.json` and the submodule pins.

C++20. Both MSVC 19.3x and AppleClang 15+ handle everything used here.

---

## 2. Repository bootstrap

```bash
mkdir aftertouch && cd aftertouch
git init
```

### Vendored dependencies

Everything is a git submodule under `third_party/`. Nothing is installed system-wide,
nothing comes from a package manager. This is what keeps Windows and macOS honest and
what stops a dependency update from silently changing your physics.

```bash
mkdir third_party

git submodule add https://github.com/libsdl-org/SDL.git third_party/SDL
git submodule add https://github.com/ocornut/imgui.git third_party/imgui
git submodule add https://github.com/doctest/doctest.git third_party/doctest
```

Now pin each one to a release tag rather than tracking a branch:

```bash
cd third_party/SDL
git tag --list 'release-3.*' | sort -V | tail -5     # pick the newest stable
git checkout release-3.X.Y
cd ../..

cd third_party/imgui
git tag --list 'v1.*' | sort -V | tail -5            # pick the newest stable
git checkout v1.XX.X
cd ../..

cd third_party/doctest
git checkout v2.4.11
cd ../..

git add third_party .gitmodules
git commit -m "Vendor SDL3, Dear ImGui, doctest"
```

Record the exact tags you picked in this file, right here, so future-you knows what
changed when something breaks:

```
SDL:     release-3.?.?
imgui:   v1.??.?
doctest: v2.4.11
```

**Pin ImGui deliberately.** The font system was reworked in the 1.92 line, so the
crispness configuration in section 6 may need adjusting depending on which tag you
land on. Check `imgui.h` and `imgui_draw.cpp` against what is written here rather
than assuming.

Cloning fresh on the other machine:

```bash
git clone --recurse-submodules <url>
# or, if you already cloned:
git submodule update --init --recursive
```

### .gitignore

```gitignore
build/
out/
.cache/
.vs/
.vscode/
.idea/
*.user
.DS_Store
compile_commands.json
```

---

## 3. Directory structure

```
aftertouch/
├── CMakeLists.txt
├── CMakePresets.json
├── PLAN.md
├── .gitignore
├── .gitmodules
│
├── cmake/
│   └── imgui.cmake              # builds ImGui + its SDL3 backends as a target
│
├── third_party/
│   ├── SDL/                     # submodule
│   ├── imgui/                   # submodule
│   └── doctest/                 # submodule
│
├── assets/
│   ├── fonts/                   # TTF for the UI
│   └── sprites/                 # later
│
├── src/
│   ├── core/                    # ===== WALL 1 =====
│   │   ├── CMakeLists.txt       # links NOTHING
│   │   ├── include/core/
│   │   │   ├── fixed.hpp
│   │   │   ├── match_input.hpp
│   │   │   ├── match_state.hpp
│   │   │   └── match_engine.hpp
│   │   └── src/
│   │       └── match_engine.cpp
│   │
│   ├── app/
│   │   ├── CMakeLists.txt
│   │   ├── main.cpp             # entry point, owns the loop
│   │   ├── app_model.hpp        # everything the UI may read/write
│   │   │
│   │   ├── platform/            # SDL lives here and in render/ and ui_imgui/
│   │   │   ├── window.hpp
│   │   │   └── window.cpp       # init, renderer, logical presentation switching
│   │   │
│   │   ├── ui/                  # ===== WALL 2 (the door) =====
│   │   │   ├── ui_backend.hpp   # abstract interface, NO imgui.h
│   │   │   ├── screen_id.hpp
│   │   │   └── intent.hpp
│   │   │
│   │   ├── ui_imgui/            # the ONLY place imgui.h may be included
│   │   │   ├── imgui_backend.hpp
│   │   │   ├── imgui_backend.cpp
│   │   │   ├── fonts.cpp
│   │   │   └── screens/
│   │   │       └── main_menu.cpp
│   │   │
│   │   └── render/              # SDL_Renderer drawing of match state
│   │       ├── match_renderer.hpp
│   │       └── match_renderer.cpp
│   │
│   └── tools/                   # Phase 0 lands here
│       └── .gitkeep             # trace_viewer/ comes next
│
├── tests/
│   ├── CMakeLists.txt
│   └── core/
│       └── test_fixed.cpp
│
└── tools/
    └── check_walls.py           # enforces rules 1 and 2
```

Note `src/tools/` versus `tools/`: the former is game-adjacent C++ executables
(trace viewer, data importer), the latter is repo scripting. Rename one if that
bothers you, but keep them separate.

---

## 4. Build system

### `CMakeLists.txt` (root)

```cmake
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
```

Hanging `check-walls` off the main target means a violation fails your normal build.
That is the point. If it becomes annoying, you are violating the wall too often, not
checking too often.

### `cmake/imgui.cmake`

ImGui ships no CMakeLists, which is deliberate. Define the target yourself:

```cmake
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/third_party/imgui)

add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdlrenderer3.cpp
)

target_include_directories(imgui PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

target_link_libraries(imgui PUBLIC SDL3::SDL3)
```

Keep `imgui_demo.cpp` in the build. It is the best widget reference you will have,
and `ImGui::ShowDemoWindow()` behind a debug key saves hours.

### `src/core/CMakeLists.txt`

```cmake
add_library(at_core STATIC
    src/match_engine.cpp
)

target_include_directories(at_core PUBLIC include)

# WALL 1: this target links nothing and must stay that way.
# If you are about to add target_link_libraries here, stop and reconsider.
```

### `src/app/CMakeLists.txt`

```cmake
add_executable(aftertouch
    main.cpp
    platform/window.cpp
    ui_imgui/imgui_backend.cpp
    ui_imgui/fonts.cpp
    ui_imgui/screens/main_menu.cpp
    render/match_renderer.cpp
)

target_include_directories(aftertouch PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(aftertouch PRIVATE at_core imgui SDL3::SDL3)

# Asset path for development builds. Replace with SDL_GetBasePath()
# and a real install layout when you package anything.
target_compile_definitions(aftertouch PRIVATE
    AT_ASSET_DIR="${CMAKE_SOURCE_DIR}/assets")

# Windows: put SDL3.dll next to the executable.
if(WIN32)
    add_custom_command(TARGET aftertouch POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:aftertouch> $<TARGET_FILE_DIR:aftertouch>
        COMMAND_EXPAND_LISTS)
endif()
```

Do **not** add `WIN32` to `add_executable` yet. You want the console window for
`printf` debugging. Add it when you ship to yourself.

### `tests/CMakeLists.txt`

```cmake
add_executable(core_tests core/test_fixed.cpp)
target_link_libraries(core_tests PRIVATE at_core doctest)
add_test(NAME core_tests COMMAND core_tests)
```

Note what is missing: no SDL, no ImGui. The core test binary builds and runs on a
machine with no graphics stack at all. If that ever stops being true, wall 1 has
been breached.

### `CMakePresets.json`

```json
{
  "version": 3,
  "cmakeMinimumRequired": { "major": 3, "minor": 24, "patch": 0 },
  "configurePresets": [
    {
      "name": "base",
      "hidden": true,
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/build/${presetName}"
    },
    {
      "name": "win-debug",
      "inherits": "base",
      "displayName": "Windows Debug (MSVC)",
      "condition": { "type": "equals", "lhs": "${hostSystemName}", "rhs": "Windows" },
      "cacheVariables": { "CMAKE_BUILD_TYPE": "Debug" }
    },
    {
      "name": "win-release",
      "inherits": "win-debug",
      "displayName": "Windows Release (MSVC)",
      "cacheVariables": { "CMAKE_BUILD_TYPE": "RelWithDebInfo" }
    },
    {
      "name": "mac-debug",
      "inherits": "base",
      "displayName": "macOS Debug (AppleClang)",
      "condition": { "type": "equals", "lhs": "${hostSystemName}", "rhs": "Darwin" },
      "cacheVariables": { "CMAKE_BUILD_TYPE": "Debug" }
    },
    {
      "name": "mac-release",
      "inherits": "mac-debug",
      "displayName": "macOS Release (AppleClang)",
      "cacheVariables": { "CMAKE_BUILD_TYPE": "RelWithDebInfo" }
    }
  ],
  "buildPresets": [
    { "name": "win-debug",   "configurePreset": "win-debug"   },
    { "name": "win-release", "configurePreset": "win-release" },
    { "name": "mac-debug",   "configurePreset": "mac-debug"   },
    { "name": "mac-release", "configurePreset": "mac-release" }
  ]
}
```

Build:

```bash
cmake --preset win-debug        # or mac-debug
cmake --build --preset win-debug
```

The binary lands in `build/win-debug/src/app/aftertouch[.exe]`.

---

## 5. The core: engine stub with the right shape

The engine does nothing yet. What matters is that its **interface** is already
correct, because everything downstream depends on that shape.

### `src/core/include/core/fixed.hpp`

```cpp
#pragma once
#include <cstdint>

namespace at {

// 16.16 signed fixed point. This is the only numeric type the simulation uses
// for anything spatial. No float, no double, anywhere under src/core/.
//
// The feel of the original comes partly from integer truncation behaviour.
// Reimplementing in floating point produces something that looks identical and
// plays wrong, and you will not be able to find out why.
struct Fix {
    int32_t raw = 0;

    static constexpr int kShift = 16;
    static constexpr int32_t kOne = 1 << kShift;

    constexpr Fix() = default;
    constexpr explicit Fix(int32_t r) : raw(r) {}

    static constexpr Fix FromInt(int32_t v) { return Fix(v << kShift); }
    constexpr int32_t ToInt() const { return raw >> kShift; }

    constexpr Fix operator+(Fix o) const { return Fix(raw + o.raw); }
    constexpr Fix operator-(Fix o) const { return Fix(raw - o.raw); }
    constexpr Fix operator-() const      { return Fix(-raw); }

    constexpr Fix operator*(Fix o) const {
        return Fix(static_cast<int32_t>(
            (static_cast<int64_t>(raw) * o.raw) >> kShift));
    }
    constexpr Fix operator/(Fix o) const {
        return Fix(static_cast<int32_t>(
            (static_cast<int64_t>(raw) << kShift) / o.raw));
    }

    constexpr bool operator==(const Fix&) const = default;
    constexpr auto operator<=>(const Fix&) const = default;
};

struct Vec2 { Fix x, y; };
struct Vec3 { Fix x, y, z; };

} // namespace at
```

Rounding semantics of `operator*` and `operator/` are placeholders. When you start
trace-diffing against a reference implementation, these are among the first things
you will need to match exactly. Leave a comment here saying so.

### `src/core/include/core/match_input.hpp`

```cpp
#pragma once
#include <cstdint>

namespace at {

// Eight-way digital direction, sampled once per tick. Never analog.
// Analog magnitude or free angle is a different game.
enum class Dir : uint8_t {
    None = 0, N, NE, E, SE, S, SW, W, NW
};

struct PlayerInput {
    Dir  dir  = Dir::None;
    bool fire = false;
};

struct MatchInput {
    PlayerInput p1;
    PlayerInput p2;
};

} // namespace at
```

### `src/core/include/core/match_state.hpp`

```cpp
#pragma once
#include "core/fixed.hpp"
#include <array>
#include <cstdint>

namespace at {

struct EntityState {
    Vec3    pos;
    Vec3    vel;
    uint8_t anim_frame = 0;
    uint8_t flags      = 0;
};

enum class MatchPhase : uint8_t {
    KickOff, InPlay, Goal, HalfTime, FullTime
};

// Everything needed to render a frame, and everything needed to write a trace
// line. Keep it trivially copyable and free of pointers so it can be memcmp'd
// against a reference trace and snapshotted into a replay ring buffer.
struct MatchState {
    uint32_t                   tick = 0;
    MatchPhase                 phase = MatchPhase::KickOff;
    EntityState                ball;
    std::array<EntityState, 22> players;
    std::array<uint8_t, 2>     score{0, 0};
};

static_assert(std::is_trivially_copyable_v<MatchState>);

} // namespace at
```

### `src/core/include/core/match_engine.hpp`

```cpp
#pragma once
#include "core/match_input.hpp"
#include "core/match_state.hpp"

namespace at {

// Deterministic, headless, fixed-step. Given the same seed and the same input
// sequence, produces the same state sequence on every platform and every run.
//
// This class is the reason the whole project is laid out the way it is. It has
// no dependency on SDL, ImGui, the filesystem, the clock, or the platform.
class MatchEngine {
public:
    static constexpr int kTickHz = 50;

    void Reset(uint32_t seed);
    void Step(const MatchInput& in);

    const MatchState& State() const { return state_; }

private:
    MatchState state_{};
    uint32_t   rng_ = 1;
};

} // namespace at
```

### `src/core/src/match_engine.cpp`

```cpp
#include "core/match_engine.hpp"

namespace at {

void MatchEngine::Reset(uint32_t seed) {
    state_ = MatchState{};
    rng_   = seed ? seed : 1;
}

void MatchEngine::Step(const MatchInput& in) {
    (void)in;
    ++state_.tick;
    // Everything else comes later. The tick counter alone is enough to prove
    // the fixed-step loop in the shell is wired correctly.
}

} // namespace at
```

---

## 6. The shell

### Two presentation modes in one renderer

This is the one genuinely fiddly part of the milestone.

The match wants a small logical resolution with integer scaling so pixels stay
square and crisp. The management UI wants window resolution so ImGui is legible and
tables fit. `SDL_SetRenderLogicalPresentation` is renderer-wide, so you switch it per
phase within a frame:

```cpp
// match pass
SDL_SetRenderLogicalPresentation(r, kMatchW, kMatchH,
                                 SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);
DrawMatch(r, engine.State());

// UI pass
SDL_SetRenderLogicalPresentation(r, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
DrawUI(r);
```

Confirm the disable call against your pinned SDL3 headers rather than trusting this
snippet. The semantics are in `SDL_render.h`.

Choose `kMatchW` / `kMatchH` now and treat them as sacred. **The amount of pitch
visible on screen is a gameplay parameter, not a visual one.** You may render the
match at 4x the original resolution with better sprites and more animation frames.
You may not show more or less pitch. If you widen the view, you have made a
different game, and you will not notice until the passing feels wrong.

### macOS retina

Do **not** pass `SDL_WINDOW_HIGH_PIXEL_DENSITY` yet. On a MacBook it will either make
everything blurry or make everything tiny, depending on how you handle it, and
debugging that on day one is a waste. Get the game running at 1x logical pixels on
both machines first, then deal with density as a separate, deliberate piece of work.

### `src/app/main.cpp`

```cpp
#include <SDL3/SDL.h>
#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_sdlrenderer3.h>

#include "core/match_engine.hpp"
#include "ui_imgui/fonts.hpp"

#include <cstdint>

namespace {

constexpr int kWindowW = 1280;
constexpr int kWindowH = 800;

// Match viewport in logical pixels. Sacred. See section 6.
constexpr int kMatchW = 320;
constexpr int kMatchH = 200;

constexpr uint64_t kTickNs = 1'000'000'000ull / at::MatchEngine::kTickHz;

enum class AppPhase { MainMenu, Match };

} // namespace

int main(int, char**) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Window*   window   = nullptr;
    SDL_Renderer* renderer = nullptr;
    if (!SDL_CreateWindowAndRenderer("aftertouch", kWindowW, kWindowH,
                                     SDL_WINDOW_RESIZABLE,
                                     &window, &renderer)) {
        SDL_Log("SDL_CreateWindowAndRenderer failed: %s", SDL_GetError());
        return 1;
    }
    SDL_SetRenderVSync(renderer, 1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;   // no imgui.ini turds in the cwd
    at::ui::LoadFonts();                    // see fonts.cpp below
    ImGui::StyleColorsDark();
    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    at::MatchEngine engine;
    AppPhase        phase = AppPhase::MainMenu;

    uint64_t last_ns     = SDL_GetTicksNS();
    uint64_t accumulator = 0;
    bool     running     = true;

    while (running) {
        // ---- events ----------------------------------------------------
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            ImGui_ImplSDL3_ProcessEvent(&ev);
            if (ev.type == SDL_EVENT_QUIT) running = false;
            if (ev.type == SDL_EVENT_KEY_DOWN &&
                ev.key.key == SDLK_ESCAPE && phase == AppPhase::Match) {
                phase = AppPhase::MainMenu;
            }
        }

        // ---- fixed timestep --------------------------------------------
        const uint64_t now_ns = SDL_GetTicksNS();
        uint64_t       frame_ns = now_ns - last_ns;
        last_ns = now_ns;

        // Clamp to avoid the spiral of death after a breakpoint or a stall.
        if (frame_ns > 250'000'000ull) frame_ns = 250'000'000ull;

        if (phase == AppPhase::Match) {
            accumulator += frame_ns;
            while (accumulator >= kTickNs) {
                at::MatchInput in{};      // real input mapping comes later
                engine.Step(in);
                accumulator -= kTickNs;
            }
        } else {
            accumulator = 0;
        }

        // ---- render: match pass ----------------------------------------
        SDL_SetRenderDrawColor(renderer, 12, 14, 18, 255);
        SDL_RenderClear(renderer);

        if (phase == AppPhase::Match) {
            SDL_SetRenderLogicalPresentation(
                renderer, kMatchW, kMatchH,
                SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);

            SDL_SetRenderDrawColor(renderer, 20, 90, 40, 255);
            SDL_FRect pitch{0, 0, (float)kMatchW, (float)kMatchH};
            SDL_RenderFillRect(renderer, &pitch);

            SDL_SetRenderDrawColor(renderer, 235, 235, 235, 255);
            SDL_RenderDebugText(renderer, 8.0f, 8.0f, "In progress..");

            char tickbuf[64];
            SDL_snprintf(tickbuf, sizeof tickbuf, "tick %u",
                         engine.State().tick);
            SDL_RenderDebugText(renderer, 8.0f, 20.0f, tickbuf);
            SDL_RenderDebugText(renderer, 8.0f, 32.0f, "ESC to return");

            SDL_SetRenderLogicalPresentation(
                renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
        }

        // ---- render: UI pass -------------------------------------------
        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        if (phase == AppPhase::MainMenu) {
            ImGui::SetNextWindowPos(ImVec2(40, 40), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(320, 200), ImGuiCond_FirstUseEver);
            ImGui::Begin("aftertouch");
            if (ImGui::Button("MATCH", ImVec2(280, 48))) {
                engine.Reset(1);
                accumulator = 0;
                phase = AppPhase::Match;
            }
            ImGui::End();
        }

        ImGui::Render();
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);

        SDL_RenderPresent(renderer);
    }

    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
```

`SDL_RenderDebugText` uses SDL3's built-in 8x8 font, so the match placeholder needs
no font asset and no SDL_ttf. It stays useful long after this milestone as a debug
overlay.

`ImGui_ImplSDLRenderer3_RenderDrawData` takes the renderer as a second argument in
recent ImGui versions and did not in older ones. Check your pinned tag.

### Font crispness: `src/app/ui_imgui/fonts.cpp`

```cpp
#include "ui_imgui/fonts.hpp"
#include <imgui.h>

namespace at::ui {

void LoadFonts() {
    ImGuiIO& io = ImGui::GetIO();

    ImFontConfig cfg;
    cfg.OversampleH = 1;      // no horizontal oversampling
    cfg.OversampleV = 1;      // no vertical oversampling
    cfg.PixelSnapH  = true;   // snap glyphs to the pixel grid

    // The player database will contain Polish, Turkish, Czech, Greek and
    // Cyrillic names. GetGlyphRangesDefault() covers Basic Latin and Latin-1
    // only, which loses every diacritic that matters.
    static ImVector<ImWchar> ranges;
    ImFontGlyphRangesBuilder builder;
    builder.AddRanges(io.Fonts->GetGlyphRangesDefault());
    builder.AddRanges(io.Fonts->GetGlyphRangesGreek());
    builder.AddRanges(io.Fonts->GetGlyphRangesCyrillic());

    static const ImWchar latin_ext[] = {
        0x0100, 0x017F,   // Latin Extended-A: Polish, Czech, Turkish, Croatian
        0x0180, 0x024F,   // Latin Extended-B
        0,
    };
    builder.AddRanges(latin_ext);
    builder.BuildRanges(&ranges);

    // Load at an exact integer size. Fractional sizes defeat pixel snapping.
    io.Fonts->AddFontFromFileTTF(
        AT_ASSET_DIR "/fonts/ui.ttf", 16.0f, &cfg, ranges.Data);
}

} // namespace at::ui
```

Drop any TTF at `assets/fonts/ui.ttf` to start. If the file is missing, ImGui falls
back to its built-in font and you will not immediately notice, so check that your
diacritics render before assuming it worked.

If your pinned ImGui is on the 1.92 or later font system, `OversampleH/V` behave
differently (they became automatic). Verify against the headers and adjust; the
`PixelSnapH` and integer-size advice holds either way.

### The door: `src/app/ui/ui_backend.hpp`

The milestone above puts ImGui calls straight in `main.cpp` so you get something
running today. **That is a deliberate temporary shortcut and it is the first thing
to fix.** Before you write the second screen, introduce the interface:

```cpp
#pragma once
#include "ui/intent.hpp"
#include "ui/screen_id.hpp"

namespace at {

struct AppModel;   // everything a screen may read or mutate

// Implemented once per UI toolkit. ImGui today, hand-rolled pixel widgets
// or RmlUi later. Note the granularity: whole screens, not widgets.
// A widget-level interface would be ImGui-shaped and would buy nothing.
class IUiBackend {
public:
    virtual ~IUiBackend() = default;

    virtual bool   Init(void* window, void* renderer) = 0;
    virtual void   Shutdown()                         = 0;
    virtual bool   HandleEvent(const void* sdl_event) = 0;
    virtual void   BeginFrame()                       = 0;
    virtual Intent DrawScreen(ScreenId, AppModel&)    = 0;
    virtual void   EndFrame(void* renderer)           = 0;
};

} // namespace at
```

`void*` for the SDL handles is intentional: this header must not include SDL, so
that it stays a pure description of what a UI backend does. The concrete backend
casts them. Slightly ugly, and worth it.

`Intent` is a small tagged union of things a screen can ask the app to do:
`StartMatch`, `ExitMatch`, `OpenSquad`, `Quit`. Screens never mutate app phase
directly; they return intent and `main.cpp` acts on it. This keeps every screen
testable and keeps the UI backend free of application control flow.

---

## 7. Wall enforcement

### `tools/check_walls.py`

```python
#!/usr/bin/env python3
"""Enforce the architectural walls described in PLAN.md section 0."""
import sys, pathlib, re

ROOT = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
INCLUDE = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]', re.M)

FORBIDDEN = [
    # (directory, list of forbidden include substrings, human explanation)
    ("src/core", ["SDL", "imgui", "app/"],
     "core must not depend on SDL, ImGui, or the shell (wall 1)"),
    ("src/app/ui",  ["imgui", "SDL"],
     "the UI interface must stay toolkit- and platform-agnostic"),
    ("src/app/screens", ["imgui"],
     "screen logic must not include imgui.h (wall 2)"),
]

ALLOWED_IMGUI_DIR = "src/app/ui_imgui"

def sources(d):
    p = ROOT / d
    if not p.exists():
        return []
    return [f for f in p.rglob("*")
            if f.suffix in (".h", ".hpp", ".c", ".cpp", ".cc")]

errors = []

for directory, banned, why in FORBIDDEN:
    for f in sources(directory):
        text = f.read_text(encoding="utf-8", errors="ignore")
        for inc in INCLUDE.findall(text):
            for b in banned:
                if b.lower() in inc.lower():
                    errors.append(
                        f"{f.relative_to(ROOT)}: includes '{inc}' -- {why}")

# Wall 2, the other direction: imgui.h anywhere outside its one directory.
for f in sources("src"):
    rel = f.relative_to(ROOT).as_posix()
    if rel.startswith(ALLOWED_IMGUI_DIR):
        continue
    if rel.startswith("src/app/main.cpp"):
        continue   # TEMPORARY: remove once the UI backend interface lands
    text = f.read_text(encoding="utf-8", errors="ignore")
    for inc in INCLUDE.findall(text):
        if "imgui" in inc.lower():
            errors.append(
                f"{rel}: includes '{inc}' outside {ALLOWED_IMGUI_DIR} (wall 2)")

if errors:
    print("Architectural wall violations:\n")
    for e in errors:
        print("  " + e)
    print(f"\n{len(errors)} violation(s). See PLAN.md section 0.")
    sys.exit(1)

print("Walls OK.")
```

That `main.cpp` exemption is a deliberate, visible piece of technical debt. Delete
the exemption line the moment `IUiBackend` exists, and let the build fail until
`main.cpp` is clean.

---

## 8. Milestone 1: definition of done

You are done with this document when:

- [ ] `cmake --preset win-debug && cmake --build --preset win-debug` succeeds on Windows
- [ ] `cmake --preset mac-debug && cmake --build --preset mac-debug` succeeds on macOS
- [ ] Both produce an identical-behaving binary from the same commit
- [ ] The window opens with an ImGui panel containing a **MATCH** button
- [ ] Clicking it switches to the match view: green field, "In progress..", a live tick counter
- [ ] The tick counter advances at 50/second regardless of display refresh rate (verify on a 60 Hz and a 120 Hz display if you have both)
- [ ] Resizing the window keeps the match view pixel-crisp with square pixels and letterboxing
- [ ] ESC returns to the menu
- [ ] `ctest` passes and `core_tests` links without SDL or ImGui
- [ ] `check-walls` passes and is wired into the default build
- [ ] Diacritics render correctly in an ImGui label (test with a string containing Polish, Turkish and Greek characters)

Commit this as a tagged `milestone-1`. It is the skeleton everything else hangs off.

---

## 9. What comes next, in order

**Do not start with the management UI.** It is the fun design work, it produces
visible progress fastest, and it is worth nothing if the match does not feel right.

### Phase 0: the trace harness

Before writing any physics, build the measuring instrument.

1. Fork a reference implementation and instrument it to dump per-tick state:
   ball position and velocity in raw internal units, all 22 player positions,
   velocities, animation frames, flags.
2. Define a trace format. Fixed-width binary records, one per tick, is fine and
   makes diffing trivial.
3. Make `MatchEngine` able to emit the same format.
4. Build `src/tools/trace_viewer/`: loads two traces, draws both overlaid with
   divergence highlighted, scrubs frame by frame. Reuses your SDL setup.
5. Record a corpus of input sequences: kickoffs, passes, shots, aftertouch,
   tackles, goalkeeper situations, throw-ins.

Now you have a number instead of an opinion. Divergence at tick N is a target you
can optimise against. This is the difference between this project working and this
project joining the long list of clones that felt almost right.

**On the reference implementation:** use it as an oracle, never as a source of code.
Disassembly-derived work is a derivative of a copyrighted binary. Read it to
understand behaviour, then write your own. Do not copy, and do not ship original
assets or team data.

### Phase 1: the match

Fixed-point physics, ball with height and shadow, aftertouch, eight-way input,
player state machine, tactics lookup for off-ball AI, keeper behaviour. Iterate
against traces until divergence is measured in hundreds of ticks rather than tens.

Then play it. If it does not feel right after this phase, stop and fix it. Nothing
downstream rescues a match engine that feels wrong.

### Phase 2: the shell you actually want

Replace `main.cpp`'s inline ImGui with `IUiBackend`. Add SQLite (vendor the
amalgamation) for career state. Build the competition engine as declarative data:
stages as a DAG, entrant sources, tiebreaker rules, so that domestic leagues, cups,
continental competitions and international tournaments are all one engine with
different configuration. Hardcoding one confederation's rules here is the mistake
that kills manager games.

### Phase 3: identity

Once the game exists and you know which screens you use, do the visual pass: heavy
`ImDrawList` custom drawing, or a small hand-rolled widget layer behind the same
`IUiBackend`. You will be styling a dozen widget types you know you need rather
than designing a toolkit speculatively.

This is the opposite order from how it feels natural to work, and it is the right
order for a project whose entire point is that you get to play it.

Pretty is a refactor. Not existing is not.

---

## 10. Addendum: assets

### What is actually in `swos-port/assets`

Contents as of checking:

```
assets/
├── compileAssets.py          26 KB
├── convertGameSprites.py     10 KB
├── processSpriteLayers.py     9 KB
├── sprites/
│   ├── game/                 162 PNG + 49 TXT, plus player/ goalkeeper/ bench/
│   └── menu/                 275 PNG + 43 TXT
└── pitches/
    ├── pitch1/ ... pitch6/   ~215 PNG + 1 TXT each
```

The PNGs (`spr0000.png`, `pt1-0000.png` and friends) are the original game's
artwork, extracted from its data files. They are the rightsholder's copyrighted
work, the repository carries no license file, and none of that changes because the
game is thirty years old or because your project is private.

**Do not commit them to this repo.** Private repos get made public, get pushed to
mirrors, get cloned onto work laptops. Keep the line clean from the start rather
than trying to scrub git history later.

### The scripts are the valuable part

The three Python files are format documentation, and they are worth more to you
than the PNGs are:

- `convertGameSprites.py` describes how sprites are laid out in the original data
- `processSpriteLayers.py` describes the layer structure
- `compileAssets.py` describes how the pieces are assembled into a runtime bundle

Read them to understand the formats, then write your own importer. Do not vendor
them.

**One structural finding worth acting on now.** `assets/sprites/game/` contains
`player/`, `goalkeeper/` and `bench/` subdirectories alongside a script called
`processSpriteLayers.py`. Player sprites are **layered**, not flat: a body layer
plus separately-tinted shirt, shorts and sock layers composited together.

That refines the kit plan from earlier. It is not a flat sprite with a palette
swap. It is a stack of masks composited with team colours. Which is better news
than a palette swap, because it means arbitrary kit colours, patterns and
combinations fall out naturally, and it maps cleanly onto pre-generating both
teams' sprite sheets at kickoff exactly as planned.

Design your own sprite format around layers from day one.

### The pattern to use: import, do not redistribute

This is the ScummVM, OpenMW, OpenRCT2 and devilutionX model. The engine is yours
and lives in the repo. The data comes from the user's own legally owned copy of
the original and never touches version control.

1. Buy SWOS 96/97 on GOG. It costs about the price of a coffee and it makes
   everything below unambiguous.
2. Write `src/tools/assetc/`: a command line tool that takes the path to your
   original installation, reads its data files, and emits **your** runtime format
   into a gitignored directory.
3. On first launch, if the runtime assets are absent, prompt for the install path
   and run the import.
4. Ship placeholder programmer art so the project builds, runs and passes its
   tests on a machine with no original data at all.

Point 4 is not a formality. If the build only works on machines that happen to have
a game installed, you have made the project fragile in a way that will bite you on
the MacBook.

### Concrete changes to the plan

Directory additions:

```
assets/
├── placeholder/              # committed: your own programmer art
│   ├── fonts/ui.ttf
│   └── sprites/              # coloured rectangles, generated
└── generated/                # GITIGNORED: importer output lands here

src/tools/
└── assetc/                   # original data -> runtime format
    ├── CMakeLists.txt
    ├── main.cpp
    ├── sprite_reader.cpp     # your reimplementation of the format
    └── pitch_reader.cpp
```

`.gitignore` additions:

```gitignore
assets/generated/
*.swosdata
```

Renderer-side abstraction, so nothing downstream knows or cares where the art
came from:

```cpp
// src/app/render/asset_source.hpp
namespace at {

// The renderer asks for sprites by logical id. Whether they were imported from
// an original installation, loaded from placeholder art, or drawn by you in
// Aseprite next year is not its problem.
class IAssetSource {
public:
    virtual ~IAssetSource() = default;
    virtual const SpriteSheet* Player(TeamSlot, Dir, int frame) const = 0;
    virtual const SpriteSheet* Ball() const = 0;
    virtual const PitchTiles*  Pitch(PitchType) const = 0;
    virtual bool               IsPlaceholder() const = 0;
};

} // namespace at
```

Two implementations: `PlaceholderAssets` (always available, always builds) and
`ImportedAssets` (reads `assets/generated/`). Same wall discipline as the UI. When
you eventually commission or draw original art, that is a third implementation and
zero changes to render code.

### Why this ordering also happens to be right for Phase 0

During trace-diffing you specifically want pixel-identical sprites, because being
able to visually compare your frame against a reference frame is worth a lot when
a number tells you divergence started at tick 340 but not why. Importing from your
own copy gives you that. Placeholder art would add noise at exactly the wrong
moment.

So the import path is not a compromise you are making for legal tidiness. It is
what you want anyway, and it leaves you with a data importer you were going to
have to write regardless, because the team and player database needs exactly the
same treatment.

### Team and player data

Same rule, same mechanism. The `.tac` tactics format and the team file format are
both well documented by the community, and the community has published season
updates for decades. Read them with your importer, keep them out of the repo, and
ship a small fictional default dataset so a fresh clone has something to play with.
