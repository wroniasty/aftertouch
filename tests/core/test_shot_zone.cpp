// B6a / S5: the shot-on-goal bonus is gated on position, not just the octant.
//
// SHOOTING §3: a set of comparisons on the ball's pitch position combined with
// a goalward kick direction; off-target kicks skip the bonus entirely, and the
// Finishing table applies inside the penalty area. Testing it through the
// launch speed keeps the classifier's own shape free to change.
#include <doctest/doctest.h>

#include "core/shooting.hpp"
#include "kick_fixture.hpp"

using namespace at;
using namespace at::test;

namespace {

// Launch speed of a held shot in `dir` taken from (x, y). Side 0 attacks the
// top goal, so N is goalward.
int16_t ShotSpeedFrom(int x, int y, Dir dir = Dir::N) {
    MatchEngine eng = MakeKickEngine(x, y);
    KickScript sc;
    sc.kick_dir      = dir;
    sc.after_dir     = Dir::None; // release: no vertical sample, no trim
    sc.react_delay   = 1;
    sc.dribble_ticks = 0;
    sc.total_ticks   = kFireHoldThreshold + 8;
    const KickRun r = RunKick(eng, sc);
    REQUIRE(r.struck);
    REQUIRE_FALSE(r.was_pass);
    return r.launch_speed;
}

} // namespace

TEST_CASE("the Finishing table pays at least as well as Velocity, point for point") {
    for (size_t i = 0; i < kBallSpeedFinishing.size(); ++i) {
        CAPTURE(i);
        CHECK(kBallSpeedFinishing[i] >= kBallSpeedKicking[i]);
    }
}

TEST_CASE("a goalward kick from the defending half is not a shot on goal") {
    const int16_t own_half   = ShotSpeedFrom(336, 600); // deep in our own half
    const int16_t away_goal  = ShotSpeedFrom(336, 300, Dir::S); // wrong way
    CHECK(own_half == away_goal);
}

TEST_CASE("a kick wide of the goal corridor is not a shot on goal") {
    const int16_t wide    = ShotSpeedFrom(140, 300);
    const int16_t no_shot = ShotSpeedFrom(336, 300, Dir::S);
    CHECK(wide == no_shot);
}

TEST_CASE("on target outside the area pays Velocity, inside it pays Finishing") {
    const int16_t no_shot   = ShotSpeedFrom(336, 300, Dir::S);
    const int16_t long_shot = ShotSpeedFrom(336, 300);
    const int16_t in_area   = ShotSpeedFrom(336, 190);

    CHECK(long_shot > no_shot);
    CHECK(in_area > long_shot);
}

TEST_CASE("the area is the pitch's penalty area, not a third of the pitch") {
    // A shot from the halfway line must not be paid as a six-yard tap-in.
    const int16_t midfield = ShotSpeedFrom(336, 430);
    const int16_t in_area  = ShotSpeedFrom(336, 190);
    const int16_t edge_out = ShotSpeedFrom(336, 260); // just outside the area
    CHECK(midfield < in_area);
    CHECK(edge_out < in_area);
    CHECK(midfield == edge_out); // both are long shots
}
