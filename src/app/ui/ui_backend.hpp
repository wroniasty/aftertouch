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
