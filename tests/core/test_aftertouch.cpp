// B6 / B6a: aftertouch latch rules, curl geometry, vertical sample, window close.
//
// These assert *properties*, not table literals. The values in
// kKickSpinFactor / kSpinMultiplierFactor are provisional fit targets
// (LEGACY §15); a test that pins them to their current numbers has to be
// rewritten the day they are measured, which is the one day it must not move.
// Every case here loops over all eight kick octants — the E/W sign inversion
// B6a/S1 fixes was invisible precisely because the old cases only used N.
#include <doctest/doctest.h>

#include "core/aftertouch.hpp"
#include "core/match_clock.hpp"
#include "core/shooting.hpp"

#include "kick_fixture.hpp"

using namespace at;
using at::test::kOctantStep;

namespace {

// Dot with the kick direction: 0 means the nudge is purely lateral.
int AlongKick(Dest nudge, int dir) {
    const Dest d = kOctantStep[static_cast<size_t>(dir)];
    return nudge.x * d.x + nudge.y * d.y;
}

// Positive when the nudge points clockwise of the kick direction on screen
// (y grows downward, so the clockwise perpendicular of (x,y) is (-y,x)).
int ClockwiseOfKick(Dest nudge, int dir) {
    const Dest d = kOctantStep[static_cast<size_t>(dir)];
    return nudge.x * (-d.y) + nudge.y * d.x;
}

MatchState SpinState(int kick_dir, int joy_dir, int16_t spin_timer) {
    MatchState s{};
    SetPl(s, GameStatePl::InProgress);
    s.sides[0].control.spin_timer = spin_timer;
    s.sides[0].control.controlled_pl_direction = static_cast<int16_t>(kick_dir);
    s.sides[0].control.current_allowed_direction = static_cast<int16_t>(joy_dir);
    s.ball.dest_x = 336;
    s.ball.dest_y = 100;
    return s;
}

} // namespace

TEST_CASE("curl vectors are lateral to the kick, on the side of the push") {
    for (int dir = 0; dir < 8; ++dir) {
        CAPTURE(dir);
        const Dest cw  = kKickSpinFactor[static_cast<size_t>(dir * 2)];
        const Dest ccw = kKickSpinFactor[static_cast<size_t>(dir * 2 + 1)];
        // Purely lateral: a curl must not add or remove range.
        CHECK(AlongKick(cw, dir) == 0);
        CHECK(AlongKick(ccw, dir) == 0);
        // A stick pushed clockwise of the kick bends the ball clockwise.
        CHECK(ClockwiseOfKick(cw, dir) > 0);
        CHECK(ClockwiseOfKick(ccw, dir) < 0);
        // The two sides are exact opposites.
        CHECK(cw.x == -ccw.x);
        CHECK(cw.y == -ccw.y);
    }
}

TEST_CASE("pass curl vectors obey the same geometry") {
    for (int dir = 0; dir < 8; ++dir) {
        CAPTURE(dir);
        const Dest cw  = kPassingSpinFactor[static_cast<size_t>(dir * 2)];
        const Dest ccw = kPassingSpinFactor[static_cast<size_t>(dir * 2 + 1)];
        CHECK(AlongKick(cw, dir) == 0);
        CHECK(AlongKick(ccw, dir) == 0);
        CHECK(ClockwiseOfKick(cw, dir) > 0);
        CHECK(ClockwiseOfKick(ccw, dir) < 0);
    }
}

TEST_CASE("latch side follows the rotational offset, for every kick octant") {
    for (int kick = 0; kick < 8; ++kick) {
        for (int diff = 0; diff < 8; ++diff) {
            const int joy = (kick + diff) & 7;
            CAPTURE(kick);
            CAPTURE(diff);
            MatchState s = SpinState(kick, joy, 0);
            ApplyAftertouchForTeam(s, 0);
            const TeamControl& tc = s.sides[0].control;
            if (diff == 0 || diff == 4) {
                CHECK(tc.spin_cw == 0);
                CHECK(tc.spin_ccw == 0);
            } else if (diff < 4) {
                CHECK(tc.spin_cw == 1);
                CHECK(tc.spin_ccw == 0);
            } else {
                CHECK(tc.spin_cw == 0);
                CHECK(tc.spin_ccw == 1);
            }
        }
    }
}

