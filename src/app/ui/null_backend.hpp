#pragma once
#include "ui/ui_backend.hpp"

#include <cstddef>
#include <vector>

namespace at {

// Headless IUiBackend for D-layer tests. No window, no ImGui, no SDL types in the
// implementation — Init/HandleEvent/EndFrame are no-ops. DrawScreen drains a
// scripted intent queue. See doc/implementation/A6-test-infrastructure.md.

class NullUiBackend final : public IUiBackend {
public:
    void Script(std::vector<Intent> intents) {
        script_ = std::move(intents);
        at_     = 0;
    }

    size_t Remaining() const { return at_ < script_.size() ? script_.size() - at_ : 0; }

    bool Init(void* /*window*/, void* /*renderer*/) override { return true; }
    void Shutdown() override {}
    bool HandleEvent(const void* /*sdl_event*/) override { return false; }
    void BeginFrame() override {}
    void EndFrame(void* /*renderer*/) override {}

    Intent DrawScreen(ScreenId /*id*/, AppModel& /*model*/) override {
        if (at_ >= script_.size()) return {};
        return script_[at_++];
    }

private:
    std::vector<Intent> script_;
    size_t              at_ = 0;
};

} // namespace at
