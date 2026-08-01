// B3: dead-ball barrier — Stopped only; dest rewrite + speed/2.
#include <doctest/doctest.h>

#include "core/ball.hpp"

using namespace at;

TEST_CASE("barrier inactive during InProgress") {
    MatchState s{};
    SetPl(s, GameStatePl::InProgress);
    s.ball.pos.x = Fix::FromInt(10); // outside barrier
    s.ball.pos.y = Fix::FromInt(400);
    s.ball.dest_x = 0;
    s.ball.dest_y = 400;
    s.ball.speed = 200;
    const int16_t dest_before = s.ball.dest_x;
    ApplyDeadBallBarrier(s, s.ball.pos);
    CHECK(s.ball.dest_x == dest_before);
    CHECK(s.ball.speed == 200);
}

TEST_CASE("barrier mirrors dest and halves speed when Stopped") {
    MatchState s{};
    SetPl(s, GameStatePl::Stopped);
    // Post-integrate position outside left barrier; saved is inside.
    Vec3 saved{};
    saved.x = Fix::FromInt(100);
    saved.y = Fix::FromInt(400);
    s.ball.pos.x = Fix::FromInt(40);
    s.ball.pos.y = Fix::FromInt(400);
    s.ball.dest_x = 0;
    s.ball.dest_y = 400;
    s.ball.speed = 200;

    ApplyDeadBallBarrier(s, saved);

    // dest_x = 2*40 - 0 = 80
    CHECK(s.ball.dest_x == 80);
    CHECK(s.ball.speed == 100);
    CHECK(s.ball.pos.x.Whole() == 100);
    CHECK(s.ball.pos.y.Whole() == 400);
}
