// Restart take: stick aims in place — no translation (B8 / play-feel).
#include <doctest/doctest.h>

#include "core/movement.hpp"
#include "core/set_pieces.hpp"

using namespace at;

TEST_CASE("restart take: held direction aims without moving") {
    MatchState s{};
    BeginRestart(s, GameState::FreeKickCentre, 336, 400, kTurnFlagsAll, 0, 1);
    PickRestartTaker(s, 0);
    PlaceTakerNearSpot(s, 0);

    TeamControl& tc = s.sides[0].control;
    tc.player_number = 1;
    const int slot = tc.controlled_slot;
    REQUIRE(slot >= 0);
    REQUIRE(slot != 0); // outfield, not GK
    Entity& e = s.players[static_cast<size_t>(slot)];
    const int16_t x0 = e.pos.x.Whole();
    const int16_t y0 = e.pos.y.Whole();

    MatchInput in{};
    in.p1.dir = Dir::E;
    for (int i = 0; i < 10; ++i) {
        s.globals.team_switch_counter = 1; // home side next
        ApplyTeamControls(s, in);
        MovePlayers(s);
    }

    CHECK(e.pos.x.Whole() == x0);
    CHECK(e.pos.y.Whole() == y0);
    CHECK(e.dest_x == x0);
    CHECK(e.dest_y == y0);
    CHECK(tc.current_allowed_direction == static_cast<int16_t>(Dir::E));
    CHECK(e.direction == static_cast<int16_t>(Dir::E));
}

TEST_CASE("goal kick picks outfield taker not GK") {
    MatchState s{};
    s.sides[0].control.player_number = 1;
    s.sides[0].control.controlled_slot = 0; // stuck on GK
    s.players[0].team_number = 1;
    s.players[0].player_ordinal = 1;
    for (int i = 1; i < 11; ++i) {
        s.players[static_cast<size_t>(i)].team_number = 1;
        s.players[static_cast<size_t>(i)].player_ordinal =
            static_cast<int16_t>(i + 1);
        s.players[static_cast<size_t>(i)].pos.x = Fix::FromInt(200 + i * 10);
        s.players[static_cast<size_t>(i)].pos.y = Fix::FromInt(150);
    }

    BeginRestart(s, GameState::GoalOutLeft, 336, 129, kTurnFlagsAll, 0, 1);
    PickRestartTaker(s, 0);
    CHECK(s.sides[0].control.controlled_slot != 0);
    CHECK(s.sides[0].control.controlled_slot >= 1);
    CHECK(s.sides[0].control.controlled_slot < 11);
}

TEST_CASE("keeper holds keeps GK as taker") {
    MatchState s{};
    BeginRestart(s, GameState::KeeperHoldsBall, 336, 180, 0xFF, 0, 1);
    PickRestartTaker(s, 0);
    CHECK(s.sides[0].control.controlled_slot == 0);
}
