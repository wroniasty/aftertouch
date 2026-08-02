#pragma once
#include "core/aftertouch.hpp"
#include "core/match_clock.hpp"
#include "core/match_state.hpp"
#include "core/out_of_play.hpp"
#include "core/profile.hpp"
#include "core/set_pieces.hpp"
#include "core/trig.hpp"

#include <cstdint>

// Ball physics — doc/implementation/B3-ball-physics.md / doc/BALL.md.
// Destination is the primitive; collisions rewrite dest and restore position.

namespace at {

// Dead-ball barrier (whole pitch units). Active only when not InProgress.
inline constexpr int16_t kBarrierMinX = 53;
inline constexpr int16_t kBarrierMaxX = 618;
inline constexpr int16_t kBarrierMinY = 100;
inline constexpr int16_t kBarrierMaxY = 799;

inline constexpr int16_t kGoalAttemptLeft  = 240;
inline constexpr int16_t kGoalAttemptRight = 431;
inline constexpr int32_t kBounceSettleThreshold = 0xA000;

// Pitch-type tables (BALL.md §9). Index 0..6 = Frozen..Hard. B3 selects Normal.
inline constexpr int16_t kPitchBallSpeedFactorAmiga[7] = {-2, 2, 3, 0, 0, -1, -1};
inline constexpr int16_t kPitchBallSpeedFactorPc[7]    = {-3, 4, 1, 0, 0, -1, -1};
inline constexpr int16_t kBallSpeedBounceFactorTable[7] = {24, 80, 80, 72, 64, 40, 32};
inline constexpr int16_t kBallBounceFactorTable[7]      = {88, 112, 104, 104, 96, 88, 80};

inline constexpr int kPitchTypeNormal = 4;

inline MatchSurface SurfaceForPitchType(int pitch_type) {
    const int i = (pitch_type < 0 || pitch_type > 6) ? kPitchTypeNormal : pitch_type;
    MatchSurface s;
    s.pitch_ball_speed_factor =
        IsAmigaProfile() ? kPitchBallSpeedFactorAmiga[i] : kPitchBallSpeedFactorPc[i];
    s.ball_speed_bounce_factor = kBallSpeedBounceFactorTable[i];
    s.ball_bounce_factor       = kBallBounceFactorTable[i];
    return s;
}

inline void ReverseDestX(Entity& e) {
    const int16_t x = e.pos.x.Whole();
    e.dest_x = static_cast<int16_t>(2 * x - e.dest_x);
}

inline void ReverseDestY(Entity& e) {
    const int16_t y = e.pos.y.Whole();
    e.dest_y = static_cast<int16_t>(2 * y - e.dest_y);
}

// Test / B6 helper: aim the ball without going through a kick pipeline.
inline void LaunchBall(MatchState& s, Dest dest, int16_t speed, Fix delta_z) {
    s.ball.dest_x = dest.x;
    s.ball.dest_y = dest.y;
    s.ball.speed  = speed;
    s.ball.delta.z = delta_z;
}

inline void ApplyFriction(MatchState& s) {
    Entity& ball = s.ball;
    int32_t decel = kBallGroundConstant;
    if (!s.sides[0].control.player_has_ball && !s.sides[1].control.player_has_ball)
        decel += s.surface.pitch_ball_speed_factor;
    if (ball.pos.z.Whole() != 0)
        decel = kBallAirConstant;
    int32_t speed = static_cast<int32_t>(ball.speed) - decel;
    if (speed < 0) speed = 0;
    ball.speed = static_cast<int16_t>(speed);
}

inline void IntegrateZAndBounce(MatchState& s) {
    Entity& ball = s.ball;
    // Live gravity while airborne or carrying vertical velocity (settled ground
    // balls must not take a bounce-loss every tick).
    if (ball.pos.z.Whole() != 0 || ball.delta.z.Raw() != 0)
        ball.delta.z -= Fix::FromRaw(kGravityConstant);

    ball.pos.z += ball.delta.z;
    if (!(ball.pos.z < 0)) return;

    // Ground contact.
    const int32_t spd = ball.speed;
    ball.speed = static_cast<int16_t>(
        spd - ((spd * s.surface.ball_speed_bounce_factor) >> 8));
    if (ball.speed < 0) ball.speed = 0;

    ball.pos.z = Fix{};
    int32_t d = (-ball.delta.z).Raw();
    d -= (d >> 8) * s.surface.ball_bounce_factor;
    d |= 1;
    if (d > kBounceSettleThreshold)
        ball.delta.z = Fix::FromRaw(d);
    else
        ball.delta.z = Fix{};
}

inline void ApplyDeadBallBarrier(MatchState& s, const Vec3& saved) {
    if (GetPl(s) == GameStatePl::InProgress) return;

    Entity& ball = s.ball;
    const Fix& x = ball.pos.x;
    const Fix& y = ball.pos.y;
    bool hit = false;

    if (x < kBarrierMinX || x > kBarrierMaxX) {
        const int16_t px = x.Whole();
        ball.dest_x = static_cast<int16_t>(2 * px - ball.dest_x);
        hit = true;
    }
    if (y < kBarrierMinY || y > kBarrierMaxY) {
        const int16_t py = y.Whole();
        ball.dest_y = static_cast<int16_t>(2 * py - ball.dest_y);
        hit = true;
    }
    if (!hit) return;

    ball.speed = static_cast<int16_t>(ball.speed >> 1);
    ball.pos = saved;
}

inline void ResetBothSpinTimers(MatchState& s) {
    s.sides[0].control.spin_timer = -1;
    s.sides[1].control.spin_timer = -1;
}

// Simplified bar/post (BALL.md §6). Net / own-goal subdivisions deferred.
inline void ApplyGoalFrame(MatchState& s, Vec3& saved) {
    Entity& ball = s.ball;
    const int16_t x = ball.pos.x.Whole();
    const int16_t y = ball.pos.y.Whole();
    const bool past_top = y < kPlayableMinY;
    const bool past_bot = y > kPlayableMaxY;
    if (!past_top && !past_bot) return;

    const bool in_mouth = x >= kGoalMouthMinX && x <= kGoalMouthMaxX;
    const bool in_attempt =
        x >= kGoalAttemptLeft && x <= kGoalAttemptRight;

    bool frame_hit = false;
    if (in_mouth && ball.pos.z.Whole() > 15) {
        // Bar: reverse vertical, restore z, nudge y clear.
        ball.delta.z = -ball.delta.z;
        ball.pos.z = saved.z;
        saved.y += Fix::FromInt(1);
        frame_hit = true;
    } else if (in_attempt && !in_mouth) {
        // Post: mirror dest Y, kill aftertouch.
        ReverseDestY(ball);
        ResetBothSpinTimers(s);
        frame_hit = true;
    }
    if (!frame_hit) return;

    ball.speed = static_cast<int16_t>(ball.speed - (ball.speed >> 2));
    ball.pos.x = saved.x;
    ball.pos.y = saved.y;
}

inline Fix ShlFix(Fix v, int n) {
    return Fix::FromRaw(
        static_cast<int32_t>(static_cast<uint32_t>(v.Raw()) << n));
}

inline void CalculateNextBallPosition(MatchState& s) {
    Entity& ball = s.ball;
    if (ball.speed == 0) {
        s.globals.ball_next_x = ball.pos.x.Whole();
        s.globals.ball_next_y = ball.pos.y.Whole();
        s.globals.ball_next_y_ground_y = s.globals.ball_next_y;
        return;
    }

    Fix x = ball.pos.x;
    Fix y = ball.pos.y;
    Fix z = ball.pos.z;
    Fix d1 = ball.delta.x;
    Fix d2 = ball.delta.y;
    Fix d3 = ball.delta.z;
    Fix d4 = Fix::FromRaw(kGravityConstant);

    // Band selector (BALL.md §8). Rising always coarsest.
    int shift = 0;
    if (d3.Raw() > 0 || z.Whole() > 35)
        shift = 3;
    else if (z.Whole() > 30)
        shift = 2;
    else if (z.Whole() > 20)
        shift = 1;

    if (shift > 0) {
        d1 = ShlFix(d1, shift);
        d2 = ShlFix(d2, shift);
        d4 = ShlFix(d4, shift);
        z = z >> shift;
    }

    // Bound the loop — a stuck ball must not hang the tick.
    for (int i = 0; i < 4096; ++i) {
        x += d1;
        y += d2;
        d3 -= d4;
        z += d3;
        if (z < 0) break;
    }

    s.globals.ball_next_x = x.Whole();
    s.globals.ball_next_y = y.Whole();
    // Alias until ballNextYGroundY is measured against the reference.
    s.globals.ball_next_y_ground_y = s.globals.ball_next_y;
}

// Attribute a goal to a squad index on the scoring side (B12).
inline uint8_t AttributeGoalSquadIndex(const MatchState& s, int side) {
    const TeamControl& tc = s.sides[static_cast<size_t>(side)].control;
    const int base = side * 11;
    if (tc.passing_kicking_slot >= base && tc.passing_kicking_slot < base + 11)
        return static_cast<uint8_t>(tc.passing_kicking_slot - base);
    if (tc.controlled_slot >= base && tc.controlled_slot < base + 11)
        return static_cast<uint8_t>(tc.controlled_slot - base);
    return 9; // default striker
}

// SIMULATION §7: latch goal attempt once per shot when ball is in the attempt
// band traveling toward a goal line with projected x in mouth/attempt range.
inline void LatchGoalAttempt(MatchState& s) {
    if (GetPl(s) != GameStatePl::InProgress) return;
    Entity& ball = s.ball;
    if (ball.speed == 0) {
        s.globals.attempt_latched = 0;
        return;
    }
    const int16_t x = ball.pos.x.Whole();
    const int16_t y = ball.pos.y.Whole();
    const bool in_attempt_x = x >= kGoalAttemptLeft && x <= kGoalAttemptRight;
    if (!in_attempt_x) {
        s.globals.attempt_latched = 0;
        return;
    }
    const bool toward_top = ball.delta.y.Raw() < 0 && y <= kPenaltyBoxTopY;
    const bool toward_bot = ball.delta.y.Raw() > 0 && y >= kPenaltyBoxBotY;
    if (!toward_top && !toward_bot) return;
    if (s.globals.attempt_latched) return;

    const uint8_t team = s.clock.last_team_played;
    if (team != 1 && team != 2) return;
    // Carrier still has it — not a shot.
    if (s.sides[0].control.player_has_ball || s.sides[1].control.player_has_ball)
        return;

    s.globals.attempt_latched = 1;
    TeamStats& st = s.sides[static_cast<size_t>(team - 1)].stats;
    ++st.goal_attempts;
    const int16_t sx = s.globals.ball_next_x;
    if (sx >= kGoalMouthMinX && sx <= kGoalMouthMaxX) ++st.on_target;
}

inline void WireOutOfPlay(MatchState& s) {
    if (GetPl(s) != GameStatePl::InProgress) return;

    const int16_t x = s.ball.pos.x.Whole();
    const int16_t y = s.ball.pos.y.Whole();
    const int16_t z = s.ball.pos.z.Whole();
    auto result = ClassifyBallOutOfPlay(x, y, z, s.clock.last_team_played);
    if (!result.has_value()) return;

    s.globals.foul_x = x;
    s.globals.foul_y = y;
    s.globals.attempt_latched = 0;
    CompleteOopRestart(s, result->state, result->is_goal);

    if (result->is_goal) {
        const uint8_t scorer =
            (y < kPlayableMinY)
                ? s.globals.team_playing_up
                : static_cast<uint8_t>(3 - s.globals.team_playing_up);
        if (scorer == 1 || scorer == 2) {
            const int side = static_cast<int>(scorer) - 1;
            ++s.score[static_cast<size_t>(side)];
            const uint8_t sq = AttributeGoalSquadIndex(s, side);
            ++s.sides[static_cast<size_t>(side)].squad[static_cast<size_t>(sq)]
                  .goals_scored;
            AppendChronicle(s, MatchEventKind::Goal, static_cast<uint8_t>(side),
                            sq);
        }
        s.phase = MatchPhase::Goal;
        return;
    }

    if (IsCornerState(result->state)) {
        const uint8_t taking = TakingTeamForOop(s, result->state, false);
        if (taking == 1 || taking == 2) {
            const uint8_t side = static_cast<uint8_t>(taking - 1);
            ++s.sides[static_cast<size_t>(side)].stats.corners_won;
            AppendChronicle(s, MatchEventKind::Corner, side, 0);
        }
    }
}

inline void UpdateBall(MatchState& s) {
    // B6: aftertouch nudges dest / may rewrite delta_z before integrate.
    ApplyAftertouch(s);

    Entity& ball = s.ball;

    const Dest from{ball.pos.x.Whole(), ball.pos.y.Whole()};
    const Dest dest{ball.dest_x, ball.dest_y};
    const DeltaResult d = CalculateDeltaXAndY(ball.speed, from, dest);
    ball.delta.x = d.dx;
    ball.delta.y = d.dy;
    if (d.heading >= 0) {
        ball.full_direction = d.heading;
        ball.direction = static_cast<int16_t>(
            ((static_cast<uint16_t>(d.heading) + 16u) & 0xFFu) >> 5);
    }

    ApplyFriction(s);

    const Vec3 saved = ball.pos;
    ball.pos.x += ball.delta.x;
    ball.pos.y += ball.delta.y;

    IntegrateZAndBounce(s);

    Vec3 saved_for_frame = saved;
    ApplyDeadBallBarrier(s, saved);
    ApplyGoalFrame(s, saved_for_frame);

    CalculateNextBallPosition(s);
    LatchGoalAttempt(s);
    WireOutOfPlay(s);
}

} // namespace at
