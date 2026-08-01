// B5: planar and height proximity bands.
#include <doctest/doctest.h>

#include "core/possession.hpp"

using namespace at;

TEST_CASE("planar bands use squared thresholds") {
    TeamControl tc{};
    ClassifyPlanarBands(tc, 0);
    CHECK(tc.pl_very_close_to_ball == 1);
    CHECK(tc.pl_close_to_ball == 1);
    CHECK(tc.pl_not_far_from_ball == 1);

    ClassifyPlanarBands(tc, 32);
    CHECK(tc.pl_very_close_to_ball == 1);

    ClassifyPlanarBands(tc, 33);
    CHECK(tc.pl_very_close_to_ball == 0);
    CHECK(tc.pl_close_to_ball == 1);

    ClassifyPlanarBands(tc, 72);
    CHECK(tc.pl_close_to_ball == 1);
    ClassifyPlanarBands(tc, 73);
    CHECK(tc.pl_close_to_ball == 0);
    CHECK(tc.pl_not_far_from_ball == 1);

    ClassifyPlanarBands(tc, 2450);
    CHECK(tc.pl_not_far_from_ball == 1);
    ClassifyPlanarBands(tc, 2451);
    CHECK(tc.pl_not_far_from_ball == 0);
}

TEST_CASE("height bands partition z") {
    TeamControl tc{};
    ClassifyHeightBands(tc, 0);
    CHECK(tc.ball_less_equal_4 == 1);
    CHECK(tc.ball_4_to_8 == 0);

    ClassifyHeightBands(tc, 4);
    CHECK(tc.ball_less_equal_4 == 1);

    ClassifyHeightBands(tc, 5);
    CHECK(tc.ball_4_to_8 == 1);

    ClassifyHeightBands(tc, 8);
    CHECK(tc.ball_4_to_8 == 1);
    ClassifyHeightBands(tc, 9);
    CHECK(tc.ball_8_to_12 == 1);

    ClassifyHeightBands(tc, 12);
    CHECK(tc.ball_8_to_12 == 1);
    ClassifyHeightBands(tc, 13);
    CHECK(tc.ball_12_to_17 == 1);

    ClassifyHeightBands(tc, 17);
    CHECK(tc.ball_12_to_17 == 1);
    ClassifyHeightBands(tc, 18);
    CHECK(tc.ball_above_17 == 1);
}

TEST_CASE("UpdateProximityBands fills controlled player flags") {
    MatchState s{};
    SetPl(s, GameStatePl::InProgress);
    s.sides[0].control.controlled_slot = 0;
    s.players[0].team_number = 1;
    s.players[0].pos.x = Fix::FromInt(100);
    s.players[0].pos.y = Fix::FromInt(100);
    s.ball.pos.x = Fix::FromInt(102); // dx=2,dy=0 → dist_sq=4
    s.ball.pos.y = Fix::FromInt(100);
    s.ball.pos.z = Fix::FromInt(3);

    UpdateProximityBands(s, 0);
    CHECK(s.players[0].ball_distance == 4);
    CHECK(s.sides[0].control.pl_very_close_to_ball == 1);
    CHECK(s.sides[0].control.ball_less_equal_4 == 1);
}
