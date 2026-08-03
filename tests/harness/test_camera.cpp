// C2 — camera: lead offset, ease, two-stage clipping. Pure: no SDL.
#include <doctest/doctest.h>

#include "render/camera.hpp"

#include "core/hash.hpp"
#include "core/match_clock.hpp"
#include "core/match_engine.hpp"
#include "mode/sandbox.hpp"
#include "render/asset_source.hpp"

using namespace at;
using namespace at::render;

namespace {

MatchState InPlayAt(int16_t bx, int16_t by, int16_t dx = 0, int16_t dy = 0) {
    MatchState s{};
    // Open play in our engine is StartingGame + InProgress: the clock flips Pl and
    // phase at kickoff and leaves game_state alone (match_clock.hpp).
    SetGameState(s, GameState::StartingGame);
    SetPl(s, GameStatePl::InProgress);
    s.ball.pos.x = Fix::FromInt(bx);
    s.ball.pos.y = Fix::FromInt(by);
    s.ball.delta.x = Fix::FromInt(dx);
    s.ball.delta.y = Fix::FromInt(dy);
    return s;
}

} // namespace

TEST_CASE("the lead ramps by 2 to 40 and reverses when the ball turns") {
    int16_t lead = 0;
    for (int i = 0; i < 19; ++i) lead = RampLead(lead, +1);
    CHECK(lead == 38);
    lead = RampLead(lead, +1);
    CHECK(lead == kLeadMax);
    lead = RampLead(lead, +1);
    CHECK(lead == kLeadMax);   // holds at the cap, not past it

    lead = RampLead(lead, -1);
    CHECK(lead == 38);
    for (int i = 0; i < 60; ++i) lead = RampLead(lead, -1);
    CHECK(lead == -kLeadMax);

    CHECK(RampLead(12, 0) == 12);   // no direction, no change
}

TEST_CASE("easing takes a sixteenth, then caps at five") {
    CHECK(EaseTowards(0, 160) == 5);      // 160/16 = 10, capped
    CHECK(EaseTowards(0, 64) == 4);       // 64/16 = 4, under the cap
    CHECK(EaseTowards(0, -160) == -5);
    CHECK(EaseTowards(100, 100) == 100);  // already there
    // Never stalls one unit short: a remainder under 16 still moves.
    CHECK(EaseTowards(0, 3) == 1);
    CHECK(EaseTowards(3, 0) == 2);
}

TEST_CASE("the destination clip is symmetric about the pitch and narrows with the margin") {
    CHECK(ClipCameraDestination(-50, kSideLimitInPlay, kCameraMaxX - kSideLimitInPlay) ==
          kSideLimitInPlay);
    CHECK(ClipCameraDestination(9999, kSideLimitInPlay, kCameraMaxX - kSideLimitInPlay) ==
          kCameraMaxX - kSideLimitInPlay);
    // A corner's narrower margin lets the camera nearer the touchline.
    CHECK(ClipCameraDestination(0, kSideLimitBreak, kCameraMaxX - kSideLimitBreak) <
          ClipCameraDestination(0, kSideLimitInPlay, kCameraMaxX - kSideLimitInPlay));
}

TEST_CASE("the camera stays inside the hard limits and its window inside the world") {
    Camera cam;
    cam.Reset(0);
    MatchState s = InPlayAt(kPlayableMaxX, kPlayableMaxY, 1, 1);
    for (int i = 0; i < 400; ++i) cam.Update(s);
    CHECK(cam.X() >= kCameraMinX);
    CHECK(cam.X() <= kCameraMaxX);
    CHECK(cam.Y() >= kCameraMinY);
    CHECK(cam.Y() <= kCameraMaxY);

    const DebugView v = cam.View();
    CHECK(v.max_x - v.min_x == kLogicalW);
    CHECK(v.max_y - v.min_y == kLogicalH);
    CHECK(v.max_x <= kPitchWorldW);
    CHECK(v.max_y <= kPitchWorldH);
}

TEST_CASE("the kick-off camera starts at one end or the other") {
    Camera top, bottom;
    top.Reset(0);
    bottom.Reset(1);
    CHECK(top.Y() != bottom.Y());
    CHECK(top.X() == bottom.X());
}

TEST_CASE("following a ball travelling right leaves it behind centre") {
    Camera cam;
    cam.Reset(0);
    MatchState s = InPlayAt(kCentreSpotX, kCentreSpotY, 1, 0);
    for (int i = 0; i < 300; ++i) cam.Update(s);

    CHECK(cam.LeadX() == kLeadMax);
    // Ball's screen x = world x − camera x. With a full positive lead the ball sits
    // left of centre, which is what "the camera anticipates" means.
    const int ball_screen_x = kCentreSpotX - cam.X();
    CHECK(ball_screen_x < kLogicalW / 2);
    CHECK(ball_screen_x == kLogicalW / 2 - kLeadMax);
}

TEST_CASE("a frozen camera does not move and does not accumulate lead") {
    Camera cam;
    cam.Reset(0);
    MatchState s = InPlayAt(kCentreSpotX, kCentreSpotY, 1, 1);
    for (int i = 0; i < 40; ++i) cam.Update(s);
    const int16_t x = cam.X(), y = cam.Y(), lead = cam.LeadX();

    s.globals.show_fans_counter = 20;
    for (int i = 0; i < 40; ++i) cam.Update(s);
    CHECK(cam.X() == x);
    CHECK(cam.Y() == y);
    CHECK(cam.LeadX() == lead);
    CHECK(cam.Mode() == CameraMode::Frozen);
}