TEST_CASE("a latched curl walks the destination laterally, never backwards") {
    for (int kick = 0; kick < 8; ++kick) {
        CAPTURE(kick);
        MatchState s = SpinState(kick, (kick + 2) & 7, 0); // clockwise push
        const Dest before{s.ball.dest_x, s.ball.dest_y};
        ApplyAftertouchForTeam(s, 0);
        const Dest moved{static_cast<int16_t>(s.ball.dest_x - before.x),
                         static_cast<int16_t>(s.ball.dest_y - before.y)};
        CHECK(AlongKick(moved, kick) == 0);
        CHECK(ClockwiseOfKick(moved, kick) > 0);
    }
}

TEST_CASE("the decay curve is positive and never rises") {
    CHECK(kSpinMultiplierFactor.size() == static_cast<size_t>(kAftertouchWindow));
    for (size_t i = 0; i < kSpinMultiplierFactor.size(); ++i) {
        CAPTURE(i);
        CHECK(kSpinMultiplierFactor[i] > 0);
        if (i > 0) CHECK(kSpinMultiplierFactor[i] <= kSpinMultiplierFactor[i - 1]);
    }
    // Sooner really is stronger: the first entry must beat the last.
    CHECK(kSpinMultiplierFactor.front() > kSpinMultiplierFactor.back());
}

TEST_CASE("the vertical sample maps offset to drive or lob, for every octant") {
    // Ordering property of the three launch heights, independent of values.
    //
    // B13 / R3 inverted one of these. On the provisional numbers the "drive"
    // sat *below* the flat launch (40000 < 70000), so aftertouch down flattened
    // a shot. On the Amiga's it sits above it ($16000 > $14000): both tick-4
    // outcomes lift the ball and the drive is merely the lower lift. There is no
    // way to aftertouch a shot flatter than it left the boot. That is a
    // behavioural difference a player feels, not a rounding change, and it is
    // recorded in B13 §6 as worth confirming against a trace.
    CHECK(kBallKickingDeltaZRaw < kNormalKickDeltaZRaw);
    CHECK(kNormalKickDeltaZRaw < kHighKickDeltaZRaw);

    for (int kick = 0; kick < 8; ++kick) {
        for (int diff = 0; diff < 8; ++diff) {
            const int joy = (kick + diff) & 7;
            CAPTURE(kick);
            CAPTURE(diff);
            MatchState s = SpinState(kick, joy, kAftertouchVerticalTick);
            s.ball.delta.z = Fix::FromRaw(kBallKickingDeltaZRaw);
            s.ball.speed = 2000;
            ApplyAftertouchForTeam(s, 0);
            const int32_t dz = s.ball.delta.z.Raw();
            if (diff == 2 || diff == 6) {
                CHECK(dz == kNormalKickDeltaZRaw);
            } else if (diff == 3 || diff == 4 || diff == 5) {
                CHECK(dz == kHighKickDeltaZRaw);
            } else {
                CHECK(dz == kBallKickingDeltaZRaw); // aligned: launch arc kept
            }
        }
    }
}

TEST_CASE("the window is exactly kAftertouchWindow samples and then closes") {
    MatchState s = SpinState(0, 0, 0);
    int samples = 0;
    while (s.sides[0].control.spin_timer != kSpinInactive && samples < 100) {
        ApplyAftertouchForTeam(s, 0);
        ++samples;
    }
    CHECK(samples == kAftertouchWindow);
    CHECK(s.sides[0].control.spin_timer == kSpinInactive);
}

TEST_CASE("a dead ball closes the window") {
    MatchState s = SpinState(0, 2, 3);
    SetPl(s, GameStatePl::Stopped);
    ApplyAftertouchForTeam(s, 0);
    CHECK(s.sides[0].control.spin_timer == kSpinInactive);
}
