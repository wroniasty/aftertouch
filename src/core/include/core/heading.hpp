#pragma once
#include "core/angle.hpp"
#include "core/ball.hpp"
#include "core/match_clock.hpp"
#include "core/match_state.hpp"
#include "core/possession.hpp"

#include <array>
#include <cstdint>

// Static / jump headers — doc/implementation/B7-contests.md / doc/HEADING.md.

namespace at {

inline constexpr int16_t kJumpHeaderSpeed          = 2048;
inline constexpr int16_t kStaticHeaderPlayerSpeed  = 256;
inline constexpr int16_t kStaticHeaderBallSpeed    = 1792;
inline constexpr int32_t kBallJumpHeaderDeltaZRaw  = 0xA000;
inline constexpr int32_t kHeaderLowJumpHeightRaw   = 0x20000;
inline constexpr int32_t kHeaderHighJumpHeightRaw  = 0x24000;
inline constexpr int8_t  kStaticHeaderDownTime     = 20;

// Indexed by Heading attr 0..12 (zero at 7). Clamp index to 12.
inline constexpr std::array<int16_t, 13> kPlayerHeaderSpeedIncrease = {
    -336, -288, -240, -192, -144, -96, -48, 0, 513, 1027, 1541, 2055, 2569};

inline int HeadingAttrIndex(uint8_t attr) {
    return attr > 12 ? 12 : static_cast<int>(attr);
}

inline uint8_t SquadHeadingAttr(const MatchState& s, const Entity& e) {
    const int side_i = e.team_number - 1;
    if (side_i < 0 || side_i >= 2) return 0;
    const int ord = e.player_ordinal;
    if (ord < 1 || ord > kMatchSquadSize) return 0;
    return s.sides[static_cast<size_t>(side_i)]
        .squad[static_cast<size_t>(ord - 1)]
        .attrs.heading;
}

inline void BeginJumpHeader(MatchState& s, int side) {
    TeamControl& tc = s.sides[static_cast<size_t>(side)].control;
    if (tc.controlled_slot < 0 || tc.controlled_slot >= kPitchPlayers) return;
    Entity& e = s.players[static_cast<size_t>(tc.controlled_slot)];

    int dir = tc.current_allowed_direction;
    if (dir < 0 || dir > 7) dir = e.direction;
    if (dir < 0 || dir > 7) dir = 0;

    e.heading = 0;
    e.direction = static_cast<int16_t>(dir);
    e.speed = kJumpHeaderSpeed;
    e.player_state = static_cast<uint8_t>(PlayerState::JumpHeader);
    e.player_down_timer = 0;
    const Dest off = kDefaultDestinations[static_cast<size_t>(dir)];
    e.dest_x = static_cast<int16_t>(e.pos.x.Whole() + off.x);
    e.dest_y = static_cast<int16_t>(e.pos.y.Whole() + off.y);
    e.is_moving = 1;

    tc.header_or_tackle = 1;
    tc.last_heading_tackling_slot = tc.controlled_slot;
    tc.ball_can_be_controlled = 0;
    tc.quick_fire = 0;
    tc.normal_fire = 0;
}

inline void BeginStaticHeader(MatchState& s, int side) {
    TeamControl& tc = s.sides[static_cast<size_t>(side)].control;
    if (tc.controlled_slot < 0 || tc.controlled_slot >= kPitchPlayers) return;
    Entity& e = s.players[static_cast<size_t>(tc.controlled_slot)];

    int dir = tc.current_allowed_direction;
    if (dir < 0 || dir > 7) dir = e.direction;
    if (dir < 0 || dir > 7) dir = 0;

    e.heading = 0;
    e.direction = static_cast<int16_t>(dir);
    e.speed = kStaticHeaderPlayerSpeed;
    e.player_state = static_cast<uint8_t>(PlayerState::StaticHeader);
    e.player_down_timer = kStaticHeaderDownTime;
    const Dest off = kDefaultDestinations[static_cast<size_t>(dir)];
    e.dest_x = static_cast<int16_t>(e.pos.x.Whole() + off.x);
    e.dest_y = static_cast<int16_t>(e.pos.y.Whole() + off.y);

    tc.header_or_tackle = 1;
    tc.last_heading_tackling_slot = tc.controlled_slot;
    tc.quick_fire = 0;
    tc.normal_fire = 0;
}

enum class HeaderTrajectory : uint8_t { Base, Flying, Lob };

inline HeaderTrajectory JumpHeaderTrajectory(int rel) {
    // rel = (facing - held) & 7
    if (rel == 2 || rel == 6) return HeaderTrajectory::Flying;
    if (rel == 3 || rel == 4 || rel == 5) return HeaderTrajectory::Lob;
    return HeaderTrajectory::Base;
}

inline int JumpHeaderAimDir(int facing, int held, int rel) {
    if (held < 0) return facing;
    if (rel == 1 || rel == 2 || rel == 3) return (facing + 7) & 7; // −1
    if (rel == 5 || rel == 6 || rel == 7) return (facing + 1) & 7; // +1
    return facing; // 0 or 4
}

inline int StaticHeaderTurn(int facing, int held) {
    if (held < 0 || held > 7) return facing;
    const int d0 = (held - facing) & 7;
    if (d0 == 0 || d0 == 4) return facing;
    int dir = facing;
    if (d0 < 4) {
        // turn right up to two
        const int steps = d0 > 2 ? 2 : d0;
        dir = (facing + steps) & 7;
    } else {
        const int left = 8 - d0;
        const int steps = left > 2 ? 2 : left;
        dir = (facing + 8 - steps) & 7;
    }
    return dir;
}

inline void ApplyHeaderContact(MatchState& s, int side, int slot) {
    Entity& pl = s.players[static_cast<size_t>(slot)];
    if (pl.heading != 0) return;
    if (PossessionBallDistSq(pl, s.ball) > kDistVeryCloseSq) return;

    TeamControl& tc = s.sides[static_cast<size_t>(side)].control;
    Entity& ball = s.ball;
    const auto st = static_cast<PlayerState>(pl.player_state);

    int held = tc.current_allowed_direction;
    const int facing = pl.direction >= 0 && pl.direction <= 7 ? pl.direction : 0;

    if (st == PlayerState::JumpHeader) {
        if (held < 0) held = facing;
        const int rel = (facing - held) & 7;
        const HeaderTrajectory traj =
            (tc.current_allowed_direction < 0) ? HeaderTrajectory::Flying
                                               : JumpHeaderTrajectory(rel);
        int aim = JumpHeaderAimDir(facing, held, rel);
        if (tc.current_allowed_direction < 0) aim = facing;

        tc.pass_in_progress = 0;
        ball.delta.z = Fix::FromRaw(kBallJumpHeaderDeltaZRaw);
        int32_t sp = static_cast<int32_t>(pl.speed) +
                     (static_cast<int32_t>(pl.speed) >> 2);

        if (traj == HeaderTrajectory::Flying) {
            ball.delta.z = Fix::FromRaw(kHeaderLowJumpHeightRaw);
            sp = sp - (sp >> 2); // 75 %
        } else if (traj == HeaderTrajectory::Lob) {
            ball.delta.z = Fix::FromRaw(kHeaderHighJumpHeightRaw);
            sp = sp - (sp >> 4); // 93.75 %
        }

        const int hidx = HeadingAttrIndex(SquadHeadingAttr(s, pl));
        sp += kPlayerHeaderSpeedIncrease[static_cast<size_t>(hidx)];
        if (sp < 0) sp = 0;
        if (sp > 32767) sp = 32767;
        ball.speed = static_cast<int16_t>(sp);
        const Dest off = kDefaultDestinations[static_cast<size_t>(aim)];
        ball.dest_x = static_cast<int16_t>(ball.pos.x.Whole() + off.x);
        ball.dest_y = static_cast<int16_t>(ball.pos.y.Whole() + off.y);
        ball.direction = static_cast<int16_t>(aim);
        pl.speed = static_cast<int16_t>(pl.speed >> 1);
    } else if (st == PlayerState::StaticHeader) {
        const int aim = StaticHeaderTurn(facing, held);
        pl.direction = static_cast<int16_t>(aim);
        const Dest off = kDefaultDestinations[static_cast<size_t>(aim)];
        ball.dest_x = static_cast<int16_t>(ball.pos.x.Whole() + off.x);
        ball.dest_y = static_cast<int16_t>(ball.pos.y.Whole() + off.y);
        ball.direction = static_cast<int16_t>(aim);
        int32_t sp = kStaticHeaderBallSpeed;
        const int hidx = HeadingAttrIndex(SquadHeadingAttr(s, pl));
        sp += kPlayerHeaderSpeedIncrease[static_cast<size_t>(hidx)];
        if (sp < 0) sp = 0;
        if (sp > 32767) sp = 32767;
        ball.speed = static_cast<int16_t>(sp);
        ball.delta.z = Fix::FromRaw(-(ball.delta.z.Raw() / 2));
    } else {
        return;
    }

    pl.heading = 1;
    ResetBothSpinTimers(s);
    s.clock.last_team_played = static_cast<uint8_t>(side + 1);
}

} // namespace at
