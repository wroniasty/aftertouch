// B8: foul → penalty / free-kick / plain Foul.
#include <doctest/doctest.h>

#include "core/set_pieces.hpp"

using namespace at;

TEST_CASE("penalty box upper yields penalty on white spot") {
    MatchState s{};
    SetPl(s, GameStatePl::InProgress);
    s.globals.team_playing_up = 1;
    ClassifyAndBeginFoulRestart(s, 336, 200, /*offending*/ 0);
    CHECK(GetGameState(s) == GameState::Penalty);
    CHECK(GetPl(s) == GameStatePl::Stopped);
    CHECK(s.globals.foul_x == kCentreSpotX);
    CHECK(s.globals.foul_y == kPenaltySpotUpperY);
    CHECK(s.globals.player_turn_flags == kTurnFlagsPenUpper);
    CHECK(s.globals.last_team_played_before_break == 2);
}

TEST_CASE("free kick band selects zone by x") {
    MatchState s{};
    SetPl(s, GameStatePl::InProgress);
    s.globals.team_playing_up = 1;
    ClassifyAndBeginFoulRestart(s, 200, 250, 0);
    CHECK(IsFreeKickState(GetGameState(s)));
    CHECK(s.globals.player_turn_flags == kTurnFlagsAll);
    CHECK(s.globals.foul_x == 200);
}

TEST_CASE("midfield foul is plain Foul state") {
    MatchState s{};
    SetPl(s, GameStatePl::InProgress);
    ClassifyAndBeginFoulRestart(s, 336, 449, 1);
    CHECK(GetGameState(s) == GameState::Foul);
    CHECK(s.globals.last_team_played_before_break == 1);
}
