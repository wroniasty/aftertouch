// B3: friction — ground / air / pitch factor (Amiga constants).
#include <doctest/doctest.h>

#include "core/ball.hpp"
#include "core/profile.hpp"

using namespace at;

namespace {

MatchState MakeRolling() {
    MatchState s{};
    s.surface = MatchSurface{}; // Normal
    s.ball.pos.x = Fix::FromInt(kCentreSpotX);
    s.ball.pos.y = Fix::FromInt(kCentreSpotY);
    s.ball.pos.z = Fix{};
    s.ball.dest_x = static_cast<int16_t>(kCentreSpotX + 50);
    s.ball.dest_y = kCentreSpotY;
    s.ball.speed = 1000;
    SetPl(s, GameStatePl::InProgress);
    return s;
}

} // namespace

TEST_CASE("amiga ground friction subtracts kBallGroundConstant") {
    REQUIRE(IsAmigaProfile());
    MatchState s = MakeRolling();
    ApplyFriction(s);
    CHECK(s.ball.speed == static_cast<int16_t>(1000 - kBallGroundConstant));
}

TEST_CASE("loose-ball pitch factor adds to ground friction") {
    MatchState s = MakeRolling();
    s.surface.pitch_ball_speed_factor = 4; // muddy-like
    ApplyFriction(s);
    CHECK(s.ball.speed == static_cast<int16_t>(1000 - (kBallGroundConstant + 4)));
}

TEST_CASE("player_has_ball skips pitch factor") {
    MatchState s = MakeRolling();
    s.surface.pitch_ball_speed_factor = 4;
    s.sides[0].control.player_has_ball = 1;
    ApplyFriction(s);
    CHECK(s.ball.speed == static_cast<int16_t>(1000 - kBallGroundConstant));
}

TEST_CASE("air friction replaces ground and pitch factor") {
    MatchState s = MakeRolling();
    s.surface.pitch_ball_speed_factor = 4;
    s.ball.pos.z = Fix::FromInt(3);
    ApplyFriction(s);
    CHECK(s.ball.speed == static_cast<int16_t>(1000 - kBallAirConstant));
}

TEST_CASE("friction clamps speed at zero") {
    MatchState s = MakeRolling();
    s.ball.speed = 5;
    ApplyFriction(s);
    CHECK(s.ball.speed == 0);
}
