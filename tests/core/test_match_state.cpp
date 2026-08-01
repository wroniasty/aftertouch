// B1: arena layout, accessors, unique-rep (core only — no at_data).
#include <doctest/doctest.h>

#include <type_traits>

#include "core/match_state.hpp"

using namespace at;

TEST_CASE("B1 types have unique object representation") {
    CHECK(std::has_unique_object_representations_v<Entity>);
    CHECK(std::has_unique_object_representations_v<TeamControl>);
    CHECK(std::has_unique_object_representations_v<SquadPlayer>);
    CHECK(std::has_unique_object_representations_v<TacticsSnapshot>);
    CHECK(std::has_unique_object_representations_v<MatchGlobals>);
    CHECK(std::has_unique_object_representations_v<MatchSide>);
    CHECK(std::has_unique_object_representations_v<MatchState>);
    CHECK(std::is_trivially_copyable_v<MatchState>);
}

TEST_CASE("B1 accessors address the fixed arena") {
    MatchState s{};
    s.ball.speed = 7;
    s.players[3].speed = 11;
    s.referee.team_number = 3;
    s.sides[0].control.controlled_slot = 3;
    s.sides[1].squad[5].shirt_number = 9;

    CHECK(s.Ball().speed == 7);
    CHECK(s.Player(3).speed == 11);
    CHECK(s.Referee().team_number == 3);
    REQUIRE(s.Controlled(0) != nullptr);
    CHECK(s.Controlled(0)->speed == 11);
    CHECK(s.Squad(1, 5).shirt_number == 9);
    CHECK(s.Controlled(1) == nullptr);
}

TEST_CASE("B1 default state zeros gameplay fields") {
    MatchState s{};
    CHECK(s.tick == 0);
    CHECK(s.phase == MatchPhase::KickOff);
    CHECK(s.ball.pos.x.Raw() == 0);
    CHECK(s.score[0] == 0);
    CHECK(s.sides[0].control.controlled_slot == -1);
    CHECK(s.sides[0].control.spin_timer == -1);
    CHECK(s.globals.booked_player == -1);
    CHECK(s.globals.marked_player_home == -1);
}
