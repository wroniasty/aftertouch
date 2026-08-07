// C3 — sprite_viewer's model layer. Pure: no SDL, no window, no ImGui.
//
// The viewer's claim is that it shows what the game shows. That claim rests
// entirely on this file's subject: which PlayerState and direction the input
// produces, and that the animation itself is stepped by the engine's own
// StepEntityAnimation rather than by anything the tool invented. Everything
// below pins the mapping; nothing below re-tests the stepper, which core_tests
// already covers.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "preview_state.hpp"

using namespace at;
using namespace at::spriteview;

namespace {

Preview Fresh() {
    Preview p;
    Reset(p);
    return p;
}

Controls Hold(int dir) {
    Controls c;
    c.direction = dir;
    return c;
}

// Run n ticks with nothing held.
void Idle(Preview& p, const Config& cfg, int n, Mode mode = Mode::Table) {
    for (int i = 0; i < n; ++i) {
        Controls c;
        Apply(p, c, cfg);
        Tick(p, mode);
    }
}

} // namespace

TEST_CASE("axes map to the octant convention, 0 = N clockwise") {
    CHECK(DirectionFromAxes(true, false, false, false) == 0);   // N
    CHECK(DirectionFromAxes(true, false, false, true) == 1);    // NE
    CHECK(DirectionFromAxes(false, false, false, true) == 2);   // E
    CHECK(DirectionFromAxes(false, true, false, true) == 3);    // SE
    CHECK(DirectionFromAxes(false, true, false, false) == 4);   // S
    CHECK(DirectionFromAxes(false, true, true, false) == 5);    // SW
    CHECK(DirectionFromAxes(false, false, true, false) == 6);   // W
    CHECK(DirectionFromAxes(true, false, true, false) == 7);    // NW
}

TEST_CASE("opposite axes cancel rather than picking a winner") {
    CHECK(DirectionFromAxes(true, true, false, false) == kNoDirection);
    CHECK(DirectionFromAxes(false, false, true, true) == kNoDirection);
    CHECK(DirectionFromAxes(true, true, true, true) == kNoDirection);
    // A cancelled axis must not eat the surviving one.
    CHECK(DirectionFromAxes(true, true, false, true) == 2);   // E
    CHECK(DirectionFromAxes(false, true, true, true) == 4);   // S
}

TEST_CASE("nothing held is nothing held") {
    CHECK(DirectionFromAxes(false, false, false, false) == kNoDirection);
}

TEST_CASE("holding a direction runs; releasing it stands") {
    Config  cfg;
    Preview p = Fresh();

    Apply(p, Hold(2), cfg);   // E
    Tick(p, Mode::Table);
    CHECK(p.e.direction == 2);
    CHECK(p.e.is_moving == 1);
    CHECK(p.e.speed == cfg.run_speed);
    CHECK(&SelectAnimationTable(p.e) == &anim::kRunning);

    Controls none;
    Apply(p, none, cfg);
    Tick(p, Mode::Table);
    CHECK(p.e.is_moving == 0);
    CHECK(p.e.speed == 0);
    CHECK(&SelectAnimationTable(p.e) == &anim::kStanding);
    // Direction survives release — a standing player still faces somewhere.
    CHECK(p.e.direction == 2);
}

TEST_CASE("the keeper flag drives the keeper tables, not just the bank") {
    Config  cfg;
    Preview p;
    p.keeper = true;
    Reset(p);
    CHECK(p.e.player_ordinal == 1);

    Apply(p, Hold(0), cfg);   // N
    Tick(p, Mode::Table);
    CHECK(&SelectAnimationTable(p.e) == &anim::kKeeperRunning);
}

TEST_CASE("fire alternates header and slide tackle") {
    Config  cfg;
    Preview p = Fresh();
    REQUIRE(p.next_oneshot == OneShot::Header);

    Controls fire;
    fire.fire_pressed = true;

    Apply(p, fire, cfg);
    CHECK(p.e.player_state == static_cast<uint8_t>(PlayerState::StaticHeader));
    CHECK(p.next_oneshot == OneShot::Slide);

    Apply(p, fire, cfg);
    CHECK(p.e.player_state == static_cast<uint8_t>(PlayerState::Tackling));
    CHECK(p.next_oneshot == OneShot::Header);

    Apply(p, fire, cfg);
    CHECK(p.e.player_state == static_cast<uint8_t>(PlayerState::StaticHeader));
}

TEST_CASE("the mood button alternates happy and sad") {
    Config  cfg;
    Preview p = Fresh();

    Controls mood;
    mood.mood_pressed = true;

    Apply(p, mood, cfg);
    CHECK(p.e.player_state == static_cast<uint8_t>(PlayerState::Happy));
    Apply(p, mood, cfg);
    CHECK(p.e.player_state == static_cast<uint8_t>(PlayerState::Sad));
    Apply(p, mood, cfg);
    CHECK(p.e.player_state == static_cast<uint8_t>(PlayerState::Happy));
}

