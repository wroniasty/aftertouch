// B8: aim table selection.
#include <doctest/doctest.h>

#include "core/set_pieces.hpp"

using namespace at;

TEST_CASE("throw-in left selects left table") {
    MatchState s{};
    s.globals.foul_x = 80;
    SetGameState(s, GameState::ThrowInCentreLeft);
    const Dest d = AimDestForDir(s, static_cast<int>(Dir::E));
    CHECK(d.x == 1000);
    CHECK(d.y == 0);
}

TEST_CASE("penalty table halves diagonals") {
    MatchState s{};
    SetGameState(s, GameState::Penalty);
    const Dest ne = AimDestForDir(s, static_cast<int>(Dir::NE));
    CHECK(ne.x == 500);
    CHECK(ne.y == -1000);
}

TEST_CASE("corner uses spot quadrant") {
    MatchState s{};
    SetGameState(s, GameState::CornerLeft);
    s.globals.foul_x = 100;
    s.globals.foul_y = 120; // upper
    const auto& t = AimTableForState(s);
    CHECK(t[0].y == -1000);
}
