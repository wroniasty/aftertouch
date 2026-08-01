// B4: controlled-player boundary stop mask.
#include <doctest/doctest.h>

#include "core/movement.hpp"

using namespace at;

TEST_CASE("outbound direction at left edge is blocked") {
    CHECK((BoundaryMask(70, 400) & kMaskLeft) != 0);
    CHECK((BoundaryMask(70, 400) & (1u << 6)) != 0); // W
}

TEST_CASE("inbound directions at left edge remain free") {
    const uint8_t m = BoundaryMask(70, 400);
    CHECK((m & (1u << 2)) == 0); // E free
}

TEST_CASE("controlled player stops instead of leaving pitch") {
    MatchState s{};
    SetPl(s, GameStatePl::InProgress);
    s.sides[0].control.player_number = 1;
    s.sides[0].control.controlled_slot = 0;
    s.players[0].team_number = 1;
    s.players[0].player_ordinal = 2;
    s.players[0].pos.x = Fix::FromInt(70);
    s.players[0].pos.y = Fix::FromInt(400);
    s.players[0].player_state = static_cast<uint8_t>(PlayerState::Normal);

    MatchInput in{};
    in.p1.dir = Dir::W; // outbound
    s.globals.team_switch_counter = 1;
    ApplyTeamControls(s, in);

    CHECK(s.players[0].dest_x == 70);
    CHECK(s.players[0].dest_y == 400);
    CHECK(s.players[0].is_moving == 0);
}
