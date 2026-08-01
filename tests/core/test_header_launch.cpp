// B7: static/jump header ball effects.
#include <doctest/doctest.h>

#include "core/heading.hpp"

using namespace at;

TEST_CASE("static header flat speed and reflected delta z") {
    MatchState s{};
    SetPl(s, GameStatePl::InProgress);
    s.sides[0].control.controlled_slot = 0;
    s.sides[0].control.current_allowed_direction = static_cast<int16_t>(Dir::N);
    s.players[0].team_number = 1;
    s.players[0].player_ordinal = 2;
    s.players[0].pos.x = Fix::FromInt(336);
    s.players[0].pos.y = Fix::FromInt(449);
    s.players[0].direction = static_cast<int16_t>(Dir::N);
    s.players[0].player_state = static_cast<uint8_t>(PlayerState::StaticHeader);
    s.players[0].heading = 0;
    s.sides[0].squad[1].attrs.heading = 7; // zero bonus
    s.ball.pos.x = Fix::FromInt(336);
    s.ball.pos.y = Fix::FromInt(449);
    s.ball.delta.z = Fix::FromRaw(100000);

    ApplyHeaderContact(s, 0, 0);

    CHECK(s.players[0].heading == 1);
    CHECK(s.ball.speed == kStaticHeaderBallSpeed);
    CHECK(s.ball.delta.z.Raw() == -(100000 / 2));
    CHECK(s.sides[0].control.spin_timer == -1);
}

TEST_CASE("jump header adds heading attribute and 125 percent base") {
    MatchState s{};
    SetPl(s, GameStatePl::InProgress);
    s.sides[0].control.controlled_slot = 0;
    s.sides[0].control.current_allowed_direction = static_cast<int16_t>(Dir::N);
    s.players[0].team_number = 1;
    s.players[0].player_ordinal = 2;
    s.players[0].pos.x = Fix::FromInt(336);
    s.players[0].pos.y = Fix::FromInt(449);
    s.players[0].direction = static_cast<int16_t>(Dir::N);
    s.players[0].speed = 1600;
    s.players[0].player_state = static_cast<uint8_t>(PlayerState::JumpHeader);
    s.players[0].heading = 0;
    s.sides[0].squad[1].attrs.heading = 8; // +513
    s.ball.pos.x = Fix::FromInt(336);
    s.ball.pos.y = Fix::FromInt(449);

    ApplyHeaderContact(s, 0, 0);

    const int32_t base = 1600 + (1600 >> 2);
    CHECK(s.ball.speed == static_cast<int16_t>(base + 513));
    CHECK(s.players[0].speed == 800);
    CHECK(s.ball.delta.z.Raw() == kBallJumpHeaderDeltaZRaw);
}

TEST_CASE("jump header lob overwrites height") {
    MatchState s{};
    SetPl(s, GameStatePl::InProgress);
    s.sides[0].control.controlled_slot = 0;
    s.sides[0].control.current_allowed_direction = static_cast<int16_t>(Dir::S); // back
    s.players[0].team_number = 1;
    s.players[0].player_ordinal = 2;
    s.players[0].pos.x = Fix::FromInt(336);
    s.players[0].pos.y = Fix::FromInt(449);
    s.players[0].direction = static_cast<int16_t>(Dir::N);
    s.players[0].speed = 1600;
    s.players[0].player_state = static_cast<uint8_t>(PlayerState::JumpHeader);
    s.sides[0].squad[1].attrs.heading = 7;
    s.ball.pos.x = Fix::FromInt(336);
    s.ball.pos.y = Fix::FromInt(449);

    ApplyHeaderContact(s, 0, 0);
    CHECK(s.ball.delta.z.Raw() == kHeaderHighJumpHeightRaw);
}
