// B5: dribble aim-ahead and Control speed trim.
#include <doctest/doctest.h>

#include "core/possession.hpp"

using namespace at;

TEST_CASE("dribble sets ball dest ahead of facing") {
    MatchState s{};
    SetPl(s, GameStatePl::InProgress);
    s.sides[0].control.player_has_ball = 1;
    s.sides[0].control.pl_very_close_to_ball = 1;
    s.sides[0].control.controlled_slot = 0;
    s.sides[0].control.current_allowed_direction = static_cast<int16_t>(Dir::E);
    s.players[0].team_number = 1;
    s.players[0].player_ordinal = 2;
    s.players[0].pos.x = Fix::FromInt(300);
    s.players[0].pos.y = Fix::FromInt(400);
    s.players[0].direction = static_cast<int16_t>(Dir::E);
    s.players[0].speed = 1000;
    s.ball.pos.x = Fix::FromInt(300);
    s.ball.pos.y = Fix::FromInt(400);
    s.tick = 0;

    ApplyDribble(s, 0);

    // Dest only — ball position is not snapped to the player.
    CHECK(s.ball.pos.x.Whole() == 300);
    CHECK(s.ball.dest_x == 1301); // nudge (1,0) + ahead (1000,0)
    CHECK(s.ball.dest_y == 400);
}

TEST_CASE("Control offset rides on carrier speed") {
    MatchState s{};
    SetPl(s, GameStatePl::InProgress);
    s.sides[0].control.player_has_ball = 1;
    s.sides[0].control.pl_very_close_to_ball = 1;
    s.sides[0].control.controlled_slot = 0;
    s.sides[0].control.current_allowed_direction = static_cast<int16_t>(Dir::N);
    s.sides[0].squad[1].attrs.ball_control = 0; // ordinal 2 → squad[1]
    s.players[0].team_number = 1;
    s.players[0].player_ordinal = 2;
    s.players[0].pos.x = Fix::FromInt(300);
    s.players[0].pos.y = Fix::FromInt(400);
    s.players[0].direction = 0;
    s.players[0].speed = 1000;
    s.ball.speed = 500;
    s.tick = 2;

    ApplyDribble(s, 0);
    CHECK(s.ball.speed == 1000 + kBallSpeedDeltaWhenControlled[0]); // 1130
}

TEST_CASE("released stick leaves ball rolling") {
    MatchState s{};
    SetPl(s, GameStatePl::InProgress);
    s.sides[0].control.player_has_ball = 1;
    s.sides[0].control.pl_very_close_to_ball = 1;
    s.sides[0].control.controlled_slot = 0;
    s.sides[0].control.current_allowed_direction = -1;
    s.players[0].team_number = 1;
    s.players[0].pos.x = Fix::FromInt(300);
    s.players[0].pos.y = Fix::FromInt(400);
    s.ball.speed = 900;
    s.ball.dest_x = 1300;
    s.ball.dest_y = 400;

    ApplyDribble(s, 0);
    CHECK(s.ball.speed == 900);
    CHECK(s.ball.dest_x == 1300);
    CHECK(s.ball.dest_y == 400);
}

TEST_CASE("cannot redirect when only pl_close") {
    MatchState s{};
    SetPl(s, GameStatePl::InProgress);
    s.sides[0].control.player_has_ball = 1;
    s.sides[0].control.pl_very_close_to_ball = 0;
    s.sides[0].control.pl_close_to_ball = 1;
    s.sides[0].control.controlled_slot = 0;
    s.sides[0].control.current_allowed_direction = static_cast<int16_t>(Dir::W);
    s.players[0].team_number = 1;
    s.players[0].pos.x = Fix::FromInt(300);
    s.players[0].pos.y = Fix::FromInt(400);
    s.players[0].speed = 1000;
    s.ball.speed = 900;
    s.ball.dest_x = 1300;
    s.ball.dest_y = 400;

    ApplyDribble(s, 0);
    CHECK(s.ball.dest_x == 1300); // unchanged — no yank on a wide turn
    CHECK(s.ball.speed == 900);
}

TEST_CASE("GiveBallForTest grants possession at feet") {
    MatchState s{};
    s.players[3].pos.x = Fix::FromInt(250);
    s.players[3].pos.y = Fix::FromInt(450);
    GiveBallForTest(s, 0, 3);
    CHECK(s.sides[0].control.player_has_ball == 1);
    CHECK(s.ball.pos.x.Whole() == 250);
    CHECK(s.sides[0].control.pl_very_close_to_ball == 1);
}