TEST_CASE("the penalty shootout camera is a fixed point, not a follow") {
    Camera cam;
    cam.Reset(0);
    MatchState s = InPlayAt(100, 700, 1, 1);
    SetGameState(s, GameState::Penalties);
    for (int i = 0; i < 400; ++i) cam.Update(s);
    const int16_t x = cam.X(), y = cam.Y();

    s.ball.pos.x = Fix::FromInt(600);   // the ball moves; the camera does not
    s.ball.pos.y = Fix::FromInt(120);
    for (int i = 0; i < 100; ++i) cam.Update(s);
    CHECK(cam.X() == x);
    CHECK(cam.Y() == y);
}

TEST_CASE("a corner lets the camera nearer the touchline than open play does") {
    MatchState corner = InPlayAt(kPlayableMinX, kPlayableMinY);
    SetGameState(corner, GameState::CornerLeft);
    SetPl(corner, GameStatePl::Stopped);
    Camera a;
    a.Reset(0);
    for (int i = 0; i < 400; ++i) a.Update(corner);

    MatchState open = InPlayAt(kPlayableMinX, kPlayableMinY);
    SetPl(open, GameStatePl::InProgress);
    Camera b;
    b.Reset(0);
    for (int i = 0; i < 400; ++i) b.Update(open);

    CHECK(a.X() < b.X());
}

TEST_CASE("open play after a centre kickoff follows the ball, not the touchline") {
    // Regression: our clock leaves game_state at StartingGame while play runs (it flips
    // Pl to InProgress and phase to InPlay instead), and set_pieces returns to it after
    // every goal. Treating that state as the reference's "players walking out" view
    // pinned the camera to the right touchline for most of a match — and for all of a
    // sandbox, which starts in exactly this state.
    MatchState s = InPlayAt(200, 600);
    SetGameState(s, GameState::StartingGame);
    SetPl(s, GameStatePl::InProgress);

    Camera cam;
    cam.Reset(0);
    for (int i = 0; i < 400; ++i) cam.Update(s);

    // The ball is on screen, near the middle of the window.
    const int bx = 200 - cam.X();
    const int by = 600 - cam.Y();
    CHECK(bx > 0);
    CHECK(bx < kLogicalW);
    CHECK(by > 0);
    CHECK(by < kLogicalH);
}

TEST_CASE("the sandbox camera converges on the ball") {
    MatchEngine engine;
    at::mode::SandboxConfig cfg = at::mode::DefaultSandboxConfig();
    at::mode::StartSandbox(engine, cfg);

    Camera cam;
    cam.Reset(engine.DrawPresentationRng());
    MatchInput in{};
    for (int i = 0; i < 300; ++i) {
        engine.Step(in);
        cam.Update(engine.State());
    }

    const int bx = engine.State().ball.pos.x.Whole() - cam.X();
    const int by = engine.State().ball.pos.y.Whole() - cam.Y();
    CHECK(bx > 0);
    CHECK(bx < kLogicalW);
    CHECK(by > 0);
    CHECK(by < kLogicalH);
}

TEST_CASE("the camera never touches the hashed state") {
    MatchEngine engine;
    engine.Reset(1234);
    MatchInput in{};
    for (int i = 0; i < 50; ++i) engine.Step(in);
    const uint64_t before = HashState(engine.State());

    Camera cam;
    cam.Reset(engine.DrawPresentationRng());   // presentation stream only
    for (int i = 0; i < 200; ++i) cam.Update(engine.State());
    CHECK(HashState(engine.State()) == before);
}

// The freeze case above holds show_fans_counter at a constant and only ever
// asserts that the camera stops. Nothing asserted it starts again — so when the
// engine began setting the counter at every goal without ticking it down, the
// camera latched onto the goal just scored for the rest of the match and no test
// noticed. A freeze test without a thaw test is half a test.
TEST_CASE("a frozen camera follows the ball again once the celebration expires") {
    Camera cam;
    cam.Reset(0);
    MatchState s = InPlayAt(kCentreSpotX, kCentreSpotY, 1, 1);
    for (int i = 0; i < 40; ++i) cam.Update(s);

    // Goal at the top end: freeze, then let the counter run out the way the
    // match clock runs it out.
    s.globals.show_fans_counter = 20;
    for (int i = 0; i < 10; ++i) cam.Update(s);
    REQUIRE(cam.Mode() == CameraMode::Frozen);
    const int16_t frozen_y = cam.Y();

    while (s.globals.show_fans_counter > 0) {
        UpdateTime(s);
        cam.Update(s);
    }
    CHECK(s.globals.show_fans_counter == 0);

    // Ball is now down the other end; the camera must go with it.
    s.ball.pos.x = Fix::FromInt(kCentreSpotX);
    s.ball.pos.y = Fix::FromInt(700);
    for (int i = 0; i < 200; ++i) cam.Update(s);

    CHECK(cam.Mode() != CameraMode::Frozen);
    CHECK(cam.Y() > frozen_y); // it actually travelled toward the ball
}
