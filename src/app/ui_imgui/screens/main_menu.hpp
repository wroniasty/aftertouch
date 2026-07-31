#pragma once
#include "ui/intent.hpp"

namespace at {

struct AppModel;

namespace ui {

// Draws the main menu and returns whatever the user asked for. Lives under
// ui_imgui/ because it speaks ImGui directly; that is allowed here and nowhere
// outside src/app/ui_imgui/ (wall 2).
Intent DrawMainMenu(AppModel& model);

} // namespace ui
} // namespace at
