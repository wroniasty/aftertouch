#pragma once
#include "core/angle.hpp"
#include "core/ball.hpp"   // AttributeGoalSquadIndex, ResetBothSpinTimers (B13 / R4)
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
// [CANDIDATE: amiga keeperSaveDistance asm:34835]
inline constexpr int16_t kKeeperSaveDistance = 24; // was 16
inline constexpr int8_t  kGkDiveDownTime     = 75;

// Skill 0..7 → positioning speed / set speed / catch threshold (AI.md §4.1).
inline constexpr std::array<int16_t, 8> kGkPositionSpeed = {
    832, 864, 896, 928, 960, 992, 1024, 1024};
inline constexpr std::array<int16_t, 8> kGkSetSpeed = {
    160, 176, 192, 208, 224, 240, 256, 256};
inline constexpr std::array<int16_t, 8> kGkCatchThreshold = {
    5, 6, 7, 8, 9, 10, 11, 12};

// [CANDIDATE: amiga goalScoredChances asm:34815] — B13 / R4.
//
// The goal-versus-save roll, which ran *before* the dive decision and which our
// reading of the port did not have at all — AMIGA_CHANGES §4.1 calls it the
// largest single gap opened by the Amiga pass. Index is
// `Finishing − goalieSkill + 7`, giving 0…14 for legal 0–7 attributes; the
// sixteenth entry is an unreachable guard.
//
// Two properties are worth stating because they are the design signature:
// an evenly matched striker and keeper is **exactly 50/50**, and the roll
// **consumes no RNG** — it reads the frame counter, like the goalmouth scatter.
// That is why this can be added without moving the RNG stream.
inline constexpr std::array<int16_t, 16> kGoalScoredChances = {
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 0};

// Gates a shot must pass before it resolves at all (amiga asm:42571–42587).
inline constexpr int32_t kShotResolveBallDist = 128;
inline constexpr int16_t kShotResolveMaxZ     = 16;
inline constexpr int16_t kShotResolveArmTicks = 22;
// A beaten keeper's ball speed is captured and clamped (amiga asm:42613).
inline constexpr int16_t kBeatenKeeperBallSpeed = 1536;

// True if this shot beats the keeper. Pure: no RNG, no state mutation.
inline constexpr bool ShotBeatsKeeper(int finishing, int goalie_skill, uint32_t tick) {
    int idx = finishing - goalie_skill + 7;
    if (idx < 0) idx = 0;
    if (idx > 15) idx = 15;
    const int roll = static_cast<int>((tick >> 1) & 15u);
    return roll < kGoalScoredChances[static_cast<size_t>(idx)];
}

// B13 / R2 invariant: one entry per goalie-skill value (itself derived into 0–7).
static_assert(kGkPositionSpeed.size() == kAttrTableSize);
static_assert(kGkSetSpeed.size() == kAttrTableSize);
static_assert(kGkCatchThreshold.size() == kAttrTableSize);

inline int GkSkillIndex(const MatchState& s, int side) {
    const auto& sp = s.sides[static_cast<size_t>(side)].squad[0];
    return AttrIndex0to7(sp.goalie_skill);
}

inline int16_t OwnGoalLineY(const MatchState& s, int side) {
    // team_playing_up attacks high-y; their own goal is the opposite line.
    const uint8_t team = static_cast<uint8_t>(side + 1);
    if (s.globals.team_playing_up == team) return kPlayableMinY;
    return kPlayableMaxY;
}

// [CANDIDATE: amiga SetPlayerWithNoBallDestination asm:36060] — the whole of
// SWOS's keeper positioning, which is four lines of arithmetic and no logic:
//
//     destX = 285 + (ballX - 81) * 103 / 510
//     destY = base + (ballY - 129) *  27 / 641      base 135 top / 737 bottom
//
// He tracks the ball across a **103-pixel arc** (285…387, centred on the goal at
// 336, about 20px past each post) and a **27-pixel depth band**: 27 off his line
// when the ball is at the far end, back on it when the ball is at his feet.
// No angle narrowing, no sweeping, no decision.
//
// What was here instead put his destination at the *midpoint between the ball
// and his own goal line* on a 254-pixel horizontal arc. With the ball on the
// halfway line that is 160 units off his line — outside his own box and heading
// for the centre circle. That is the "keeper chases the ball across the whole
// pitch" report, and it is why this now lives in one function that both callers
// share instead of two divergent copies.
inline constexpr int16_t kKeeperArcX      = 285;
inline constexpr int16_t kKeeperArcSpan   = 103;
inline constexpr int16_t kKeeperBandSpan  = 27;
inline constexpr int16_t kKeeperTopBase   = 135;
inline constexpr int16_t kKeeperTopLimit  = 161;
inline constexpr int16_t kKeeperBotBase   = 737;
inline constexpr int16_t kKeeperBotLimit  = 763;

