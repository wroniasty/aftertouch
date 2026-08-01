// B3: bounce settle threshold, d|=1, restitution factors.
#include <doctest/doctest.h>

#include "core/ball.hpp"

using namespace at;

TEST_CASE("bounce settles at or below 0xA000 rebound") {
    MatchState s{};
    s.surface = MatchSurface{}; // Normal 64 / 96
    // Start barely above ground with small downward velocity so post-gravity
    // integration crosses zero with a weak rebound.
    s.ball.pos.z = Fix::FromRaw(1000);
    s.ball.delta.z = Fix::FromRaw(-5000);
    s.ball.speed = 500;

    IntegrateZAndBounce(s);

    CHECK(s.ball.pos.z.Raw() == 0);
    CHECK(s.ball.delta.z.Raw() == 0);
}

TEST_CASE("bounce keeps d|=1 when rebound is above settle") {
    MatchState s{};
    s.surface = MatchSurface{};
    s.ball.pos.z = Fix::FromRaw(1000);
    s.ball.delta.z = Fix::FromRaw(-90000);
    s.ball.speed = 1000;

    IntegrateZAndBounce(s);

    CHECK(s.ball.pos.z.Raw() == 0);
    CHECK(s.ball.delta.z.Raw() != 0);
    CHECK((s.ball.delta.z.Raw() & 1) == 1);
    CHECK(s.ball.delta.z.Raw() > kBounceSettleThreshold);
}

TEST_CASE("bounce applies horizontal speed loss") {
    MatchState s{};
    s.surface.ball_speed_bounce_factor = 64;
    s.surface.ball_bounce_factor = 96;
    s.ball.pos.z = Fix::FromRaw(1000);
    s.ball.delta.z = Fix::FromRaw(-90000);
    s.ball.speed = 256;

    IntegrateZAndBounce(s);

    // speed -= (256 * 64) >> 8 = 64 → 192
    CHECK(s.ball.speed == 192);
}
