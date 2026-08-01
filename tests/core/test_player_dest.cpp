// B4: eight-way destination and immediate stop.
#include <doctest/doctest.h>

#include "core/movement.hpp"

using namespace at;

TEST_CASE("eight directions offset dest by 1000") {
    MatchState s{};
    SetPl(s, GameStatePl::InProgress);
    s.sides[0].control.player_number = 1;
    s.sides[0].control.controlled_slot = 0;
    s.players[0].team_number = 1;
    s.players[0].player_ordinal = 2;
    s.players[0].pos.x = Fix::FromInt(300);
    s.players[0].pos.y = Fix::FromInt(400);
    s.players[0].player_state = static_cast<uint8_t>(PlayerState::Normal);

    MatchInput in{};
    in.p1.dir = Dir::E;
    // Force side 0: counter becomes odd→side1 first; call twice or set counter.
    s.globals.team_switch_counter = 1; // ++ → 2, side 0
    ApplyTeamControls(s, in);

    CHECK(s.players[0].dest_x == 1300);
    CHECK(s.players[0].dest_y == 400);
    CHECK(s.players[0].speed == LookupPlayerSpeed(s, 0, true));
}

TEST_CASE("no input stops on the same decision tick") {
    MatchState s{};
    SetPl(s, GameStatePl::InProgress);
    s.sides[0].control.player_number = 1;
    s.sides[0].control.controlled_slot = 0;
    s.players[0].team_number = 1;
    s.players[0].player_ordinal = 2;
    s.players[0].pos.x = Fix::FromInt(300);
    s.players[0].pos.y = Fix::FromInt(400);
    s.players[0].player_state = static_cast<uint8_t>(PlayerState::Normal);
    s.players[0].delta.x = Fix::FromRaw(1000);
    s.players[0].speed = 1000;

    MatchInput in{};
    in.p1.dir = Dir::None;
    s.globals.team_switch_counter = 1;
    ApplyTeamControls(s, in);

    CHECK(s.players[0].dest_x == 300);
    CHECK(s.players[0].dest_y == 400);
    CHECK(s.players[0].delta.x.Raw() == 0);
    CHECK(s.players[0].delta.y.Raw() == 0);
    CHECK(s.players[0].is_moving == 0);
}
