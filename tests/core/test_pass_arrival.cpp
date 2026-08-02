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
    CHECK(s.ball.dest_x == 350);

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

TEST_CASE("out of range pass falls back to facing ground kick") {
    MatchState s = MakePassScene();
    // Far east — beyond Passing=4 max (~70+32=102).
    s.players[7].pos.x = Fix::FromInt(500);
    s.players[7].pos.y = Fix::FromInt(400);
    s.sides[0].control.pass_to_slot = 7;

    REQUIRE(ApplyKickOrPass(s, 0));
    CHECK(s.sides[0].control.pass_to_slot == -1);
    CHECK(s.sides[0].control.pass_in_progress == 1);
    CHECK(s.ball.delta.z.Raw() == kBallPassingDeltaZRaw);
    CHECK(s.ball.dest_x == static_cast<int16_t>(300 + 1000));
    CHECK(s.ball.dest_y == 400);
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

TEST_CASE("pass speed indexes Passing attribute") {
    MatchState s = MakePassScene();
    s.sides[0].squad[5].attrs.passing = 7; // ordinal 6 → squad index 5
    s.players[5].player_ordinal = 6;
    s.players[7].pos.x = Fix::FromInt(340);
    s.players[7].pos.y = Fix::FromInt(400);

    REQUIRE(ApplyKickOrPass(s, 0));
    CHECK(s.ball.speed == static_cast<int16_t>(kBallPassingSpeed +
                                               kBallSpeedPassingIncrease[7]));
}
