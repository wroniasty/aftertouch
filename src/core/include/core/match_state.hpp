#pragma once
#include "core/fixed.hpp"
#include <array>
#include <cstdint>
#include <type_traits>

namespace at {

struct EntityState {
    Vec3    pos;
    Vec3    vel;
    uint8_t anim_frame = 0;
    uint8_t flags      = 0;
};

enum class MatchPhase : uint8_t {
    KickOff, InPlay, Goal, HalfTime, FullTime
};

// Everything needed to render a frame, and everything needed to write a trace
// line. Keep it trivially copyable and free of pointers so it can be memcmp'd
// against a reference trace and snapshotted into a replay ring buffer.
struct MatchState {
    uint32_t                   tick = 0;
    MatchPhase                 phase = MatchPhase::KickOff;
    EntityState                ball;
    std::array<EntityState, 22> players;
    std::array<uint8_t, 2>     score{0, 0};
};

static_assert(std::is_trivially_copyable_v<MatchState>);

} // namespace at
