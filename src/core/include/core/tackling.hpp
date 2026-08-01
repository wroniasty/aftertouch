#pragma once
#include "core/angle.hpp"
#include "core/ball.hpp"
#include "core/heading.hpp"
#include "core/match_clock.hpp"
#include "core/match_state.hpp"
#include "core/possession.hpp"

#include <array>
#include <cstdint>

// Slide tackle, foul test, possession contest — doc/implementation/B7-contests.md.
// Sources: doc/TACKLING.md.

namespace at {

inline constexpr int16_t kPlayerTacklingSpeed = 1792;
inline constexpr int16_t kTackleStateNone     = 0;
inline constexpr int16_t kTackleStateTouched  = 1;
inline constexpr int16_t kTackleStateGood     = 2;
inline constexpr int16_t kWonTheBallTicks     = 12;

inline constexpr int16_t kFoulBoxMinX = 81;
inline constexpr int16_t kFoulBoxMaxX = 590;
inline constexpr int16_t kFoulBoxMinY = 129;
inline constexpr int16_t kFoulBoxMaxY = 769;

inline constexpr std::array<int16_t, 8> kPlayerTacklingDownTime = {
    30, 27, 24, 21, 18, 15, 12, 9};
inline constexpr std::array<int16_t, 8> kComputerTacklingDownTime = {
    3, 3, 3, 3, 3, 3, 3, 3};
inline constexpr std::array<int16_t, 8> kPlAvgTacklingBallControlDiffChance = {
    16, 17, 18, 19, 20, 21, 22, 23};

inline int TacklingAttrIndex(uint8_t attr) {
    return attr > 7 ? 7 : static_cast<int>(attr);
}

inline uint8_t SquadTacklingAttr(const MatchState& s, const Entity& e) {
    const int side_i = e.team_number - 1;
    if (side_i < 0 || side_i >= 2) return 0;
    const int ord = e.player_ordinal;
    if (ord < 1 || ord > kMatchSquadSize) return 0;
    return s.sides[static_cast<size_t>(side_i)]
        .squad[static_cast<size_t>(ord - 1)]
        .attrs.tackling;
}

inline uint8_t SquadControlAttr(const MatchState& s, const Entity& e) {
    const int side_i = e.team_number - 1;
    if (side_i < 0 || side_i >= 2) return 0;
    const int ord = e.player_ordinal;
    if (ord < 1 || ord > kMatchSquadSize) return 0;
    return s.sides[static_cast<size_t>(side_i)]
        .squad[static_cast<size_t>(ord - 1)]
        .attrs.ball_control;
}

inline int32_t PlayerDistSq(const Entity& a, const Entity& b) {
    const int32_t dx = static_cast<int32_t>(a.pos.x.Whole()) -
                       static_cast<int32_t>(b.pos.x.Whole());
    const int32_t dy = static_cast<int32_t>(a.pos.y.Whole()) -
                       static_cast<int32_t>(b.pos.y.Whole());
    return dx * dx + dy * dy;
}

inline int OctantDelta(int a, int b) {
    int d = a - b;
    if (d < 0) d += 8;
    if (d > 4) d = 8 - d;
    return d;
}

inline bool InFoulPitchBox(int16_t x, int16_t y) {
    return x >= kFoulBoxMinX && x <= kFoulBoxMaxX && y >= kFoulBoxMinY &&
           y <= kFoulBoxMaxY;
}

inline void TickWonTheBallTimer(TeamControl& tc) {
    if (tc.won_the_ball_timer > 0) {
        --tc.won_the_ball_timer;
        tc.ball_can_be_controlled = 1;
    }
}

inline void BeginSlide(MatchState& s, int side) {
    TeamControl& tc = s.sides[static_cast<size_t>(side)].control;
    if (tc.controlled_slot < 0 || tc.controlled_slot >= kPitchPlayers) return;
    Entity& e = s.players[static_cast<size_t>(tc.controlled_slot)];

    int dir = tc.current_allowed_direction;
    if (dir < 0 || dir > 7) dir = e.direction;
    if (dir < 0 || dir > 7) dir = 0;

    e.tackle_state = kTackleStateNone;
    e.tackling_timer = 0;
    e.player_down_timer = -1;
    e.player_state = static_cast<uint8_t>(PlayerState::Tackling);
    e.direction = static_cast<int16_t>(dir);
    e.speed = kPlayerTacklingSpeed;
    const Dest off = kDefaultDestinations[static_cast<size_t>(dir)];
    e.dest_x = static_cast<int16_t>(e.pos.x.Whole() + off.x);
    e.dest_y = static_cast<int16_t>(e.pos.y.Whole() + off.y);
    e.is_moving = 1;

    tc.controlled_pl_direction = static_cast<int16_t>(dir);
    tc.header_or_tackle = 1;
    tc.last_heading_tackling_slot = tc.controlled_slot;
    tc.quick_fire = 0;
    tc.normal_fire = 0;
}

inline bool WantContestEntry(const TeamControl& tc) {
    return tc.fire_this_frame != 0 || tc.quick_fire != 0 || tc.normal_fire != 0;
}

// Returns true if a slide or header was started.
inline bool TryBeginSlideOrHeader(MatchState& s, int side) {
    TeamControl& tc = s.sides[static_cast<size_t>(side)].control;
    if (!WantContestEntry(tc)) return false;
    if (GetPl(s) != GameStatePl::InProgress) return false;
    if (tc.player_has_ball) return false;
    if (tc.controlled_slot < 0 || tc.controlled_slot >= kPitchPlayers) return false;

    Entity& e = s.players[static_cast<size_t>(tc.controlled_slot)];
    if (static_cast<PlayerState>(e.player_state) != PlayerState::Normal)
        return false;

    const bool high = tc.ball_8_to_12 || tc.ball_12_to_17 || tc.ball_above_17;
    const bool near_header = tc.pl_very_close_to_ball || tc.pl_not_far_from_ball;

    if (near_header && high) {
        BeginJumpHeader(s, side);
        return true;
    }
    if (near_header) {
        BeginStaticHeader(s, side);
        return true;
    }
    BeginSlide(s, side);
    return true;
}

inline void SetPlayerDowntimeAfterTackle(MatchState& s, Entity& e) {
    const int idx = TacklingAttrIndex(SquadTacklingAttr(s, e));
    if (e.tackling_timer == -1)
        e.player_down_timer =
            static_cast<int8_t>(kComputerTacklingDownTime[static_cast<size_t>(idx)]);
    else
        e.player_down_timer =
            static_cast<int8_t>(kPlayerTacklingDownTime[static_cast<size_t>(idx)]);
}

inline void TickSlideTimers(MatchState& s, int side, Entity& e) {
    const TeamControl& tc = s.sides[static_cast<size_t>(side)].control;
    if (e.tackling_timer >= 0) ++e.tackling_timer;
    // Early release (human only).
    if (e.tackling_timer >= 0 && tc.player_number != 0 && !tc.fire_pressed) {
        e.tackling_timer = static_cast<int16_t>(-e.tackling_timer);
        if (e.tackling_timer >= -2) e.tackling_timer = -1;
    }
}

// avg = (tackling + control) >> 1; returns winning side 0/1.
inline int ResolvePossessionContest(MatchState& s, int slot_a, int slot_b) {
    const Entity& a = s.players[static_cast<size_t>(slot_a)];
    const Entity& b = s.players[static_cast<size_t>(slot_b)];
    const int avg_a =
        (static_cast<int>(SquadTacklingAttr(s, a)) +
         static_cast<int>(SquadControlAttr(s, a))) >>
        1;
    const int avg_b =
        (static_cast<int>(SquadTacklingAttr(s, b)) +
         static_cast<int>(SquadControlAttr(s, b))) >>
        1;
    int diff = avg_a - avg_b;
    const int favoured = (diff >= 0) ? (a.team_number - 1) : (b.team_number - 1);
    if (diff < 0) diff = -diff;
    if (diff > 7) diff = 7; // conscious clamp — attrs 0–15, table has 8 entries

    const uint8_t roll = static_cast<uint8_t>(s.resolve_rng.Draw() & 31);
    const int16_t thr =
        kPlAvgTacklingBallControlDiffChance[static_cast<size_t>(diff)];
    const int winner = (roll < static_cast<uint8_t>(thr)) ? favoured : (1 - favoured);

    TeamControl& wtc = s.sides[static_cast<size_t>(winner)].control;
    wtc.won_the_ball_timer = kWonTheBallTicks;
    wtc.ball_can_be_controlled = 1;

    Entity& ball = s.ball;
    ball.speed = 0;
    ball.dest_x = ball.pos.x.Whole();
    ball.dest_y = ball.pos.y.Whole();
    ball.delta.x = Fix{};
    ball.delta.y = Fix{};
    ResetBothSpinTimers(s);
    return winner;
}

inline void PromoteGoodTackle(MatchState& s, int side, Entity& tackler) {
    const int opp = 1 - side;
    const TeamControl& otc = s.sides[static_cast<size_t>(opp)].control;
    if (otc.controlled_slot < 0 || otc.controlled_slot >= kPitchPlayers) return;
    const Entity& victim = s.players[static_cast<size_t>(otc.controlled_slot)];
    if (victim.ball_distance < 9) return;
    if (PlayerDistSq(tackler, victim) <= kDistVeryCloseSq) return;
    tackler.tackle_state = kTackleStateGood;
}

inline void ApplyTackleBallContact(MatchState& s, int side, int slot) {
    Entity& tackler = s.players[static_cast<size_t>(slot)];
    if (tackler.tackle_state != kTackleStateNone) return;
    if (PossessionBallDistSq(tackler, s.ball) > kDistVeryCloseSq) return;

    TeamControl& tc = s.sides[static_cast<size_t>(side)].control;
    const int opp = 1 - side;
    const TeamControl& otc = s.sides[static_cast<size_t>(opp)].control;
    const bool challenged =
        otc.controlled_slot >= 0 && otc.controlled_slot < kPitchPlayers &&
        s.players[static_cast<size_t>(otc.controlled_slot)].ball_distance < 9;

    if (challenged) {
        tackler.tackle_state = kTackleStateTouched;
        tackler.speed = static_cast<int16_t>(tackler.speed >> 1);
        ResolvePossessionContest(s, slot, otc.controlled_slot);
        return;
    }

    int dir = tc.current_allowed_direction;
    if (dir < 0 || dir > 7) dir = tackler.direction;
    if (dir < 0 || dir > 7) dir = 0;

    Entity& ball = s.ball;
    ball.direction = static_cast<int16_t>(dir);
    const Dest off = kDefaultDestinations[static_cast<size_t>(dir)];
    ball.dest_x = static_cast<int16_t>(ball.pos.x.Whole() + off.x);
    ball.dest_y = static_cast<int16_t>(ball.pos.y.Whole() + off.y);
    const int32_t sp = static_cast<int32_t>(tackler.speed) +
                       (static_cast<int32_t>(tackler.speed) >> 2);
    ball.speed = static_cast<int16_t>(sp > 32767 ? 32767 : sp);
    tackler.speed = static_cast<int16_t>(tackler.speed >> 1);
    tackler.tackle_state = kTackleStateTouched;
    ResetBothSpinTimers(s);
    PromoteGoodTackle(s, side, tackler);
    s.clock.last_team_played = static_cast<uint8_t>(side + 1);
}

inline void PlayerTackled(Entity& victim) {
    victim.player_state = static_cast<uint8_t>(PlayerState::Tackled);
    victim.player_down_timer = 20;
    // Injury / cards → B8.
}

inline void PlayerTacklingTestFoul(MatchState& s, int side, int slot) {
    Entity& tackler = s.players[static_cast<size_t>(slot)];
    if (static_cast<PlayerState>(tackler.player_state) != PlayerState::Tackling)
        return;

    const int opp = 1 - side;
    TeamControl& otc = s.sides[static_cast<size_t>(opp)].control;
    if (otc.controlled_slot < 0 || otc.controlled_slot >= kPitchPlayers) return;
    Entity& victim = s.players[static_cast<size_t>(otc.controlled_slot)];
    if (PlayerDistSq(tackler, victim) > kDistVeryCloseSq) return;

    // Keeper exemption.
    if (victim.player_ordinal == 1) {
        tackler.speed = static_cast<int16_t>((tackler.speed >> 2) | 1);
        return;
    }

    if (!InFoulPitchBox(victim.pos.x.Whole(), victim.pos.y.Whole())) return;

    tackler.speed = static_cast<int16_t>((tackler.speed >> 2) | 1);
    PlayerTackled(victim);

    // Foul ladder.
    if (victim.ball_distance > 800) return;
    bool foul = false;
    if (tackler.tackle_state == kTackleStateNone)
        foul = true;
    else if (tackler.tackle_state == kTackleStateGood)
        foul = false; // dangerous-play comment only — B8/commentary
    else if (OctantDelta(tackler.direction, victim.direction) <= 1)
        foul = true;

    if (foul) {
        s.sides[static_cast<size_t>(side)].stats.fouls_conceded += 1;
        s.globals.foul_x = victim.pos.x.Whole();
        s.globals.foul_y = victim.pos.y.Whole();
    }
}

inline void TickDownTimerState(Entity& e) {
    if (e.player_down_timer > 0) {
        --e.player_down_timer;
        if (e.player_down_timer == 0) {
            e.player_state = static_cast<uint8_t>(PlayerState::Normal);
            e.tackling_timer = 0;
            e.tackle_state = kTackleStateNone;
            e.heading = 0;
            e.dest_x = e.pos.x.Whole();
            e.dest_y = e.pos.y.Whole();
            e.delta.x = Fix{};
            e.delta.y = Fix{};
            e.is_moving = 0;
        }
    }
}

inline void OnTacklingStopped(MatchState& s, Entity& e) {
    if (e.speed != 0) return;
    if (e.player_down_timer > 0) return;
    SetPlayerDowntimeAfterTackle(s, e);
    if (e.player_down_timer <= 0) {
        e.player_state = static_cast<uint8_t>(PlayerState::Normal);
        e.tackling_timer = 0;
        e.tackle_state = kTackleStateNone;
    }
}

inline void ProcessContestContacts(MatchState& s) {
    // Refresh distances for contact tests.
    for (int i = 0; i < kPitchPlayers; ++i)
        s.players[static_cast<size_t>(i)].ball_distance =
            PossessionBallDistSq(s.players[static_cast<size_t>(i)], s.ball);

    for (int side = 0; side < 2; ++side) {
        for (int i = 0; i < 11; ++i) {
            const int slot = side * 11 + i;
            Entity& e = s.players[static_cast<size_t>(slot)];
            const auto st = static_cast<PlayerState>(e.player_state);

            if (st == PlayerState::Tackling) {
                TickSlideTimers(s, side, e);
                ApplyTackleBallContact(s, side, slot);
                PlayerTacklingTestFoul(s, side, slot);
                OnTacklingStopped(s, e);
            } else if (st == PlayerState::JumpHeader ||
                       st == PlayerState::StaticHeader) {
                ApplyHeaderContact(s, side, slot);
                if (st == PlayerState::JumpHeader && e.speed == 0 &&
                    e.player_down_timer <= 0) {
                    e.player_state = static_cast<uint8_t>(PlayerState::Normal);
                    e.heading = 0;
                }
            }

            TickDownTimerState(e);
        }
        // Clear one-tick contest trigger flag after processing.
        s.sides[static_cast<size_t>(side)].control.header_or_tackle = 0;
    }
}

} // namespace at
