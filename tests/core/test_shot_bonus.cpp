// B6: Finishing inside box vs Velocity outside.
#include <doctest/doctest.h>

#include "core/shooting.hpp"

using namespace at;

TEST_CASE("InPenaltyBox recognises top and bottom areas") {
    CHECK(InPenaltyBox(336, 200));
    CHECK(InPenaltyBox(336, 700));
    CHECK_FALSE(InPenaltyBox(336, 449)); // centre
}

TEST_CASE("in-box goalward shot adds Finishing bonus") {
    MatchState s{};
    SetPl(s, GameStatePl::InProgress);
    s.globals.team_playing_up = 1;
    s.sides[0].control.controlled_slot = 0;
    s.sides[0].control.current_allowed_direction = static_cast<int16_t>(Dir::N);
    s.sides[0].control.pl_very_close_to_ball = 1;
    s.sides[0].control.player_has_ball = 1;
    s.sides[0].control.normal_fire = 1;
    s.sides[0].squad[0].attrs.finishing = 4;
    s.sides[0].squad[0].attrs.shooting = 7;
    s.players[0].team_number = 1;
    s.players[0].player_ordinal = 1;
    s.players[0].pos.x = Fix::FromInt(336);
    s.players[0].pos.y = Fix::FromInt(200);
    s.ball.pos.x = Fix::FromInt(336);
    s.ball.pos.y = Fix::FromInt(200);

    CHECK(ApplyKickOrPass(s, 0));
    CHECK(s.ball.speed ==
          kBallKickingSpeed + kBallSpeedFinishing[4]);
}

TEST_CASE("outside-box goalward shot adds Velocity bonus") {
    MatchState s{};
    SetPl(s, GameStatePl::InProgress);
    s.globals.team_playing_up = 1;
    s.sides[0].control.controlled_slot = 0;
    s.sides[0].control.current_allowed_direction = static_cast<int16_t>(Dir::N);
    s.sides[0].control.pl_very_close_to_ball = 1;
    s.sides[0].control.player_has_ball = 1;
    s.sides[0].control.normal_fire = 1;
    s.sides[0].squad[1].attrs.finishing = 7;
    s.sides[0].squad[1].attrs.shooting = 3;
    s.players[0].team_number = 1;
    s.players[0].player_ordinal = 2;
    s.players[0].pos.x = Fix::FromInt(336);
    s.players[0].pos.y = Fix::FromInt(449);
    s.ball.pos.x = Fix::FromInt(336);
    s.ball.pos.y = Fix::FromInt(449);

    CHECK(ApplyKickOrPass(s, 0));
    CHECK(s.ball.speed == kBallKickingSpeed + kBallSpeedKicking[3]);
}
