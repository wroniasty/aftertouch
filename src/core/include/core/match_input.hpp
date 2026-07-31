#pragma once
#include <cstdint>

namespace at {

// Eight-way digital direction, sampled once per tick. Never analog.
// Analog magnitude or free angle is a different game.
enum class Dir : uint8_t {
    None = 0, N, NE, E, SE, S, SW, W, NW
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
