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

// B13 / R2: this case used to set heading = 8 and assert `base + 513` — the
// phantom entry from the 13-value mis-read, at an attribute value that is not
// legal. It asserts the *shape* now: the top attribute is the zero point and
// everything below it is a penalty, so the header can never exceed its base.
static int16_t JumpHeaderSpeedAt(uint8_t heading_attr) {
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
    s.sides[0].squad[1].attrs.heading = heading_attr;
    s.ball.pos.x = Fix::FromInt(336);
    s.ball.pos.y = Fix::FromInt(449);

    ApplyHeaderContact(s, 0, 0);
    return s.ball.speed;
}

TEST_CASE("jump header is 125 percent of player speed at the top attribute") {
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
    s.sides[0].squad[1].attrs.heading = kAttrMax; // the zero point, not a bonus
    s.ball.pos.x = Fix::FromInt(336);
    s.ball.pos.y = Fix::FromInt(449);

    ApplyHeaderContact(s, 0, 0);

    const int32_t base = 1600 + (1600 >> 2);
    CHECK(s.ball.speed == static_cast<int16_t>(base));
    CHECK(s.players[0].speed == 800);
    CHECK(s.ball.delta.z.Raw() == kBallJumpHeaderDeltaZRaw);
}

TEST_CASE("heading is a handicap ramp with no upside") {
    const int32_t base = 1600 + (1600 >> 2);
    const int16_t top = JumpHeaderSpeedAt(kAttrMax);
    CHECK(top == static_cast<int16_t>(base));

    // Monotonic, and never above the base at any legal attribute value.
    int16_t prev = JumpHeaderSpeedAt(0);
    CHECK(prev < top);
    for (uint8_t a = 1; a <= kAttrMax; ++a) {
        const int16_t here = JumpHeaderSpeedAt(a);
        CHECK(here >= prev);
        CHECK(here <= top);
        prev = here;
    }
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
