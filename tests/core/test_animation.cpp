// C3 — the frame stepper and its opcode interpreter.
#include <doctest/doctest.h>

#include "core/animation.hpp"

using namespace at;

namespace {

Entity Runner(int direction, int16_t delay = 1) {
    Entity e{};
    e.team_number = 1;
    e.player_ordinal = 5;
    e.direction = static_cast<int16_t>(direction);
    e.speed = 1000;
    e.is_moving = 1;
    e.frame_delay = delay;
    e.frame_index = -1;
    e.image_index = -1;
    e.player_state = static_cast<uint8_t>(PlayerState::Normal);
    return e;
}

} // namespace

TEST_CASE("running cycles through its direction's frame list and loops") {
    Entity e = Runner(2); // E → 6, 7, 8
    CHECK(StepEntityAnimation(e) == 6);
    CHECK(StepEntityAnimation(e) == 7);
    CHECK(StepEntityAnimation(e) == 8);
    CHECK(StepEntityAnimation(e) == 6);   // kLoop restarts the sequence
    CHECK(e.frame_switch_counter == 4);
}

TEST_CASE("frame_delay gates the advance") {
    Entity e = Runner(2, 3);
    CHECK(StepEntityAnimation(e) == 6);
    // Two idle ticks: the image holds, the counter does not move.
    CHECK(StepEntityAnimation(e) == 6);
    CHECK(StepEntityAnimation(e) == 6);
    CHECK(e.frame_switch_counter == 1);
    CHECK(StepEntityAnimation(e) == 7);
    CHECK(e.frame_switch_counter == 2);
}

TEST_CASE("a stationary Normal player stands rather than running on the spot") {
    Entity e = Runner(4);
    e.speed = 0;
    e.is_moving = 0;
    const int16_t first = StepEntityAnimation(e);
    CHECK(first == 4);                       // the legs-together S frame
    CHECK(StepEntityAnimation(e) == first);  // kHold freezes it
    CHECK(StepEntityAnimation(e) == first);
}

TEST_CASE("every direction resolves to a distinct standing frame") {
    int16_t seen[anim::kDirections] = {};
    for (int d = 0; d < anim::kDirections; ++d) {
        Entity e = Runner(d);
        e.speed = 0;
        e.is_moving = 0;
        seen[d] = StepEntityAnimation(e);
        CHECK(seen[d] >= 0);
        CHECK(seen[d] < 101);
    }
    for (int a = 0; a < anim::kDirections; ++a)
        for (int b = a + 1; b < anim::kDirections; ++b) CHECK(seen[a] != seen[b]);
}

TEST_CASE("a state change restarts the sequence instead of inheriting the cursor") {
    Entity e = Runner(2);
    StepEntityAnimation(e);
    StepEntityAnimation(e);
    CHECK(e.frame_index == 1);

    // N's list is six frames long; E's is three plus the loop marker. Switching to the
    // longer list must not leave a cursor pointing past a shorter one later.
    e.direction = 0;
    StepEntityAnimation(e);
    CHECK(e.frame_index >= 0);
    CHECK(e.image_index >= 0);
    e.direction = 2;
    e.frame_index = 99;   // stale cursor from a longer list
    const int16_t v = StepEntityAnimation(e);
    CHECK(v >= 0);
}

TEST_CASE("tackling holds one prone frame per direction") {
    for (int d = 0; d < anim::kDirections; ++d) {
        Entity e = Runner(d);
        e.player_state = static_cast<uint8_t>(PlayerState::Tackling);
        const int16_t f = StepEntityAnimation(e);
        CHECK(f >= 54);
        CHECK(f <= 63);
        CHECK(StepEntityAnimation(e) == f);
    }
}

TEST_CASE("the keeper's N and S cycles stay inside his own 58-frame bank") {
    for (int d = 0; d < anim::kDirections; ++d) {
        Entity e = Runner(d);
        e.player_ordinal = 1;   // keeper
        for (int i = 0; i < 12; ++i) {
            const int16_t f = StepEntityAnimation(e);
            CHECK(f >= 0);
            CHECK(f < 58);
        }
    }
}

TEST_CASE("the ball spins with speed and holds when still") {
    Entity ball{};
    ball.speed = 30;
    int changes = 0;
    int16_t last = ball.image_index;
    for (int i = 0; i < 8; ++i) {
        StepBallAnimation(ball);
        if (ball.image_index != last) ++changes;
        last = ball.image_index;
        CHECK(ball.image_index >= 0);
        CHECK(ball.image_index < 4);
    }
    CHECK(changes >= 4);   // fast ball, one frame per tick

    Entity still{};
    still.speed = 0;
    still.image_index = 2;
    StepBallAnimation(still);
    CHECK(still.image_index == 2);
}

TEST_CASE("StepAnimations leaves off-pitch players alone") {
    MatchState s{};
    for (int i = 0; i < kPitchPlayers; ++i) {
        s.players[static_cast<size_t>(i)] = Runner(2);
        ParkOffPitch(s.players[static_cast<size_t>(i)]);
        s.players[static_cast<size_t>(i)].image_index = -1;
    }
    StepAnimations(s);
    for (int i = 0; i < kPitchPlayers; ++i)
        CHECK(s.players[static_cast<size_t>(i)].image_index == -1);
}
