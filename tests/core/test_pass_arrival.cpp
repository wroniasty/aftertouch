// Pass arrival handoff, park receiver, range gate, settle-on-capture.
#include <doctest/doctest.h>

#include "core/ai.hpp"
#include "core/movement.hpp"
#include "core/possession.hpp"
#include "core/shooting.hpp"

using namespace at;

namespace {

MatchState MakePassScene() {
    MatchState s{};
    SetPl(s, GameStatePl::InProgress);
    s.globals.team_playing_up = 1;
    TeamControl& tc = s.sides[0].control;
    tc.player_number = 1;
    tc.team_number = 1;
    tc.controlled_slot = 5;
    tc.ball_can_be_controlled = 1;
    tc.ball_in_play = 1;
    tc.ball_out_of_play = 0;
    tc.current_allowed_direction = static_cast<int16_t>(Dir::E);
    tc.pl_very_close_to_ball = 1;
    tc.pl_close_to_ball = 1;
    tc.player_has_ball = 1;
    tc.quick_fire = 1;

    for (int i = 0; i < 11; ++i) {
        Entity& e = s.players[static_cast<size_t>(i)];
        e.team_number = 1;
        e.player_ordinal = static_cast<int16_t>(i + 1);
        e.player_state = static_cast<uint8_t>(PlayerState::Normal);
        e.cards = 0;
        e.pos.x = Fix::FromInt(200);
        e.pos.y = Fix::FromInt(400);
        e.direction = static_cast<int16_t>(Dir::E);
        s.sides[0].squad[static_cast<size_t>(i)].attrs.passing = 4;
    }
    s.players[5].pos.x = Fix::FromInt(300);
    s.players[5].pos.y = Fix::FromInt(400);
    s.ball.pos.x = Fix::FromInt(300);
    s.ball.pos.y = Fix::FromInt(400);
    s.ball.pos.z = Fix{};
    return s;
}

} // namespace

TEST_CASE("pass arrival hands control to receiver and arms switch timer") {
    MatchState s = MakePassScene();
    s.players[7].pos.x = Fix::FromInt(350);
    s.players[7].pos.y = Fix::FromInt(400);
    s.sides[0].control.pass_to_slot = 7;

    REQUIRE(ApplyKickOrPass(s, 0));
    CHECK(s.sides[0].control.pass_in_progress == 1);
    CHECK(s.sides[0].control.pass_to_slot == 7);
    // Ray extended through the receiver, so the aim point is beyond him and on
    // the same heading — the ball runs on if he misses it.
    CHECK(s.ball.dest_x > 350);
    CHECK(s.ball.dest_y == 400);

    // Ball arrives at receiver feet (lockout still running — handoff first).
    s.ball.pos.x = Fix::FromInt(350);
    s.ball.pos.y = Fix::FromInt(400);
    s.ball.speed = 1800;
    s.players[7].ball_distance = 0;

    CHECK(TryCompletePassArrival(s, 0));
    CHECK(s.sides[0].control.controlled_slot == 7);
    CHECK(s.sides[0].control.pass_to_slot == -1);
    CHECK(s.sides[0].control.player_switch_timer == kPassArrivalSwitchTicks);
    CHECK(s.sides[0].control.pass_in_progress == 1); // cleared on capture
}

TEST_CASE("pass target is parked while ball is in flight") {
    MatchState s = MakePassScene();
    s.players[7].pos.x = Fix::FromInt(350);
    s.players[7].pos.y = Fix::FromInt(400);
    s.sides[0].control.pass_to_slot = 7;
    s.sides[0].control.pass_in_progress = 1;
    s.sides[0].control.player_has_ball = 0;
    s.sides[0].control.ball_out_of_play = 1;

    ApplyOffBallDestination(s, 0, 7);
    CHECK(s.players[7].dest_x == 350);
    CHECK(s.players[7].dest_y == 400);
    CHECK(s.players[7].speed == 0);
}

// This case used to assert that a team-mate 200 units away was "out of range"
// and the pass degraded to a clearance. There is no range limit in the original
// — the only filters are the cone and the player's state — and inventing one at
// ~100 units meant almost nobody on a 510x641 pitch ever qualified, so nearly
// every pass became a facing-direction clearance. That was the reported "passes
// just go where you're facing".
TEST_CASE("a distant team-mate in the cone is still a valid pass target") {
    MatchState s = MakePassScene();
    s.players[7].pos.x = Fix::FromInt(500); // 200 units east of the ball
    s.players[7].pos.y = Fix::FromInt(400);

    REQUIRE(ApplyKickOrPass(s, 0));
    CHECK(s.sides[0].control.pass_to_slot == 7);
    CHECK(s.sides[0].control.pass_in_progress == 1);
    CHECK(s.ball.delta.z.Raw() == kBallPassingDeltaZRaw);
    CHECK(s.ball.dest_y == 400);
    CHECK(s.ball.dest_x > 500); // aim ray extended past him
}

TEST_CASE("a pass with nobody in the cone is a flat-speed clearance") {
    MatchState s = MakePassScene();
    // Everyone parked behind the passer, well outside the +-22.5 degree cone.
    for (int i = 0; i < 11; ++i) {
        if (i == 5) continue;
        s.players[static_cast<size_t>(i)].pos.x = Fix::FromInt(120);
        s.players[static_cast<size_t>(i)].pos.y = Fix::FromInt(760);
    }
    REQUIRE(ApplyKickOrPass(s, 0));
    CHECK(s.sides[0].control.pass_to_slot == -1);
    CHECK(s.ball.dest_x == static_cast<int16_t>(300 + 1000));
    CHECK(s.ball.dest_y == 400);
    CHECK(s.ball.speed == kPassClearanceSpeed); // flat, no Passing bonus
}