TEST_CASE("a one-shot restarts its sequence rather than resuming a stale cursor") {
    Config  cfg;
    Preview p = Fresh();

    Apply(p, Hold(2), cfg);
    for (int i = 0; i < 20; ++i) Tick(p, Mode::Table);
    REQUIRE(p.e.frame_index >= 0);

    Controls fire;
    fire.fire_pressed = true;
    Apply(p, fire, cfg);
    CHECK(p.e.frame_index == -1);
    CHECK(p.e.cycle_frames_timer == 0);
}

TEST_CASE("a one-shot returns to idle instead of freezing on kHold forever") {
    Config cfg;
    cfg.oneshot_ticks = 6;
    Preview p = Fresh();

    Controls fire;
    fire.fire_pressed = true;
    Apply(p, fire, cfg);
    REQUIRE(p.oneshot_timer == 6);

    Idle(p, cfg, 5);
    CHECK(p.oneshot_timer == 1);
    CHECK(p.e.player_state == static_cast<uint8_t>(PlayerState::StaticHeader));

    Idle(p, cfg, 1);
    CHECK(p.oneshot_timer == 0);
    CHECK(p.e.player_state == static_cast<uint8_t>(PlayerState::Normal));
}

TEST_CASE("direction is applied during a one-shot so a header can be aimed") {
    Config cfg;
    cfg.oneshot_ticks = 30;
    Preview p = Fresh();

    Controls fire;
    fire.fire_pressed = true;
    fire.direction    = 6;   // W
    Apply(p, fire, cfg);
    CHECK(p.e.direction == 6);
    CHECK(p.e.player_state == static_cast<uint8_t>(PlayerState::StaticHeader));

    // ...and a later direction change does not cancel it.
    Apply(p, Hold(2), cfg);
    Tick(p, Mode::Table);
    CHECK(p.e.direction == 2);
    CHECK(p.e.player_state == static_cast<uint8_t>(PlayerState::StaticHeader));
}

TEST_CASE("raw mode shows the requested frame and ignores the tables") {
    Config  cfg;
    Preview p = Fresh();
    p.raw_frame = 77;

    Apply(p, Hold(2), cfg);   // running would otherwise walk 6,7,8
    CHECK(Tick(p, Mode::Raw) == 77);
    CHECK(Tick(p, Mode::Raw) == 77);
    CHECK(p.e.image_index == 77);
}

TEST_CASE("raw frame clamps to the bank it is scrubbing") {
    Preview p = Fresh();

    p.raw_frame = -5;
    CHECK(Tick(p, Mode::Raw) == 0);

    p.raw_frame = 9999;
    CHECK(Tick(p, Mode::Raw) == kPlayerFrameCount - 1);

    p.keeper = true;
    Reset(p);
    p.raw_frame = 9999;
    CHECK(Tick(p, Mode::Raw) == kKeeperFrameCount - 1);
}

TEST_CASE("StateHasTable is honest about the states C3 has not measured yet") {
    Config  cfg;
    Preview p = Fresh();
    CHECK(StateHasTable(p));                // standing

    Controls fire;
    fire.fire_pressed = true;
    Apply(p, fire, cfg);                    // header
    CHECK_FALSE(StateHasTable(p));
    Apply(p, fire, cfg);                    // slide
    CHECK(StateHasTable(p));

    Controls mood;
    mood.mood_pressed = true;
    Apply(p, mood, cfg);                    // happy
    CHECK_FALSE(StateHasTable(p));
}

TEST_CASE("shirt type selects the geometry bank, and only that") {
    Preview p = Fresh();

    p.shirt_type = 0;
    CHECK(Geometry(p) == ShirtGeometry::VerticalStripes);
    p.shirt_type = 1;
    CHECK(Geometry(p) == ShirtGeometry::ColouredSleeves);
    p.shirt_type = 2;
    CHECK(Geometry(p) == ShirtGeometry::VerticalStripes);
    p.shirt_type = 3;
    CHECK(Geometry(p) == ShirtGeometry::HorizontalStripes);
}

TEST_CASE("Reset keeps the bank selection and clears everything else") {
    Config  cfg;
    Preview p = Fresh();
    p.keeper     = true;
    p.shirt_type = 3;

    Controls fire;
    fire.fire_pressed = true;
    fire.direction    = 5;
    Apply(p, fire, cfg);
    for (int i = 0; i < 4; ++i) Tick(p, Mode::Table);

    Reset(p);
    CHECK(p.keeper);
    CHECK(p.shirt_type == 3);
    CHECK(p.e.player_ordinal == 1);
    CHECK(p.oneshot_timer == 0);
    CHECK(p.e.player_state == static_cast<uint8_t>(PlayerState::Normal));
    CHECK(p.e.frame_index == -1);
    CHECK(p.e.image_index == -1);
}
