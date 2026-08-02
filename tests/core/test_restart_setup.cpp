// B8: four writes + throw-in taker placement.
#include <doctest/doctest.h>

#include "core/set_pieces.hpp"

using namespace at;

TEST_CASE("BeginRestart writes four fields and parks ball") {
    MatchState s{};
    BeginRestart(s, GameState::CornerLeft, 81, 129, 0x38, 4, 1);
    CHECK(GetGameState(s) == GameState::CornerLeft);
    CHECK(GetPl(s) == GameStatePl::Stopped);
    CHECK(s.globals.foul_x == 81);
    CHECK(s.globals.foul_y == 129);
    CHECK(s.globals.player_turn_flags == 0x38);
    CHECK(s.globals.camera_direction == 4);
    CHECK(s.globals.last_team_played_before_break == 1);
    CHECK(s.globals.break_camera_mode == kBreakCameraModeRestart);
    CHECK(s.ball.pos.x.Whole() == 81);
    CHECK(s.ball.speed == 0);
}

TEST_CASE("throw-in taker placed three units off pitch") {
    MatchState s{};
    s.sides[0].control.controlled_slot = 1;
    s.players[1].team_number = 1;
    s.players[1].player_ordinal = 2;
    s.ball.pos.x = Fix::FromInt(70);
    s.ball.pos.y = Fix::FromInt(400);
    BeginRestart(s, GameState::ThrowInCentreLeft, 70, 400, 0x1C, 4, 1);
    PlaceThrowInTaker(s, 0);
    CHECK(s.players[1].pos.x.Whole() == 67);
    CHECK(s.players[1].pos.y.Whole() == 400);
    CHECK(static_cast<PlayerState>(s.players[1].player_state) ==
          PlayerState::ThrowIn);
    CHECK(s.globals.hide_ball == 1);
}
