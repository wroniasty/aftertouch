// B6a / S2: the aftertouch window measured through MatchEngine::Step.
//
// Unit-level calls to ApplyAftertouchForTeam cannot see this: the defect is
// that the strike and the first window sample land in the same Step, where the
// stick can only ever read back the kick direction.
#include <doctest/doctest.h>

#include "core/aftertouch.hpp"
#include "kick_fixture.hpp"

using namespace at;
using namespace at::test;

namespace {

// Bend of a shot whose stick is pushed one octant clockwise `delay` Steps
// after the launch. Positive = bent clockwise of the kick.
int BendAtDelay(int delay, Dir kick = Dir::N) {
    MatchEngine eng = MakeKickEngine(336, 600);
    KickScript sc;
    sc.kick_dir      = kick;
    sc.after_dir     = OctantPlus(kick, 1);
    sc.react_delay   = delay;
    sc.dribble_ticks = 20;
    sc.total_ticks   = 60;
    const KickRun r = RunKick(eng, sc);
    REQUIRE(r.struck);
    return LateralBend(r, kick);
}

} // namespace

TEST_CASE("the first window sample is reachable by a stick the player can move") {
    MatchEngine eng = MakeKickEngine(336, 600);
    KickScript sc;
    sc.kick_dir      = Dir::N;
    sc.after_dir     = Dir::NE;
    sc.react_delay   = 1; // the very next Step after the strike
    sc.dribble_ticks = 20;
    const KickRun r = RunKick(eng, sc);

    REQUIRE(r.struck);
    // Reacting on the first possible Step must land on spin_timer 0 — the
    // highest-weighted entry of the decay curve. If it lands on 1, one tenth of
    // the window (and its strongest tick) is unreachable by construction.
    CHECK(r.latch_spin == 0);
}

TEST_CASE("curl authority is strongest at the first sample and decays") {
    const int first = BendAtDelay(1);
    CHECK(first > 0);

    int prev = first;
    for (int d = 2; d <= kAftertouchWindow; ++d) {
        CAPTURE(d);
        const int bend = BendAtDelay(d);
        CHECK(bend <= prev); // never rises with a later reaction
        prev = bend;
    }
    CHECK(first > prev); // and the earliest beats the latest
}

TEST_CASE("a push after the window closes bends nothing") {
    MatchEngine eng = MakeKickEngine(336, 600);
    KickScript sc;
    sc.kick_dir      = Dir::N;
    sc.after_dir     = Dir::NE;
    sc.react_delay   = kAftertouchWindow + 2;
    sc.dribble_ticks = 20;
    const KickRun r = RunKick(eng, sc);

    REQUIRE(r.struck);
    CHECK(r.latch_spin < 0);
    CHECK(LateralBend(r, Dir::N) == 0);
}

TEST_CASE("curl bends the ball with the stick for every kick octant") {
    // The E/W rows of the spin tables used to hold the counter-clockwise
    // vector, so a horizontal kick curled against the push (B6a / S1).
    for (int d = 0; d < 8; ++d) {
        const Dir kick = static_cast<Dir>(d);
        CAPTURE(d);
        MatchEngine eng = MakeKickEngine(336, 449);
        KickScript sc;
        sc.kick_dir      = kick;
        sc.after_dir     = OctantPlus(kick, 1); // one octant clockwise
        sc.react_delay   = 1;
        sc.dribble_ticks = 20;
        sc.total_ticks   = 40;
        const KickRun r = RunKick(eng, sc);
        REQUIRE(r.struck);
        CHECK(LateralBend(r, kick) > 0);
    }
}

// Nothing used to close the aftertouch window when a player collected the ball.
// Two things followed, and both were reported as bugs:
//
//  * the curl kept rewriting the dest of a ball now at a player's feet, so it
//    squirted away from him;
//  * capture clears pass_in_progress, so a pass received before the tick-4
//    vertical sample then took the **shot** branch of that sample and launched
//    a tapped ball off the receiver's foot at shot pace with a full lob rise.
TEST_CASE("collecting the ball closes the aftertouch window") {
    MatchState s{};
    SetPl(s, GameStatePl::InProgress);
    TeamControl& tc = s.sides[0].control;
    tc.controlled_slot = 0;
    tc.ball_can_be_controlled = 1;
    tc.passing_kicking_slot = -1;
    tc.pass_in_progress = 1;
    tc.spin_timer = 2; // window open, before the vertical sample at 4

    s.players[0].team_number = 1;
    s.players[0].player_ordinal = 2;
    s.players[0].player_state = static_cast<uint8_t>(PlayerState::Normal);
    s.players[0].pos.x = Fix::FromInt(200);
    s.players[0].pos.y = Fix::FromInt(400);
    s.ball.pos.x = Fix::FromInt(201);
    s.ball.pos.y = Fix::FromInt(400);

    UpdatePossessionForSide(s, 0);
    REQUIRE(tc.player_has_ball == 1);
    CHECK(tc.spin_timer == kSpinInactive);
    CHECK(s.sides[1].control.spin_timer == kSpinInactive);
}

TEST_CASE("a collected ball cannot be lofted by the tick-4 vertical sample") {
    MatchState s{};
    SetPl(s, GameStatePl::InProgress);
    TeamControl& tc = s.sides[0].control;
    tc.controlled_slot = 0;
    tc.ball_can_be_controlled = 1;
    tc.passing_kicking_slot = -1;
    tc.pass_in_progress = 1;
    tc.spin_timer = 3;
    tc.controlled_pl_direction = static_cast<int16_t>(Dir::N);
    tc.current_allowed_direction = static_cast<int16_t>(Dir::S); // full back-push

    s.players[0].team_number = 1;
    s.players[0].player_ordinal = 2;
    s.players[0].player_state = static_cast<uint8_t>(PlayerState::Normal);
    s.players[0].pos.x = Fix::FromInt(200);
    s.players[0].pos.y = Fix::FromInt(400);
    s.ball.pos.x = Fix::FromInt(201);
    s.ball.pos.y = Fix::FromInt(400);

    UpdatePossessionForSide(s, 0);
    REQUIRE(tc.player_has_ball == 1);

    ApplyAftertouchForTeam(s, 0); // would have been the sample tick
    CHECK(s.ball.delta.z.Raw() == 0); // still on the deck
    CHECK(s.ball.speed == 0);         // still at his feet
}
