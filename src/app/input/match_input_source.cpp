#include "input/match_input_source.hpp"

#include "core/game_events.hpp"

#include <SDL3/SDL.h>

namespace at {

MatchInputSource::~MatchInputSource() {
    if (pad0_) SDL_CloseGamepad(pad0_);
    if (pad1_) SDL_CloseGamepad(pad1_);
    pad0_ = pad1_ = nullptr;
}

void MatchInputSource::Init() {
    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    if (!ids) return;
    if (count >= 1) pad0_ = SDL_OpenGamepad(ids[0]);
    if (count >= 2) pad1_ = SDL_OpenGamepad(ids[1]);
    SDL_free(ids);
}

uint32_t MatchInputSource::PollKeyboardP1() const {
    const bool* keys = SDL_GetKeyboardState(nullptr);
    uint32_t f = 0;
    if (keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_W]) f |= kGameEventUp;
    if (keys[SDL_SCANCODE_DOWN] || keys[SDL_SCANCODE_S]) f |= kGameEventDown;
    if (keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A]) f |= kGameEventLeft;
    if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D]) f |= kGameEventRight;
    if (keys[SDL_SCANCODE_SPACE] || keys[SDL_SCANCODE_Z]) f |= kGameEventKick;
    return f;
}

uint32_t MatchInputSource::PollKeyboardP2() const {
    const bool* keys = SDL_GetKeyboardState(nullptr);
    uint32_t f = 0;
    if (keys[SDL_SCANCODE_I]) f |= kGameEventUp;
    if (keys[SDL_SCANCODE_K]) f |= kGameEventDown;
    if (keys[SDL_SCANCODE_J]) f |= kGameEventLeft;
    if (keys[SDL_SCANCODE_L]) f |= kGameEventRight;
    if (keys[SDL_SCANCODE_RETURN] || keys[SDL_SCANCODE_RCTRL]) f |= kGameEventKick;
    return f;
}

uint32_t MatchInputSource::PollGamepad(SDL_Gamepad* pad) const {
    if (!pad) return 0;
    uint32_t f = 0;
    if (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_UP)) f |= kGameEventUp;
    if (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_DOWN)) f |= kGameEventDown;
    if (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_LEFT)) f |= kGameEventLeft;
    if (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) f |= kGameEventRight;
    if (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_SOUTH)) f |= kGameEventKick;

    // Left stick with deadzone (~20% of int16 range).
    constexpr int kDead = 6400;
    const int ax = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX);
    const int ay = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTY);
    if (ay < -kDead) f |= kGameEventUp;
    if (ay > kDead) f |= kGameEventDown;
    if (ax < -kDead) f |= kGameEventLeft;
    if (ax > kDead) f |= kGameEventRight;
    return f;
}

void MatchInputSource::Poll(MatchInput& out) {
    uint32_t p1 = PollKeyboardP1() | PollGamepad(pad0_);
    uint32_t p2 = PollKeyboardP2() | PollGamepad(pad1_);
    // If only one pad, P2 keyboard still works; pad0 stays on P1.

    p1 = FilterOverlappedEvents(p1, prev_p1_);
    p2 = FilterOverlappedEvents(p2, prev_p2_);
    prev_p1_ = p1;
    prev_p2_ = p2;

    out.p1 = EventsToPlayerInput(p1);
    out.p2 = EventsToPlayerInput(p2);
}

} // namespace at
