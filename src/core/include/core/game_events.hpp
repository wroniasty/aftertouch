#pragma once
#include "core/match_input.hpp"

#include <cstdint>

// Two-level game-control events — doc/implementation/B10-match-input.md /
// doc/INPUT.md §2. Pure; no SDL. Devices live in src/app/input/.

namespace at {

enum GameControlEvents : uint32_t {
    kNoGameEvents     = 0,
    kGameEventUp      = 1u << 0,
    kGameEventDown    = 1u << 1,
    kGameEventLeft    = 1u << 2,
    kGameEventRight   = 1u << 3,
    kGameEventKick    = 1u << 4,
    kGameEventBench   = 1u << 5,  // reserved
    kGameEventPause   = 1u << 6,  // reserved
};

inline constexpr uint32_t kGameEventMovementMask =
    kGameEventUp | kGameEventDown | kGameEventLeft | kGameEventRight;

// Drop conflicting opposite axes, keeping the side that was held last frame
// (INPUT.md / MOVEMENT.md overlapped-events rule).
inline uint32_t FilterOverlappedEvents(uint32_t cur, uint32_t prev) {
    uint32_t out = cur;
    if ((out & kGameEventUp) && (out & kGameEventDown)) {
        if (prev & kGameEventUp)
            out &= ~kGameEventUp;
        else if (prev & kGameEventDown)
            out &= ~kGameEventDown;
        else
            out &= ~kGameEventDown; // default: keep up
    }
    if ((out & kGameEventLeft) && (out & kGameEventRight)) {
        if (prev & kGameEventLeft)
            out &= ~kGameEventLeft;
        else if (prev & kGameEventRight)
            out &= ~kGameEventRight;
        else
            out &= ~kGameEventRight; // default: keep left
    }
    return out;
}

// Diagonals first (MOVEMENT.md §3.1).
inline Dir EventsToDir(uint32_t flags) {
    const bool u = (flags & kGameEventUp) != 0;
    const bool d = (flags & kGameEventDown) != 0;
    const bool l = (flags & kGameEventLeft) != 0;
    const bool r = (flags & kGameEventRight) != 0;

    if (u && r) return Dir::NE;
    if (d && r) return Dir::SE;
    if (d && l) return Dir::SW;
    if (u && l) return Dir::NW;
    if (u) return Dir::N;
    if (r) return Dir::E;
    if (d) return Dir::S;
    if (l) return Dir::W;
    return Dir::None;
}

inline PlayerInput EventsToPlayerInput(uint32_t flags) {
    PlayerInput p{};
    p.dir  = EventsToDir(flags);
    p.fire = (flags & kGameEventKick) != 0;
    return p;
}

} // namespace at
