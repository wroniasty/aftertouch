// B6: aftertouch latch, curl, tick-4 vertical, window close.
#include <doctest/doctest.h>

#include "core/aftertouch.hpp"
#include "core/match_clock.hpp"
#include "core/shooting.hpp"

using namespace at;

TEST_CASE("aftertouch latches left curl and nudges dest") {
    MatchState s{};
    SetPl(s, GameStatePl::InProgress);
    s.sides[0].control.spin_timer = 0;
    s.sides[0].control.controlled_pl_direction = static_cast<int16_t>(Dir::N);
    s.sides[0].control.current_allowed_direction = static_cast<int16_t>(Dir::NW); // diff 7 → right? 
    // (joy - kick) & 7: NW=7, N=0 → (7-0)&7 = 7 → right (≥5)
    s.ball.dest_x = 336;
    s.ball.dest_y = 100;

    ApplyAftertouchForTeam(s, 0);
    CHECK(s.sides[0].control.right_spin == 1);
    CHECK(s.sides[0].control.spin_timer == 1);
    // N right factor {-12,0} * m[0]=8 → dest_x -= 96
    CHECK(s.ball.dest_x == static_cast<int16_t>(336 - 96));
}

TEST_CASE("aftertouch latches left on NE from N") {
    MatchState s{};
    SetPl(s, GameStatePl::InProgress);
    s.sides[0].control.spin_timer = 0;
    s.sides[0].control.controlled_pl_direction = static_cast<int16_t>(Dir::N);
    s.sides[0].control.current_allowed_direction = static_cast<int16_t>(Dir::NE); // diff 1 → left
    s.ball.dest_x = 336;
    s.ball.dest_y = 100;

    ApplyAftertouchForTeam(s, 0);
    CHECK(s.sides[0].control.left_spin == 1);
    CHECK(s.ball.dest_x == static_cast<int16_t>(336 + 96)); // {12,0}*8
}

TEST_CASE("tick 4 applies low drive on perpendicular stick") {
    MatchState s{};
    SetPl(s, GameStatePl::InProgress);
    s.sides[0].control.spin_timer = 4;
    s.sides[0].control.controlled_pl_direction = static_cast<int16_t>(Dir::N);
    s.sides[0].control.current_allowed_direction = static_cast<int16_t>(Dir::E); // diff 2
    s.sides[0].control.left_spin = 1;
    s.ball.speed = 1000;
    s.ball.delta.z = Fix::FromRaw(kBallKickingDeltaZRaw);

    ApplyAftertouchForTeam(s, 0);
    CHECK(s.ball.delta.z.Raw() == kNormalKickDeltaZRaw);
    // E is dir 2 — facing trim unchanged for 2/6
    CHECK(s.ball.speed == kNormalKickBallSpeed);
}

TEST_CASE("window closes at 10") {
    MatchState s{};
    SetPl(s, GameStatePl::InProgress);
    s.sides[0].control.spin_timer = 9;
    s.sides[0].control.controlled_pl_direction = 0;
    s.sides[0].control.current_allowed_direction = -1;
    ApplyAftertouchForTeam(s, 0);
    CHECK(s.sides[0].control.spin_timer == -1);
}
