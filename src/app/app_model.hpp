#pragma once
#include "core/match_engine.hpp"
#include "ui/screen_id.hpp"

namespace at {

// Everything the UI may read or write. Screens receive this by reference and
// communicate back to the app exclusively through the Intent they return.
//
// For milestone 1 the loop in main.cpp is still inline and does not thread this
// through the UI backend yet; this is the shape it grows into once IUiBackend
// replaces the inline ImGui calls (PLAN.md Phase 2).
struct AppModel {
    ScreenId    screen = ScreenId::MainMenu;
    MatchEngine engine;
};

} // namespace at
