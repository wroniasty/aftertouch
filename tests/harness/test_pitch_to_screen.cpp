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

TEST_CASE("follow window centres the anchor near screen mid") {
    const DebugView v = WindowAround(kCentreSpotX, kCentreSpotY);
    CHECK(v.min_x >= kViewMinX);
    CHECK(v.max_x <= kViewMaxX);
    CHECK(v.min_y >= kViewMinY);
    CHECK(v.max_y <= kViewMaxY);
    CHECK(v.max_x - v.min_x == kFollowWinW);
    CHECK(v.max_y - v.min_y == kFollowWinH);

    const auto s = PitchToScreen(kCentreSpotX, kCentreSpotY, v);
    // Letterboxed; expect near the middle of the used area.
    CHECK(s.x > kLogicalW / 4);
    CHECK(s.x < (3 * kLogicalW) / 4);
    CHECK(s.y > kLogicalH / 4);
    CHECK(s.y < (3 * kLogicalH) / 4);
}

TEST_CASE("follow window renders 1 pitch unit per pixel") {
    // Anything but 1.0 blits 16-px tiles at a fractional size and smears the
    // pattern weave; sprites go lumpy with it. See C1a §2.1.
    const DebugView v = WindowAround(kCentreSpotX, kCentreSpotY);
    CHECK(PitchUniformScale(v) == doctest::Approx(1.f));

    const auto a = PitchToScreen(kCentreSpotX, kCentreSpotY, v);
    const auto b = PitchToScreen(static_cast<int16_t>(kCentreSpotX + 64),
                                 kCentreSpotY, v);
    CHECK(b.x - a.x == 64);
}

TEST_CASE("follow window clamps at dead-ball corners") {
    const DebugView tl = WindowAround(kViewMinX, kViewMinY);
    CHECK(tl.min_x == kViewMinX);
    CHECK(tl.min_y == kViewMinY);
    CHECK(tl.max_x == static_cast<int16_t>(kViewMinX + kFollowWinW));
    CHECK(tl.max_y == static_cast<int16_t>(kViewMinY + kFollowWinH));

    const DebugView br = WindowAround(kViewMaxX, kViewMaxY);
    CHECK(br.max_x == kViewMaxX);
    CHECK(br.max_y == kViewMaxY);
    CHECK(br.min_x == static_cast<int16_t>(kViewMaxX - kFollowWinW));
    CHECK(br.min_y == static_cast<int16_t>(kViewMaxY - kFollowWinH));
}

TEST_CASE("full pitch view matches legacy frustum") {
    const DebugView full = FullPitchView();
    CHECK(full.min_x == kViewMinX);
    CHECK(full.max_x == kViewMaxX);
    const auto a = PitchToScreen(kCentreSpotX, kCentreSpotY, full);
    const auto b = PitchToScreen(kCentreSpotX, kCentreSpotY);
    CHECK(a.x == b.x);
    CHECK(a.y == b.y);
}
