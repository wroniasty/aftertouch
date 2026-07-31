#pragma once
#include "ui/ui_backend.hpp"

namespace at {

// The one and only place the ImGui UI toolkit is bound to IUiBackend. Milestone
// 1 still drives ImGui inline from main.cpp; this concrete backend is the target
// that inline code migrates into before the second screen is written (PLAN.md
// section 6, "The door").
class ImGuiBackend final : public IUiBackend {
public:
    bool   Init(void* window, void* renderer) override;
    void   Shutdown() override;
    bool   HandleEvent(const void* sdl_event) override;
    void   BeginFrame() override;
    Intent DrawScreen(ScreenId, AppModel&) override;
    void   EndFrame(void* renderer) override;
};

} // namespace at
