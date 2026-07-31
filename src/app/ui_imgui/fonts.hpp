#pragma once

namespace at::ui {

// Loads the UI font with the glyph ranges the player database needs. Call once,
// after ImGui::CreateContext() and before the first frame. See fonts.cpp.
void LoadFonts();

} // namespace at::ui
