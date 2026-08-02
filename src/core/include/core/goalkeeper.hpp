#pragma once
#include "core/angle.hpp"
#include "core/match_clock.hpp"
#include "core/match_input.hpp"
#include "core/match_state.hpp"

#include <array>
#include <cstdint>

// Goalkeeper AI — doc/implementation/B9-ai.md / doc/AI.md §4.

namespace at {

inline constexpr int16_t kGkMoveToBallSpeed = 1024;
inline constexpr int16_t kGkNearJumpSpeed   = 1024;
inline constexpr int16_t kGkFarJumpSpeed    = 2048;
inline constexpr int16_t kKeeperSaveDistance = 16;
inline constexpr int8_t  kGkDiveDownTime     = 75;

// Skill 0..7 → positioning speed / set speed / catch threshold (AI.md §4.1).
inline constexpr std::array<int16_t, 8> kGkPositionSpeed = {
    832, 864, 896, 928, 960, 992, 1024, 1024};
inline constexpr std::array<int16_t, 8> kGkSetSpeed = {
    160, 176, 192, 208, 224, 240, 256, 256};
inline constexpr std::array<int16_t, 8> kGkCatchThreshold = {
    5, 6, 7, 8, 9, 10, 11, 12};

inline int GkSkillIndex(const MatchState& s, int side) {
    const auto& sp = s.sides[static_cast<size_t>(side)].squad[0];
    int sk = static_cast<int>(sp.goalie_skill);
    if (sk < 0) sk = 0;
    if (sk > 7) sk = 7;
    return sk;
}

inline int16_t OwnGoalLineY(const MatchState& s, int side) {
    // team_playing_up attacks high-y; their own goal is the opposite line.
    const uint8_t team = static_cast<uint8_t>(side + 1);
    if (s.globals.team_playing_up == team) return kPlayableMinY;
    return kPlayableMaxY;
}

inline bool LandingInOwnBox(const MatchState& s, int side, int16_t lx, int16_t ly) {
    if (lx < 193 || lx > 478) return false;
    const int16_t gy = OwnGoalLineY(s, side);
    if (gy < kCentreSpotY) // top goal
        return ly >= 137 && ly <= 216;
    return ly >= 682 && ly <= 761;
}

inline void ApplyGoalkeeperAI(MatchState& s, int side) {
    const int slot = side * 11; // ordinal 1
    Entity& gk = s.players[static_cast<size_t>(slot)];
    if (gk.cards < 0) return;

    TeamControl& tc = s.sides[static_cast<size_t>(side)].control;
    // Human controlling the keeper out of goal — leave to stick.
    if (tc.player_number != 0 && tc.controlled_slot == slot &&
        tc.goalie_playing_or_out)
        return;

    const int sk = GkSkillIndex(s, side);
    const int16_t bx = s.ball.pos.x.Whole();
    const int16_t by = s.ball.pos.y.Whole();
    const int16_t goal_y = OwnGoalLineY(s, side);
    const auto st = static_cast<PlayerState>(gk.player_state);

    // Catch / deflect while diving (before rest/claim early-outs).
    if (st == PlayerState::GoalieDivingHigh ||
        st == PlayerState::GoalieDivingLow) {
        if (gk.ball_distance <= 32) {
            const int roll = static_cast<int>((s.tick & 0xF0u) >> 4);
            if (roll < kGkCatchThreshold[static_cast<size_t>(sk)]) {
                gk.player_state =
                    static_cast<uint8_t>(PlayerState::GoalieClaimed);
                gk.speed = 0;
                s.ball.speed = 0;
                s.ball.dest_x = s.ball.pos.x.Whole();
                s.ball.dest_y = s.ball.pos.y.Whole();
                s.ball.delta = {};
                SetGameState(s, GameState::KeeperHoldsBall);
                SetPl(s, GameStatePl::Stopped);
                s.globals.last_team_played_before_break =
                    static_cast<uint8_t>(side + 1);
                s.globals.foul_x = s.ball.pos.x.Whole();
                s.globals.foul_y = s.ball.pos.y.Whole();
                s.globals.player_turn_flags = 0xFF;
                s.globals.break_camera_mode = 255;
                tc.controlled_slot = static_cast<int8_t>(slot);
                for (int i = 0; i < 2; ++i)
                    s.sides[static_cast<size_t>(i)].control.ball_in_play = 0;
                if (PlayerMatchStats* ms = MatchStatsFor(s, side, 0))
                    BumpU16(ms->saves);
            } else {
                s.ball.speed = 1200;
                s.ball.dest_x = s.ball.pos.x.Whole();
                s.ball.dest_y = static_cast<int16_t>(
                    s.ball.pos.y.Whole() +
                    ((goal_y < kCentreSpotY) ? 200 : -200));
                gk.player_state = static_cast<uint8_t>(PlayerState::Normal);
            }
        }
        return;
    }
    if (st == PlayerState::GoalieCatchingBall ||
        st == PlayerState::GoalieClaimed)
        return;

    // Claim: landing in box and keeper twice as close as ball.
    const int16_t lx = s.globals.ball_next_x;
    const int16_t ly = s.globals.ball_next_y_ground_y;
    if (GetPl(s) == GameStatePl::InProgress && LandingInOwnBox(s, side, lx, ly)) {
        const int32_t kdx = gk.pos.x.Whole() - lx;
        const int32_t kdy = gk.pos.y.Whole() - ly;
        const int32_t kdist = kdx * kdx + kdy * kdy;
        const int32_t bdx = bx - lx;
        const int32_t bdy = by - ly;
        const int32_t bdist = bdx * bdx + bdy * bdy;
        if (kdist * 4 <= bdist) {
            gk.dest_x = lx;
            gk.dest_y = ly;
            gk.speed = kGkMoveToBallSpeed;
            const int32_t dy = (goal_y < kCentreSpotY) ? (by - gk.pos.y.Whole())
                                                       : (gk.pos.y.Whole() - by);
            if (dy >= 0 && dy <= kKeeperSaveDistance &&
                gk.ball_distance <= 2000) {
                const bool high = s.ball.pos.z.Whole() > 5;
                gk.player_state = static_cast<uint8_t>(
                    high ? PlayerState::GoalieDivingHigh
                         : PlayerState::GoalieDivingLow);
                gk.player_down_timer = kGkDiveDownTime;
                gk.speed = (gk.ball_distance <= 128) ? kGkNearJumpSpeed
                                                     : kGkFarJumpSpeed;
                const int dir =
                    (bx >= gk.pos.x.Whole()) ? static_cast<int>(Dir::E)
                                             : static_cast<int>(Dir::W);
                gk.direction = static_cast<int16_t>(dir);
                const Dest off = kDefaultDestinations[static_cast<size_t>(dir)];
                gk.dest_x = static_cast<int16_t>(gk.pos.x.Whole() + off.x);
                gk.dest_y = static_cast<int16_t>(gk.pos.y.Whole() + off.y);
            }
            return;
        }
    }

    // Rest: narrow the angle — only while the ball is live.
    if (GetPl(s) != GameStatePl::InProgress) {
        gk.dest_x = gk.pos.x.Whole();
        gk.dest_y = gk.pos.y.Whole();
        gk.delta = {};
        gk.speed = 0;
        return;
    }
    gk.dest_x = static_cast<int16_t>(kCentreSpotX + (bx - kCentreSpotX) / 2);
    gk.dest_y = static_cast<int16_t>(by + (goal_y - by) / 2);
    gk.speed = kGkPositionSpeed[static_cast<size_t>(sk)];
}

} // namespace at
