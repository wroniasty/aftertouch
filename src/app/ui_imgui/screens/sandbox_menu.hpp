#pragma once
#include "mode/sandbox.hpp"

#include <cstdint>

namespace at::ui {

// What the sandbox setup dialog is asking the app to do this frame.
enum class SandboxAction : uint8_t { None, Start, Back };

// Draws the C1b sandbox configuration dialog, editing cfg in place. Lives under
// ui_imgui/ because it speaks ImGui directly; that is allowed here and nowhere
// outside src/app/ui_imgui/ (wall 2).
SandboxAction DrawSandboxMenu(mode::SandboxConfig& cfg);

} // namespace at::ui
