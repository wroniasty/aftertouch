// B6: Finishing inside the area vs Velocity outside it.
// The zone predicate itself is exercised end-to-end in test_shot_zone.cpp.
#include <doctest/doctest.h>

#include "core/shooting.hpp"

using namespace at;

namespace {

// Side 0 attacking the top goal (team_playing_up = 2).
MatchState ShooterAt(int16_t x, int16_t y, int ordinal) {
    MatchState s{};
    SetPl(s, GameStatePl::InProgress);
    s.globals.team_playing_up = 2;
    s.sides[0].control.controlled_slot = 0;
    s.sides[0].control.current_allowed_direction = static_cast<int16_t>(Dir::N);
    s.sides[0].control.pl_very_close_to_ball = 1;
    s.sides[0].control.player_has_ball = 1;
    s.sides[0].control.normal_fire = 1;
    s.players[0].team_number = 1;
    s.players[0].player_ordinal = static_cast<int16_t>(ordinal);
    s.players[0].pos.x = Fix::FromInt(x);
    s.players[0].pos.y = Fix::FromInt(y);
    s.ball.pos.x = Fix::FromInt(x);
    s.ball.pos.y = Fix::FromInt(y);
    return s;
}

} // namespace

TEST_CASE("the attacking penalty area is the engine's own area") {
    MatchState s{};
    s.globals.team_playing_up = 2; // side 0 attacks the top
    CHECK(InAttackingPenaltyArea(s, 0, 336, 200));
    CHECK_FALSE(InAttackingPenaltyArea(s, 0, 336, 700)); // that is our own area
    CHECK_FALSE(InAttackingPenaltyArea(s, 0, 336, 449)); // centre
    CHECK_FALSE(InAttackingPenaltyArea(s, 0, 100, 200)); // wide of the corridor
    // Mirrored for the other side.
    CHECK(InAttackingPenaltyArea(s, 1, 336, 700));
    CHECK_FALSE(InAttackingPenaltyArea(s, 1, 336, 200));
}

TEST_CASE("in-box goalward shot adds Finishing bonus") {
    MatchState s = ShooterAt(336, 200, 1);
    s.sides[0].squad[0].attrs.finishing = 4;
    s.sides[0].squad[0].attrs.shooting = 7;

    CHECK(ApplyKickOrPass(s, 0));
    CHECK(s.ball.speed == kBallKickingSpeed + kBallSpeedFinishing[4]);
}

TEST_CASE("outside-box goalward shot adds Velocity bonus") {
    MatchState s = ShooterAt(336, 400, 2);
    s.sides[0].squad[1].attrs.finishing = 7;
    s.sides[0].squad[1].attrs.shooting = 3;

    CHECK(ApplyKickOrPass(s, 0));
    CHECK(s.ball.speed == kBallKickingSpeed + kBallSpeedKicking[3]);
}

TEST_CASE("a goalward kick from our own half is not a shot on goal") {
    MatchState s = ShooterAt(336, 600, 2);
    s.sides[0].squad[1].attrs.finishing = 7;
    s.sides[0].squad[1].attrs.shooting = 7;

    CHECK(ApplyKickOrPass(s, 0));
    CHECK(s.ball.speed == kBallKickingSpeed);
}
