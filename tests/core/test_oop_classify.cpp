// B2: pure out-of-play classification (not wired into Step).
#include <doctest/doctest.h>

#include "core/out_of_play.hpp"

using namespace at;

TEST_CASE("ball inside playable area is not out of play") {
    CHECK_FALSE(ClassifyBallOutOfPlay(kCentreSpotX, kCentreSpotY, 0, 1).has_value());
    CHECK_FALSE(ClassifyBallOutOfPlay(200, 400, 0, 1).has_value());
}

TEST_CASE("touchline maps to throw-in thirds") {
    auto r = ClassifyBallOutOfPlay(50, 200, 0, 1); // left, forward third
    REQUIRE(r.has_value());
    CHECK(r->state == GameState::ThrowInForwardLeft);
    CHECK_FALSE(r->is_goal);

    r = ClassifyBallOutOfPlay(600, 400, 0, 1); // right, centre
    REQUIRE(r.has_value());
    CHECK(r->state == GameState::ThrowInCentreRight);

    r = ClassifyBallOutOfPlay(50, 600, 0, 1); // left, back
    REQUIRE(r.has_value());
    CHECK(r->state == GameState::ThrowInBackLeft);
}

TEST_CASE("goal mouth past byline is a goal") {
    auto r = ClassifyBallOutOfPlay(336, 100, 0, 1); // z=0, in mouth, past top
    REQUIRE(r.has_value());
    CHECK(r->is_goal);
    CHECK(r->state == GameState::GoalOutLeft);
}

TEST_CASE("byline outside mouth is corner or goal kick") {
    auto corner = ClassifyBallOutOfPlay(100, 100, 0, 2); // defending top last touched
    REQUIRE(corner.has_value());
    CHECK_FALSE(corner->is_goal);
    CHECK(corner->state == GameState::CornerLeft);

    auto gk = ClassifyBallOutOfPlay(100, 100, 0, 1); // attacking team last touched
    REQUIRE(gk.has_value());
    CHECK_FALSE(gk->is_goal);
    CHECK(gk->state == GameState::GoalOutLeft);
}
