// B5: capture, release, lockout.
#include <doctest/doctest.h>

#include "core/possession.hpp"

using namespace at;

namespace {

MatchState MakeNearBall() {
    MatchState s{};
    SetPl(s, GameStatePl::InProgress);
    s.sides[0].control.controlled_slot = 0;
    s.sides[0].control.ball_can_be_controlled = 1;
    s.sides[0].control.pass_kick_timer = 0;
    s.players[0].team_number = 1;
    s.players[0].player_ordinal = 2;
    s.players[0].player_state = static_cast<uint8_t>(PlayerState::Normal);
    s.players[0].pos.x = Fix::FromInt(200);
    s.players[0].pos.y = Fix::FromInt(400);
    s.ball.pos.x = Fix::FromInt(201);
    s.ball.pos.y = Fix::FromInt(400);
    return s;
}

} // namespace

TEST_CASE("very close captures the ball") {
    MatchState s = MakeNearBall();
    UpdatePossessionForSide(s, 0);
    CHECK(s.sides[0].control.player_has_ball == 1);
    CHECK(s.clock.last_team_played == 1);
}

TEST_CASE("leaving close band releases possession") {
    MatchState s = MakeNearBall();
    UpdatePossessionForSide(s, 0);
    REQUIRE(s.sides[0].control.player_has_ball == 1);

    // Move ball far away.
    s.ball.pos.x = Fix::FromInt(400);
    UpdatePossessionForSide(s, 0);
    CHECK(s.sides[0].control.player_has_ball == 0);
    CHECK(s.sides[0].control.ball_out_of_play == 1);
}

TEST_CASE("pass_kick_timer lockout blocks the kicker re-capturing") {
    MatchState s = MakeNearBall();
    s.sides[0].control.pass_kick_timer = 3;
    s.sides[0].control.ball_can_be_controlled = 0;
    // The lockout is the *kicker's*. It used to be applied to the whole side,
    // which also stopped team-mates receiving the pass (see the case below).
    s.sides[0].control.passing_kicking_slot = 0;
    UpdatePossessionForSide(s, 0);
    CHECK(s.sides[0].control.player_has_ball == 0);
    CHECK(s.sides[0].control.pass_kick_timer == 2);

    UpdatePossessionForSide(s, 0); // 1
    UpdatePossessionForSide(s, 0); // 0 → controllable
    CHECK(s.sides[0].control.pass_kick_timer == 0);
    CHECK(s.sides[0].control.ball_can_be_controlled == 1);
    UpdatePossessionForSide(s, 0);
    CHECK(s.sides[0].control.player_has_ball == 1);
}

// The bug this pins against: a side's TeamControl is shared by all eleven
// players, and it is only served every other frame, so a 25-tick post-kick
// lockout forbade the *receiver* from taking his own team's pass for a full
// second of real time. Short passes rolled straight through the man they were
// aimed at — reported as "the receiver doesn't control it, it bounces off him".
TEST_CASE("a team-mate may receive a pass while the kicker is locked out") {
    MatchState s = MakeNearBall();
    // Slot 1 is standing on the ball; slot 0 has just played it.
    s.players[1].team_number = 1;
    s.players[1].player_ordinal = 3;
    s.players[1].player_state = static_cast<uint8_t>(PlayerState::Normal);
    s.players[1].pos.x = Fix::FromInt(201);
    s.players[1].pos.y = Fix::FromInt(400);

    s.sides[0].control.controlled_slot = 1;
    s.sides[0].control.passing_kicking_slot = 0;
    s.sides[0].control.pass_kick_timer = 25;
    s.sides[0].control.ball_can_be_controlled = 0;

    UpdatePossessionForSide(s, 0);
    CHECK(s.sides[0].control.player_has_ball == 1);
}

// passing_kicking_slot excludes a player from selection and from being a pass
// target. It used to be set on every kick and never cleared, so the last man to
// touch the ball was permanently unselectable — the "nobody closes on the ball"
// report. It must not outlive the lockout it shares a purpose with.
TEST_CASE("the kicker exclusion expires with the kick lockout") {
    MatchState s = MakeNearBall();
    TeamControl& tc = s.sides[0].control;
    tc.passing_kicking_slot = 0;
    tc.pass_kick_timer = 3;
    tc.ball_can_be_controlled = 0;

    for (int i = 0; i < 2; ++i) {
        UpdatePossessionForSide(s, 0);
        CHECK(tc.passing_kicking_slot == 0); // still locked out
    }
    UpdatePossessionForSide(s, 0);
    CHECK(tc.pass_kick_timer == 0);
    CHECK(tc.passing_kicking_slot == -1); // released, and selectable again
}
