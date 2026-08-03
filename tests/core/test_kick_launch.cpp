// B6: kick launch arms lockout and opens spin.
#include <doctest/doctest.h>

#include "core/ball.hpp"
#include "core/possession.hpp"
#include "core/shooting.hpp"

using namespace at;

namespace {

MatchState MakeKickReady() {
    MatchState s{};
    SetPl(s, GameStatePl::InProgress);
    s.globals.team_playing_up = 1;
    s.sides[0].control.controlled_slot = 0;
    s.sides[0].control.player_number = 1;
    s.sides[0].control.ball_can_be_controlled = 1;
    s.sides[0].control.current_allowed_direction = static_cast<int16_t>(Dir::N);
    s.sides[0].control.pl_very_close_to_ball = 1;
    s.sides[0].control.pl_close_to_ball = 1;
    s.sides[0].control.player_has_ball = 1;
    s.sides[0].control.normal_fire = 1;
    s.players[0].team_number = 1;
    s.players[0].player_ordinal = 2;
    s.players[0].pos.x = Fix::FromInt(336);
    s.players[0].pos.y = Fix::FromInt(449);
    s.players[0].direction = static_cast<int16_t>(Dir::N);
    s.ball.pos.x = Fix::FromInt(336);
    s.ball.pos.y = Fix::FromInt(449);
    s.ball.pos.z = Fix{};
    return s;
}

} // namespace

TEST_CASE("shot launch sets speed height lockout and spin") {
    MatchState s = MakeKickReady();
    CHECK(ApplyKickOrPass(s, 0));
    // Centre spot: on the halfway line, so a goalward N kick is a long shot
    // with a Velocity-0 bonus — the same speed as no bonus at all.
    CHECK(s.ball.speed == kBallKickingSpeed);
    CHECK(s.ball.delta.z.Raw() == kBallKickingDeltaZRaw);
    CHECK(s.sides[0].control.player_has_ball == 0);
    CHECK(s.sides[0].control.pass_kick_timer == kPassKickLockoutTicks);
    CHECK(s.sides[0].control.ball_can_be_controlled == 0);
    // Armed, not open: the window's first sample is the next Step (B6a / S2).
    CHECK(s.sides[0].control.spin_timer == kSpinArmed);
    CHECK(s.sides[0].control.pass_in_progress == 0);
    CHECK(s.sides[0].control.controlled_pl_direction == static_cast<int16_t>(Dir::N));
    CHECK(s.ball.dest_y == static_cast<int16_t>(449 - 1000));
}

TEST_CASE("base launch loft clears whole z within a few ticks") {
    MatchState s = MakeKickReady();
    REQUIRE(ApplyKickOrPass(s, 0));
    for (int i = 0; i < 16; ++i) UpdateBall(s);
    CHECK(s.ball.pos.z.Whole() > 0);
}

TEST_CASE("pass launch sets pass_in_progress") {
    MatchState s = MakeKickReady();
    s.sides[0].control.normal_fire = 0;
    s.sides[0].control.quick_fire = 1;
    // Teammate ahead (north).
    s.players[1].team_number = 1;
    s.players[1].pos.x = Fix::FromInt(336);
    s.players[1].pos.y = Fix::FromInt(400);
    CHECK(ApplyKickOrPass(s, 0));
    CHECK(s.sides[0].control.pass_in_progress == 1);
    CHECK(s.sides[0].control.pass_kick_timer == kPassKickLockoutTicks);
    // The aim point is the ball->receiver ray extended past the pitch edge, not
    // the receiver's own position: the ball must travel *through* him. Direction
    // is what is asserted here; the aim point itself is off the pitch by design.
    CHECK(s.ball.dest_x == s.ball.pos.x.Whole());
    CHECK(s.ball.dest_y < 400);
    CHECK(s.ball.delta.z.Raw() == kBallPassingDeltaZRaw);
    CHECK(s.ball.pos.z.Whole() == 0);
}
