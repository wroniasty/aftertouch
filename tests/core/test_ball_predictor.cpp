// B3: landing predictor bands (rising ×8) and known landing.
#include <doctest/doctest.h>

#include "core/ball.hpp"

using namespace at;

TEST_CASE("predictor with speed 0 is current position") {
    MatchState s{};
    s.ball.pos.x = Fix::FromInt(200);
    s.ball.pos.y = Fix::FromInt(300);
    s.ball.speed = 0;
    CalculateNextBallPosition(s);
    CHECK(s.globals.ball_next_x == 200);
    CHECK(s.globals.ball_next_y == 300);
    CHECK(s.globals.ball_next_y_ground_y == 300);
}

TEST_CASE("rising ball uses coarse band and still lands") {
    MatchState s{};
    s.ball.pos.x = Fix::FromInt(kCentreSpotX);
    s.ball.pos.y = Fix::FromInt(kCentreSpotY);
    s.ball.pos.z = Fix::FromInt(5);
    s.ball.delta.x = Fix::FromRaw(0);
    s.ball.delta.y = Fix::FromRaw(1 << 16); // +1 per fine step
    s.ball.delta.z = Fix::FromRaw(20000);  // rising
    s.ball.speed = 500;

    CalculateNextBallPosition(s);

    CHECK(s.globals.ball_next_x == kCentreSpotX);
    // Must have advanced in y (coarse steps still integrate forward).
    CHECK(s.globals.ball_next_y > kCentreSpotY);
    CHECK(s.globals.ball_next_y_ground_y == s.globals.ball_next_y);
}

TEST_CASE("grounded horizontal motion predicts along path") {
    MatchState s{};
    s.ball.pos.x = Fix::FromInt(300);
    s.ball.pos.y = Fix::FromInt(400);
    s.ball.pos.z = Fix{};
    s.ball.delta.x = Fix::FromRaw(1 << 16);
    s.ball.delta.y = Fix{};
    s.ball.delta.z = Fix{};
    s.ball.speed = 100;

    // z==0 and delta.z==0: first loop step applies gravity to d3, z goes
    // negative immediately → landing ≈ current + one step.
    CalculateNextBallPosition(s);
    CHECK(s.globals.ball_next_x == 301);
    CHECK(s.globals.ball_next_y == 400);
}
