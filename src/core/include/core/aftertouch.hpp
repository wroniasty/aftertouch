#pragma once
#include "core/angle.hpp"
#include "core/match_clock.hpp"
#include "core/match_input.hpp"
#include "core/match_state.hpp"

#include <array>
#include <cstdint>

// Aftertouch window — doc/implementation/B6-shooting.md / doc/AFTERTOUCH.md.
// Table values are provisional fit targets (LEGACY §15).

namespace at {

inline constexpr int16_t kAftertouchWindow = 10;

// Provisional vertical pairs at spinTimer == 4 (must be visible in C1a).
inline constexpr int32_t kHighKickDeltaZRaw   = 220000;
inline constexpr int16_t kHighKickBallSpeed   = 2200;
inline constexpr int32_t kNormalKickDeltaZRaw = 40000;
inline constexpr int16_t kNormalKickBallSpeed = 3000;

// Decay weights indexed by spin_timer 0..9.
inline constexpr std::array<int16_t, 10> kSpinMultiplierFactor = {
    8, 7, 6, 5, 4, 3, 2, 2, 1, 1};

// 8 dirs × 2 sides (0=left, 1=right). Lateral dest nudge (provisional, visible).
inline constexpr std::array<Dest, 16> kKickSpinFactor = {{
    // N: left=+x, right=-x
    {12, 0}, {-12, 0},
    // NE
    { 8, 8}, { -8, -8},
    // E
    { 0,-12}, { 0, 12},
    // SE
    {-8, 8}, { 8, -8},
    // S
    {-12, 0}, {12, 0},
    // SW
    {-8,-8}, { 8, 8},
    // W
    { 0, 12}, { 0,-12},
    // NW
    { 8,-8}, {-8, 8},
}};

inline constexpr std::array<Dest, 16> kPassingSpinFactor = {{
    { 6, 0}, {-6, 0},
    { 4, 4}, {-4,-4},
    { 0,-6}, { 0, 6},
    {-4, 4}, { 4,-4},
    {-6, 0}, { 6, 0},
    {-4,-4}, { 4, 4},
    { 0, 6}, { 0,-6},
    { 4,-4}, {-4, 4},
}};

// Human stick must refresh every tick while spin is live — team controls alone
// only run every other tick and the 10-tick window is easy to miss.
inline void RefreshAftertouchInput(MatchState& s, const MatchInput& in) {
    for (int side = 0; side < 2; ++side) {
        TeamControl& tc = s.sides[static_cast<size_t>(side)].control;
        if (tc.spin_timer < 0) continue;
        if (tc.player_number == 0) {
            // B9: curl opposite to aim error using ai_ball_spin_direction.
            int kick_dir = tc.controlled_pl_direction;
            if (kick_dir < 0 || kick_dir > 7) kick_dir = 0;
            int adj = 0;
            if (tc.ai_ball_spin_direction < 0) adj = -1;
            else if (tc.ai_ball_spin_direction > 0) adj = 1;
            const int str = tc.ai_aftertouch_strength;
            if (str > 0) adj *= (str > 2 ? 2 : str);
            tc.current_allowed_direction =
                static_cast<int16_t>((kick_dir + adj + 8) & 7);
            continue;
        }
        const PlayerInput& pin = (side == 0) ? in.p1 : in.p2;
        tc.current_allowed_direction = static_cast<int16_t>(pin.dir);
    }
}

inline void ApplyAftertouchForTeam(MatchState& s, int side) {
    TeamControl& tc = s.sides[static_cast<size_t>(side)].control;
    if (tc.spin_timer < 0) return;
    if (GetPl(s) != GameStatePl::InProgress) {
        tc.spin_timer = -1;
        return;
    }

    if (tc.spin_timer == 0) {
        tc.left_spin = 0;
        tc.right_spin = 0;
    }

    const int kick_dir = tc.controlled_pl_direction;
    if (kick_dir < 0 || kick_dir > 7) {
        ++tc.spin_timer;
        if (tc.spin_timer >= kAftertouchWindow) tc.spin_timer = -1;
        return;
    }

    const int joy = tc.current_allowed_direction;

    // Latch curl side on first off-axis push.
    if (!tc.left_spin && !tc.right_spin && joy >= 0 && joy <= 7) {
        const int diff = (joy - kick_dir) & 7;
        if (diff != 0 && diff != 4) {
            if (diff < 4)
                tc.left_spin = 1;
            else
                tc.right_spin = 1;
        }
    }

    const bool latched = tc.left_spin || tc.right_spin;
    if (latched && tc.spin_timer >= 0 && tc.spin_timer < kAftertouchWindow) {
        const int side_i = tc.right_spin ? 1 : 0;
        const size_t idx = static_cast<size_t>(kick_dir * 2 + side_i);
        const int16_t m = kSpinMultiplierFactor[static_cast<size_t>(tc.spin_timer)];
        const auto& table = tc.pass_in_progress ? kPassingSpinFactor : kKickSpinFactor;
        // Pass path indexes by ball direction when travelling; fall back to kick.
        size_t use = idx;
        if (tc.pass_in_progress) {
            int bd = s.ball.direction;
            if (bd < 0 || bd > 7) bd = kick_dir;
            use = static_cast<size_t>(bd * 2 + side_i);
        }
        const Dest f = table[use];
        s.ball.dest_x = static_cast<int16_t>(s.ball.dest_x + f.x * m);
        s.ball.dest_y = static_cast<int16_t>(s.ball.dest_y + f.y * m);
    }

    // Vertical decide once at tick 4.
    if (tc.spin_timer == 4 && joy >= 0 && joy <= 7 && !tc.pass_in_progress) {
        const int diff = (joy - kick_dir) & 7;
        if (diff == 2 || diff == 6) {
            s.ball.delta.z = Fix::FromRaw(kNormalKickDeltaZRaw);
            s.ball.speed = kNormalKickBallSpeed;
        } else if (diff == 3 || diff == 4 || diff == 5) {
            s.ball.delta.z = Fix::FromRaw(kHighKickDeltaZRaw);
            s.ball.speed = kHighKickBallSpeed;
        }
        // Facing speed trim.
        if (joy == 0 || joy == 4)
            s.ball.speed = static_cast<int16_t>(s.ball.speed - s.ball.speed / 4);
        else if (joy & 1)
            s.ball.speed = static_cast<int16_t>(s.ball.speed - s.ball.speed / 4 +
                                                s.ball.speed / 8);
    }

    ++tc.spin_timer;
    if (tc.spin_timer >= kAftertouchWindow)
        tc.spin_timer = -1;
}

inline void ApplyAftertouch(MatchState& s) {
    ApplyAftertouchForTeam(s, 0);
    ApplyAftertouchForTeam(s, 1);
}

} // namespace at
