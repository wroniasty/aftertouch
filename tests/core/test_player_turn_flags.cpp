// B4: turn flags only when play is stopped.
#include <doctest/doctest.h>

#include "core/movement.hpp"

using namespace at;

TEST_CASE("turn flags walk down from 7") {
    CHECK(ApplyTurnFlags(2, 0x01) == 0); // only N allowed
    CHECK(ApplyTurnFlags(4, 0x80) == 7); // only NW
    CHECK(ApplyTurnFlags(3, 0xFF) == 3);
}

TEST_CASE("turn flags ignored during InProgress") {
    MatchState s{};
    SetPl(s, GameStatePl::InProgress);
    s.globals.player_turn_flags = 0x01; // only N
    s.sides[0].control.player_number = 1;
    s.sides[0].control.controlled_slot = 0;
    s.players[0].team_number = 1;
    s.players[0].player_ordinal = 2;
    s.players[0].pos.x = Fix::FromInt(300);
    s.players[0].pos.y = Fix::FromInt(400);
    s.players[0].player_state = static_cast<uint8_t>(PlayerState::Normal);

    MatchInput in{};
    in.p1.dir = Dir::E;
    s.globals.team_switch_counter = 1;
    ApplyTeamControls(s, in);

    CHECK(s.players[0].dest_x == 1300); // E allowed in play
}

TEST_CASE("turn flags rewrite direction when Stopped") {
    MatchState s{};
    SetPl(s, GameStatePl::Stopped);
    s.globals.player_turn_flags = 0x01; // only N
    s.sides[0].control.player_number = 1;
    s.sides[0].control.controlled_slot = 0;
    s.players[0].team_number = 1;
    s.players[0].player_ordinal = 2;
    s.players[0].pos.x = Fix::FromInt(300);
    s.players[0].pos.y = Fix::FromInt(400);
    s.players[0].player_state = static_cast<uint8_t>(PlayerState::Normal);

    MatchInput in{};
    in.p1.dir = Dir::E;
    s.globals.team_switch_counter = 1;
    ApplyTeamControls(s, in);

    // Rewritten to N → dest y - 1000
    CHECK(s.sides[0].control.current_allowed_direction == 0);
    CHECK(s.players[0].dest_x == 300);
    CHECK(s.players[0].dest_y == -600);
}
