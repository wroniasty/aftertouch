#pragma once
#include "core/match_input.hpp"

#include <cstdint>

struct SDL_Gamepad;

namespace at {

// SDL keyboard + gamepad → MatchInput. See B10-match-input.md.
class MatchInputSource {
public:
    MatchInputSource() = default;
    ~MatchInputSource();

    MatchInputSource(const MatchInputSource&) = delete;
    MatchInputSource& operator=(const MatchInputSource&) = delete;

    // Open first available gamepad if any. Safe to call once after SDL_Init.
    void Init();

    // Sample devices into MatchInput (filtered overlapping axes).
    void Poll(MatchInput& out);

    // Neutral input for a tick the player does not own (a dialog has the
    // keyboard). Still one MatchInput per tick, so traces stay replayable.
    void PollNeutral(MatchInput& out);

private:
    uint32_t PollKeyboardP1() const;
    uint32_t PollKeyboardP2() const;
    uint32_t PollGamepad(SDL_Gamepad* pad) const;

    SDL_Gamepad* pad0_ = nullptr;
    SDL_Gamepad* pad1_ = nullptr;
    uint32_t     prev_p1_ = 0;
    uint32_t     prev_p2_ = 0;
};

} // namespace at
