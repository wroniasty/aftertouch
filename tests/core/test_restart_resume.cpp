// B8: restart take returns to InProgress.
#include <doctest/doctest.h>

#include "core/set_pieces.hpp"

using namespace at;

TEST_CASE("free kick take resumes open play") {
    MatchState s{};
    BeginRestart(s, GameState::FreeKickCentre, 336, 250, kTurnFlagsAll, 4, 1);
    s.sides[0].control.controlled_slot = 1;
    s.sides[0].control.current_allowed_direction = static_cast<int16_t>(Dir::N);
    s.sides[0].control.quick_fire = 1;
    s.players[1].team_number = 1;
    s.players[1].pos.x = Fix::FromInt(336);
    s.players[1].pos.y = Fix::FromInt(250);
    s.players[1].direction = static_cast<int16_t>(Dir::N);

    CHECK(ApplyRestartTake(s, 0));
    CHECK(GetPl(s) == GameStatePl::InProgress);
    CHECK(GetGameState(s) == GameState::StartingGame);
    CHECK(s.ball.speed == kRestartKickSpeed);
    CHECK(s.globals.hide_ball == 0);
    CHECK(s.globals.player_turn_flags == kTurnFlagsAll);
}

TEST_CASE("throw-in take clears hide ball") {
    MatchState s{};
    BeginRestart(s, GameState::ThrowInCentreLeft, 70, 400, 0x1C, 4, 1);
    PlaceThrowInTaker(s, 0);
    s.sides[0].control.controlled_slot = 1;
    s.sides[0].control.fire_this_frame = 1;
    s.sides[0].control.current_allowed_direction = static_cast<int16_t>(Dir::E);
    s.players[1].team_number = 1;
    s.players[1].pos.x = Fix::FromInt(67);
    s.players[1].pos.y = Fix::FromInt(400);

    CHECK(ApplyRestartTake(s, 0));
    CHECK(GetPl(s) == GameStatePl::InProgress);
    CHECK(s.globals.hide_ball == 0);
    CHECK(static_cast<PlayerState>(s.players[1].player_state) ==
          PlayerState::Normal);
}