// The cone is anchored at the **ball**, not the passer. The two differ through
// most of a dribble, which is when passes are actually played.
TEST_CASE("the pass cone is measured from the ball") {
    MatchState s = MakePassScene();
    // Ball well ahead of the passer; team-mate due east of the ball but well
    // off the axis as seen from the passer.
    s.ball.pos.x = Fix::FromInt(300);
    s.ball.pos.y = Fix::FromInt(400);
    s.players[5].pos.x = Fix::FromInt(240);
    s.players[5].pos.y = Fix::FromInt(400);
    s.players[7].pos.x = Fix::FromInt(420);
    s.players[7].pos.y = Fix::FromInt(400);

    REQUIRE(ApplyKickOrPass(s, 0));
    CHECK(s.sides[0].control.pass_to_slot == 7);
}

TEST_CASE("pass capture settles ball speed with neutral stick") {
    MatchState s = MakePassScene();
    TeamControl& tc = s.sides[0].control;
    tc.player_has_ball = 0;
    tc.pass_in_progress = 1;
    tc.passing_kicking_slot = 5;
    tc.controlled_slot = 7;
    tc.pass_to_slot = -1;
    tc.pass_kick_timer = 0;
    tc.ball_can_be_controlled = 1;
    tc.current_allowed_direction = -1;

    s.players[7].pos.x = Fix::FromInt(350);
    s.players[7].pos.y = Fix::FromInt(400);
    s.ball.pos.x = Fix::FromInt(350);
    s.ball.pos.y = Fix::FromInt(400);
    s.ball.speed = 1600;
    s.ball.delta.x = Fix::FromInt(2);
    s.ball.delta.y = Fix{};

    UpdatePossessionForSide(s, 0);
    CHECK(tc.player_has_ball == 1);
    CHECK(tc.pass_in_progress == 0);
    CHECK(s.ball.speed == 0);
    CHECK(s.ball.delta.x.Raw() == 0);
    CHECK(s.ball.delta.y.Raw() == 0);
}

namespace {

// One pass, from the ball at x=300 to a receiver `reach` units to the east.
int16_t PassSpeedFor(uint8_t passing_attr, int16_t reach) {
    MatchState s = MakePassScene();
    s.sides[0].squad[5].attrs.passing = passing_attr; // ordinal 6 → squad index 5
    s.players[5].player_ordinal = 6;
    s.players[7].pos.x = Fix::FromInt(static_cast<int16_t>(300 + reach));
    s.players[7].pos.y = Fix::FromInt(400);
    REQUIRE(ApplyKickOrPass(s, 0));
    return s.ball.speed;
}

} // namespace

// B13 / R3. Pass strength is banded by **distance to the receiver**, not by how
// long fire was held — the Amiga set corrected itself on this (AMIGA_CHANGES
// §6a). Asserted as two monotonicities rather than one arithmetic identity, so
// these survive the day the bands are fitted.
TEST_CASE("pass speed rises with the Passing attribute") {
    constexpr int16_t kReach = 40;
    int16_t prev = PassSpeedFor(0, kReach);
    for (uint8_t a = 1; a <= kAttrMax; ++a) {
        const int16_t here = PassSpeedFor(a, kReach);
        CHECK(here > prev);
        prev = here;
    }
}

TEST_CASE("pass speed rises with distance to the receiver") {
    // Only bands a pass can actually reach are exercised. PassTargetMaxDistSq
    // caps a Passing-7 pass at 126 units, so bands 3–7 of the Amiga's eight
    // 50-unit bands are unreachable through the targeted path — a real tension
    // between our pass range and the banding, recorded in B13 §6 rather than
    // silently resolved by widening one of them here.
    constexpr uint8_t kAttr = kAttrMax;
    const int16_t band0 = PassSpeedFor(kAttr, 25);
    const int16_t band1 = PassSpeedFor(kAttr, 75);
    const int16_t band2 = PassSpeedFor(kAttr, 120);
    CHECK(band1 > band0);
    CHECK(band2 > band1);
}

TEST_CASE("a targetless quick-fire is a clearance, not a maximum-power pass") {
    // Nobody in the cone: the destination becomes the +-1000 facing ray, whose
    // length must not be read as a pass distance (B13 / R3).
    MatchState s = MakePassScene();
    s.sides[0].squad[5].attrs.passing = kAttrMax;
    s.players[5].player_ordinal = 6;
    for (int i = 0; i < 11; ++i) { // park every possible receiver far away
        if (i == 5) continue;
        s.players[static_cast<size_t>(i)].pos.x = Fix::FromInt(120);
        s.players[static_cast<size_t>(i)].pos.y = Fix::FromInt(760);
    }
    REQUIRE(ApplyKickOrPass(s, 0));
    CHECK(s.sides[0].control.pass_to_slot == -1);
    CHECK(s.ball.speed < static_cast<int16_t>(kBallPassingSpeedByBand[7] +
                                              kBallSpeedPassingIncrease[kAttrMax]));
}
