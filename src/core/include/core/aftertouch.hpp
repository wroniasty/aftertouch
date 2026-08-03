#pragma once
#include "core/angle.hpp"
#include "core/match_clock.hpp"
#include "core/match_input.hpp"
#include "core/match_state.hpp"
#include "core/profile.hpp"

#include <array>
#include <cstdint>

// Aftertouch window — doc/implementation/B6-shooting.md / doc/AFTERTOUCH.md.
// Table values are provisional fit targets (LEGACY §15).

namespace at {

inline constexpr int16_t kAftertouchWindow = 10;

// B13 / R5 #2 — the side latch. Reading A is (joystick - reference) & 7; the
// Amiga is (reference - joystick) & 7. One helper so all three call sites move
// together — they diverged once already (B6a / S1) and that is how the E and W
// curl rows ended up holding the wrong perpendicular.
inline constexpr int AftertouchLatchDiff(int joystick, int reference) {
    return kAftertouchLatchInverted ? ((reference - joystick) & 7)
                                    : ((joystick - reference) & 7);
}

// spin_timer sentinels. Armed means "the strike happened this tick"; the window
// opens on the next Step so that spin_timer 0 — the highest-weighted entry of
// kSpinMultiplierFactor — is sampled against a stick the player can still move.
// Sampling it on the strike tick can only ever read the kick direction back.
inline constexpr int16_t kSpinInactive = -1;
inline constexpr int16_t kSpinArmed    = -2;

// The single tick at which the vertical launch is decided (AFTERTOUCH §5).
inline constexpr int16_t kAftertouchVerticalTick = 4;

// Close the window on both sides. Lives here with the spin constants rather than
// in ball.hpp so that possession.hpp can end the window on capture without
// pulling in the whole ball physics header.
inline void ResetBothSpinTimers(MatchState& s) {
    s.sides[0].control.spin_timer = kSpinInactive;
    s.sides[1].control.spin_timer = kSpinInactive;
}

// [CANDIDATE: amiga — lob $20000/2688, drive $16000/2560, at tick 4 exactly]
// Vertical pairs written at the sample tick. Note the drive is no longer the
// *faster* of the two by a wide margin: 2688 vs 2560 is a 5 % gap, where the
// provisional pair had the drive 36 % faster than the lob. Lofting a shot now
// costs almost nothing in pace, which is a materially different trade-off.
inline constexpr int32_t kHighKickDeltaZRaw   = 0x20000; // lob
inline constexpr int16_t kHighKickBallSpeed   = 2688;
inline constexpr int32_t kNormalKickDeltaZRaw = 0x16000; // drive
inline constexpr int16_t kNormalKickBallSpeed = 2560;

// [PROVISIONAL: LEGACY §15 "Aftertouch" — pass vs shot]
// The pass path's loft pair (longPass / longSpinPass in the reference).
inline constexpr int32_t kLongPassDeltaZRaw = 120000;
inline constexpr int16_t kLongPassBallSpeed = 2400;

// [CANDIDATE: amiga spinMultiplierFactor asm:30735]
// Weights indexed by spin_timer 0..9.
//
// This sums to **23**, where the provisional ramp summed to 39, and it is far
// more front-loaded: over half the authority (5+4+3 = 12 of 23) is spent in the
// first three ticks.
//
// Do not read the smaller sum as "less curl". The per-tick magnitudes in
// kKickSpinFactor rise from 12 to 32 in the same change, so net authority is
// 32×23 = 736 against 12×39 = 468 — roughly **1.6× more** total curl, delivered
// in a much shorter burst. Aftertouch becomes sharper and stronger, not weaker.
// It is the single most felt change in B13 and belongs against the play-feel
// gate, not just a re-pin.
inline constexpr std::array<int16_t, 10> kSpinMultiplierFactor = {
    5, 4, 3, 2, 2, 2, 2, 1, 1, 1};

static_assert(kSpinMultiplierFactor.size() == kAftertouchWindow,
              "one decay weight per tick of the window");

// 8 kick octants × 2 sides, [cw, ccw] per octant. Each entry is the lateral
// dest nudge for a stick pushed to that side of the kick line: the screen-space
// perpendicular of the kick direction, clockwise first. With y growing
// downward the clockwise perpendicular of (dx, dy) is (-dy, dx) — the E and W
// rows used to hold the counter-clockwise one, so horizontal kicks curled
// against the stick (B6a / S1).
//
// [CANDIDATE: amiga kickSpinFactor asm:30746 — magnitudes 0 / 23 / 32]
// The geometry is unchanged; only the magnitudes move, 12 → 32 on a cardinal
// axis and 8 → 23 on a diagonal. 23 is 32/√2 to the nearest integer (22.6), so
// the two rows describe one circle rather than two independent tunings — which
// is decent evidence the pair is read correctly.
inline constexpr std::array<Dest, 16> kKickSpinFactor = {{
    // N  (0,-1) -> cw (1,0)
    { 32,   0}, {-32,   0},
    // NE (1,-1) -> cw (1,1)
    { 23,  23}, {-23, -23},
    // E  (1, 0) -> cw (0,1)
    {  0,  32}, {  0, -32},
    // SE (1, 1) -> cw (-1,1)
    {-23,  23}, { 23, -23},
    // S  (0, 1) -> cw (-1,0)
    {-32,   0}, { 32,   0},
    // SW (-1,1) -> cw (-1,-1)
    {-23, -23}, { 23,  23},
    // W  (-1,0) -> cw (0,-1)
    {  0, -32}, {  0,  32},
    // NW (-1,-1) -> cw (1,-1)
    { 23, -23}, {-23,  23},
}};

// Same geometry, indexed by the ball's travel direction (AFTERTOUCH §6).
// Half strength for passes; 23/2 rounds to 11, so the circle is preserved
// (16/√2 = 11.3).
inline constexpr std::array<Dest, 16> kPassingSpinFactor = {{
    { 16,   0}, {-16,   0},  // N
    { 11,  11}, {-11, -11},  // NE
    {  0,  16}, {  0, -16},  // E
    {-11,  11}, { 11, -11},  // SE
    {-16,   0}, { 16,   0},  // S
    {-11, -11}, { 11,  11},  // SW
    {  0, -16}, {  0,  16},  // W
    { 11, -11}, {-11,  11},  // NW
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
    if (tc.spin_timer == kSpinArmed) {
        // The strike landed this Step. Open the window without sampling: the
        // stick this tick is the kick direction by construction, so sampling
        // now would spend kSpinMultiplierFactor[0] on a guaranteed no-op.
        tc.spin_timer = (GetPl(s) == GameStatePl::InProgress) ? int16_t{0}
                                                             : kSpinInactive;
        return;
    }
    if (tc.spin_timer < 0) return;
    if (GetPl(s) != GameStatePl::InProgress) {
        tc.spin_timer = kSpinInactive;
        return;
    }

    if (tc.spin_timer == 0) {
        tc.spin_cw = 0;
        tc.spin_ccw = 0;
    }

    const int kick_dir = tc.controlled_pl_direction;
    if (kick_dir < 0 || kick_dir > 7) {
        ++tc.spin_timer;
        if (tc.spin_timer >= kAftertouchWindow) tc.spin_timer = kSpinInactive;
        return;
    }

    const int joy = tc.current_allowed_direction;

    // Latch curl side on first off-axis push.
    if (!tc.spin_cw && !tc.spin_ccw && joy >= 0 && joy <= 7) {
        const int diff = AftertouchLatchDiff(joy, kick_dir);
        if (diff != 0 && diff != 4) {
            if (diff < 4)
                tc.spin_cw = 1;
            else
                tc.spin_ccw = 1;
        }
    }

    const bool latched = tc.spin_cw || tc.spin_ccw;
    if (latched && tc.spin_timer >= 0 && tc.spin_timer < kAftertouchWindow) {
        const int side_i = tc.spin_ccw ? 1 : 0;
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

    // Vertical decide once at the sample tick.
    if (tc.spin_timer == kAftertouchVerticalTick && joy >= 0 && joy <= 7) {
        if (!tc.pass_in_progress) {
            const int diff = AftertouchLatchDiff(joy, kick_dir);
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
        } else {
            // Pass loft (AFTERTOUCH §6): the pass path gates height on
            // longPass / longSpinPass rather than swapping a launch pair, and
            // measures the stick against the ball's travel direction the same
            // way its curl table is indexed.
            int ref_dir = s.ball.direction;
            if (ref_dir < 0 || ref_dir > 7) ref_dir = kick_dir;
            const int diff = AftertouchLatchDiff(joy, ref_dir);
            if (diff == 3 || diff == 4 || diff == 5) {
                tc.long_pass = 1;
                if (latched) tc.long_spin_pass = 1;
                // B13 / R5 #5 — whether a pass can be lofted by aftertouch at
                // all. Reading A raises it; the Amiga leaves deltaZ untouched
                // and gives a one-shot +1/8 on speed instead. See profile.hpp.
                if (kPassLoftEnabled) {
                    s.ball.delta.z = Fix::FromRaw(kLongPassDeltaZRaw);
                    s.ball.speed = kLongPassBallSpeed;
                } else {
                    s.ball.speed = static_cast<int16_t>(
                        s.ball.speed + (s.ball.speed >> kPassAftertouchSpeedShift));
                }
            }
        }
    }

    ++tc.spin_timer;
    if (tc.spin_timer >= kAftertouchWindow)
        tc.spin_timer = kSpinInactive;
}

inline void ApplyAftertouch(MatchState& s) {
    ApplyAftertouchForTeam(s, 0);
    ApplyAftertouchForTeam(s, 1);
}

} // namespace at
