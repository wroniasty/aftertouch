// B4: speed tables and on-ball modifier.
#include <doctest/doctest.h>

#include "core/movement.hpp"

using namespace at;

TEST_CASE("in-progress speed table by attribute") {
    MatchState s{};
    SetPl(s, GameStatePl::InProgress);
    s.players[0].team_number = 1;
    s.players[0].player_ordinal = 1;
    s.sides[0].squad[0].attrs.speed = 0;
    CHECK(LookupPlayerSpeed(s, 0, false) == 928);
    s.sides[0].squad[0].attrs.speed = 7;
    CHECK(LookupPlayerSpeed(s, 0, false) == 1250);
    // Defence in depth: an out-of-range attribute cannot reach a table index.
    // A5's validator makes this unreachable through the data path (B13 / R2);
    // a hand-built state like this one is exactly why the clamp stays.
    s.sides[0].squad[0].attrs.speed = 15;
    CHECK(LookupPlayerSpeed(s, 0, false) == 1250);
}

TEST_CASE("stopped speed table is flatter") {
    MatchState s{};
    SetPl(s, GameStatePl::Stopped);
    s.players[0].team_number = 1;
    s.players[0].player_ordinal = 1;
    s.sides[0].squad[0].attrs.speed = 0;
    CHECK(LookupPlayerSpeed(s, 0, false) == 1136);
    s.sides[0].squad[0].attrs.speed = 7;
    CHECK(LookupPlayerSpeed(s, 0, false) == 1248);
}

TEST_CASE("controlled with ball loses 12.5 percent") {
    MatchState s{};
    SetPl(s, GameStatePl::InProgress);
    s.players[0].team_number = 1;
    s.players[0].player_ordinal = 1;
    s.sides[0].squad[0].attrs.speed = 7;
    s.sides[0].control.player_has_ball = 1;
    // 1250 - 1250/8 = 1250 - 156 = 1094
    CHECK(LookupPlayerSpeed(s, 0, true) == 1094);
    CHECK(LookupPlayerSpeed(s, 0, false) == 1250);
}