inline Dest KeeperRestDestination(const MatchState& s, int side) {
    const int32_t bx = s.ball.pos.x.Whole();
    const int32_t by = s.ball.pos.y.Whole();

    int32_t gx = kKeeperArcX + (bx - kPlayableMinX) * kKeeperArcSpan / 510;
    if (gx < kKeeperArcX) gx = kKeeperArcX;
    if (gx > kKeeperArcX + kKeeperArcSpan) gx = kKeeperArcX + kKeeperArcSpan;

    const bool top = OwnGoalLineY(s, side) < kCentreSpotY;
    const int32_t base = top ? kKeeperTopBase : kKeeperBotBase;
    const int32_t lim  = top ? kKeeperTopLimit : kKeeperBotLimit;
    int32_t gy = base + (by - kPlayableMinY) * kKeeperBandSpan / 641;
    if (gy < base) gy = base;
    if (gy > lim) gy = lim;

    return Dest{static_cast<int16_t>(gx), static_cast<int16_t>(gy)};
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
    // A keeper holding the ball stays put. But the claim states were only ever
    // cleared by ApplyRestartTake, and only for the player who *takes* the
    // restart — so a keeper who claimed and then did not take it (the ball went
    // out at the far end, a team-mate restarted, sandbox reset the scene) was
    // latched into GoalieClaimed for good, and this early return then froze him
    // wherever he stood for the rest of the match.
    //
    // Open play with the ball no longer his is proof the claim is over.
    if (st == PlayerState::GoalieCatchingBall || st == PlayerState::GoalieClaimed) {
        const bool ball_is_his =
            tc.player_has_ball && tc.controlled_slot == static_cast<int8_t>(slot);
        if (GetPl(s) == GameStatePl::InProgress && !ball_is_his)
            gk.player_state = static_cast<uint8_t>(PlayerState::Normal);
        else
            return;
    }

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

            // B13 / R4 — goal or save, decided *before* the dive. If the striker
            // beats the keeper on the chance table the keeper is not allowed to
            // commit to a save at all: the ball is let through at a clamped speed
            // with the aftertouch window killed, and the despairing dive is
            // cosmetic. Only then does reachability get a say.
            //
            // Gated the same way the Amiga gates it, so an ordinary claim of a
            // loose ball is not resolved as a shot: close, low, and not on the
            // very frame the ball was struck (the 22-frame arming delay).
            if (gk.ball_distance <= kShotResolveBallDist &&
                s.ball.pos.z.Whole() <= kShotResolveMaxZ && !tc.ball_above_17) {
                const int atk = 1 - side;
                const TeamControl& atc = s.sides[static_cast<size_t>(atk)].control;
                if (atc.pass_kick_timer >= kShotResolveArmTicks) {
                    const uint8_t sq = AttributeGoalSquadIndex(s, atk);
                    const int fin = AttrIndex0to7(
                        s.sides[static_cast<size_t>(atk)].squad[sq].attrs.finishing);
                    if (ShotBeatsKeeper(fin, sk, s.tick)) {
                        if (s.ball.speed > kBeatenKeeperBallSpeed)
                            s.ball.speed = kBeatenKeeperBallSpeed;
                        ResetBothSpinTimers(s);
                        gk.player_state =
                            static_cast<uint8_t>(PlayerState::GoalieDivingLow);
                        gk.player_down_timer = kGkDiveDownTime;
                        gk.speed = 0;
                        return; // beaten: no save attempt this tick
                    }
                }
            }

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

    // Rest: track the ball inside the arc and the depth band (§ above).
    //
    // While the ball is dead he walks back to his post rather than freezing
    // wherever he happened to be. Freezing is what left him standing in odd
    // places after the ball went out — with nothing to move him again until the
    // next kickoff, an out-of-play while he was upfield stranded him there.
    const Dest rest = KeeperRestDestination(s, side);
    gk.dest_x = rest.x;
    gk.dest_y = rest.y;
    gk.speed = (GetPl(s) == GameStatePl::InProgress)
                   ? kGkPositionSpeed[static_cast<size_t>(sk)]
                   : kGkSetSpeed[static_cast<size_t>(sk)];
}

} // namespace at
