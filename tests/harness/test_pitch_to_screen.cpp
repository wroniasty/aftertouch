// C1a: pure pitch→screen mapper (no SDL).
#include <doctest/doctest.h>

#include "render/pitch_view.hpp"

#include "core/match_state.hpp"

using namespace at;
using namespace at::render;

TEST_CASE("centre spot maps inside the logical frame") {
    const auto s = PitchToScreen(kCentreSpotX, kCentreSpotY);
    CHECK(s.x >= 0);
    CHECK(s.y >= 0);
    CHECK(s.x < kLogicalW);
    CHECK(s.y < kLogicalH);
}

TEST_CASE("playable / view corners land on-screen") {
    const auto a = PitchToScreen(kViewMinX, kViewMinY);
    const auto b = PitchToScreen(kViewMaxX, kViewMaxY);
    CHECK(a.x >= 0);
    CHECK(a.y >= 0);
    CHECK(b.x < kLogicalW);
    CHECK(b.y < kLogicalH);
    CHECK(a.x < b.x);
    CHECK(a.y < b.y);
}

TEST_CASE("mapping is monotonic in x and y") {
    const auto left  = PitchToScreen(100, 400);
    const auto right = PitchToScreen(500, 400);
    const auto top   = PitchToScreen(300, 150);
    const auto bot   = PitchToScreen(300, 700);
    CHECK(left.x < right.x);
    CHECK(top.y < bot.y);
}
