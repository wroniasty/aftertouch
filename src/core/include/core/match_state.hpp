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

// The seven outfield attributes the engine may read. Range 0–15 is a hard rule
// (doc/DATA.md section 3); A5's loader rejects anything outside it before these
// fields are filled. Career-side continuous abilities never appear here — see
// implementation/PLAN.md standing constraint 4.
struct PlayerAttrs {
    uint8_t passing      = 0;
    uint8_t shooting     = 0;
    uint8_t heading      = 0;
    uint8_t tackling     = 0;
    uint8_t ball_control = 0;
    uint8_t speed        = 0;
    uint8_t finishing    = 0;
};

// Kit colours are 0–9 ordinals; the non-contiguous map onto palette indices lives
// in palette.atl (A4), not here.
struct KitSpec {
    uint8_t shirt_type = 0;   // 0 plain … 3 coloured sleeves
    uint8_t stripes    = 0;
    uint8_t shirt      = 0;
    uint8_t shorts     = 0;
    uint8_t socks      = 0;
};

// Per-side sheet projected at kickoff. Constant for the match; not written into
// the per-tick trace record (A3) — squad identity is not a physics signal.
struct TeamSheet {
    std::array<char, 24> name{};
    uint8_t              tactics_id = 0;
    KitSpec              primary{};
    KitSpec              secondary{};
};

// Everything needed to render a frame, and everything needed to write a trace
// line. Keep it trivially copyable and free of pointers so it can be memcmp'd
// against a reference trace and snapshotted into a replay ring buffer.
//
// Players 0–10 are side 0, 11–21 are side 1. Parallel arrays hold the A5
// projection (attrs / shirt / position) so EntityState stays the physics blob
// B1 will expand.
struct MatchState {
    uint32_t                    tick  = 0;
    MatchPhase                  phase = MatchPhase::KickOff;
    EntityState                 ball;
    std::array<EntityState, 22> players;
    std::array<uint8_t, 2>      score{0, 0};

    std::array<TeamSheet, 2>     teams{};
    std::array<PlayerAttrs, 22>  player_attrs{};
    std::array<uint8_t, 22>      shirt_numbers{};
    std::array<uint8_t, 22>      positions{};   // Position enum, see data/game_data.hpp
};

static_assert(std::is_trivially_copyable_v<MatchState>);

} // namespace at
