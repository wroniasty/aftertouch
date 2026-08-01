// B7: slide ball contact deflects at 125% in held direction.
#include <doctest/doctest.h>

#include "core/ball.hpp"
#include "core/possession.hpp"
#include "core/tackling.hpp"

using namespace at;

TEST_CASE("tackle ball contact deflects at 125 percent held dir") {
    MatchState s{};
    SetPl(s, GameStatePl::InProgress);
    s.sides[0].control.controlled_slot = 0;
    s.sides[0].control.current_allowed_direction = static_cast<int16_t>(Dir::NE);
    s.sides[1].control.controlled_slot = 11;
    s.players[0].team_number = 1;
    s.players[0].player_ordinal = 2;
    s.players[0].pos.x = Fix::FromInt(336);
    s.players[0].pos.y = Fix::FromInt(449);
    s.players[0].direction = static_cast<int16_t>(Dir::N);
    s.players[0].speed = 1600;
    s.players[0].player_state = static_cast<uint8_t>(PlayerState::Tackling);
    s.players[0].tackle_state = kTackleStateNone;
    s.ball.pos.x = Fix::FromInt(336);
    s.ball.pos.y = Fix::FromInt(449);
    s.ball.pos.z = Fix{};
    // Opponent far from ball → free deflect (not contest).
    s.players[11].team_number = 2;
    s.players[11].player_ordinal = 2;
    s.players[11].pos.x = Fix::FromInt(100);
    s.players[11].pos.y = Fix::FromInt(100);
    s.players[11].ball_distance = 10000;

    ApplyTackleBallContact(s, 0, 0);

    // Far opponent → free deflect promotes to good tackle.
    CHECK(s.players[0].tackle_state == kTackleStateGood);
    CHECK(s.players[0].speed == 800);
    CHECK(s.ball.direction == static_cast<int16_t>(Dir::NE));
    CHECK(s.ball.speed == static_cast<int16_t>(1600 + (1600 >> 2)));
    CHECK(s.ball.dest_x == static_cast<int16_t>(336 + 1000));
    CHECK(s.ball.dest_y == static_cast<int16_t>(449 - 1000));
    CHECK(s.sides[0].control.spin_timer == -1);
}

TEST_CASE("good tackle when opponent not close") {
    MatchState s{};
    SetPl(s, GameStatePl::InProgress);
    s.sides[0].control.controlled_slot = 0;
    s.sides[0].control.current_allowed_direction = static_cast<int16_t>(Dir::N);
    s.sides[1].control.controlled_slot = 11;
    s.players[0].team_number = 1;
    s.players[0].player_ordinal = 2;
    s.players[0].pos.x = Fix::FromInt(336);
    s.players[0].pos.y = Fix::FromInt(449);
    s.players[0].direction = static_cast<int16_t>(Dir::N);
    s.players[0].speed = 1600;
    s.players[0].player_state = static_cast<uint8_t>(PlayerState::Tackling);
    s.ball.pos.x = Fix::FromInt(336);
    s.ball.pos.y = Fix::FromInt(449);
    s.players[11].team_number = 2;
    s.players[11].player_ordinal = 2;
    s.players[11].pos.x = Fix::FromInt(400);
    s.players[11].pos.y = Fix::FromInt(500);
    s.players[11].ball_distance = 50; // >= 9

    ApplyTackleBallContact(s, 0, 0);
    CHECK(s.players[0].tackle_state == kTackleStateGood);
}
