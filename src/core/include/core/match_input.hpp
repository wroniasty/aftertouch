#pragma once
#include <cstdint>

namespace at {

// Eight-way digital direction. Numbering matches the reference
// (doc/MOVEMENT.md section 3.1): 0 = up, clockwise to 7 = up-left, with -1 for
// none. "None" is the same sentinel the reference holds in
// currentAllowedDirection — not a ninth enumerator.
enum class Dir : int8_t {
    None = -1,
    N    = 0,
    NE   = 1,
    E    = 2,
    SE   = 3,
    S    = 4,
    SW   = 5,
    W    = 6,
    NW   = 7,
};

struct PlayerInput {
    Dir  dir  = Dir::None;
    bool fire = false;
};

struct MatchInput {
    PlayerInput p1;
    PlayerInput p2;
};

} // namespace at
