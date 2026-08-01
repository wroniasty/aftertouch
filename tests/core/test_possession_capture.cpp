// B5: capture, release, lockout.
#include <doctest/doctest.h>

#include "core/possession.hpp"

using namespace at;

namespace {

MatchState MakeNearBall() {
    MatchState s{};
    SetPl(s, GameStatePl::InProgress);
    s.sides[0].control.controlled_slot = 0;
    s.sides[0].control.ball_can_be_controlled = 1;
    s.sides[0].control.pass_kick_timer = 0;
    s.players[0].team_number = 1;
    s.players[0].player_ordinal = 2;
    s.players[0].player_state = static_cast<uint8_t>(PlayerState::Normal);
    s.players[0].pos.x = Fix::FromInt(200);
    s.players[0].pos.y = Fix::FromInt(400);
    s.ball.pos.x = Fix::FromInt(201);
    s.ball.pos.y = Fix::FromInt(400);
    return s;
}

} // namespace

TEST_CASE("very close captures the ball") {
    MatchState s = MakeNearBall();
    UpdatePossessionForSide(s, 0);
    CHECK(s.sides[0].control.player_has_ball == 1);
    CHECK(s.clock.last_team_played == 1);
}

TEST_CASE("leaving close band releases possession") {
    MatchState s = MakeNearBall();
    UpdatePossessionForSide(s, 0);
    REQUIRE(s.sides[0].control.player_has_ball == 1);

    // Move ball far away.
    s.ball.pos.x = Fix::FromInt(400);
    UpdatePossessionForSide(s, 0);
    CHECK(s.sides[0].control.player_has_ball == 0);
    CHECK(s.sides[0].control.ball_out_of_play == 1);
}

TEST_CASE("pass_kick_timer lockout blocks capture") {
    MatchState s = MakeNearBall();
    s.sides[0].control.pass_kick_timer = 3;
    s.sides[0].control.ball_can_be_controlled = 0;
    UpdatePossessionForSide(s, 0);
    CHECK(s.sides[0].control.player_has_ball == 0);
    CHECK(s.sides[0].control.pass_kick_timer == 2);

    UpdatePossessionForSide(s, 0); // 1
    UpdatePossessionForSide(s, 0); // 0 → controllable
    CHECK(s.sides[0].control.pass_kick_timer == 0);
    CHECK(s.sides[0].control.ball_can_be_controlled == 1);
    UpdatePossessionForSide(s, 0);
    CHECK(s.sides[0].control.player_has_ball == 1);
}
