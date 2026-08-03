// Byline -> team conventions: goal attribution, last-man foul, chase capture.
// Regression cover for the defects found in traces/match_C1A00001_01.txt, where
// all five goals went into the bottom net and all five were credited to HOME.
#include <doctest/doctest.h>

#include "core/ai.hpp"
#include "core/ball.hpp"
#include "core/match_engine.hpp"
#include "core/match_input.hpp"
#include "core/movement.hpp"
#include "core/possession.hpp"
#include "core/set_pieces.hpp"

using namespace at;

namespace {

// Minimal in-progress state with both squads populated so PlacePlayersAtKickoff
// and the squad-index attribution have something to work with.
MatchState MakeLiveState(uint8_t team_playing_up) {
    MatchState s{};
    SetPl(s, GameStatePl::InProgress);
    SetGameState(s, GameState::StartingGame);
    s.globals.team_playing_up = team_playing_up;
    s.clock.last_team_played = 1;
    s.clock.match_started = 1;
    for (int i = 0; i < kPitchPlayers; ++i) {
        Entity& e = s.players[static_cast<size_t>(i)];
        e.team_number = static_cast<int16_t>(i < 11 ? 1 : 2);
        e.player_ordinal = static_cast<int16_t>((i % 11) + 1);
        e.player_state = static_cast<uint8_t>(PlayerState::Normal);
        e.pos.x = Fix::FromInt(static_cast<int16_t>(100 + i));
        e.pos.y = Fix::FromInt(static_cast<int16_t>(200 + i));
    }
    return s;
}

// Drive a goal through the real wiring: ball in the mouth, past a byline.
MatchState ScoreAt(uint8_t team_playing_up, int16_t y) {
    MatchState s = MakeLiveState(team_playing_up);
    s.ball.pos.x = Fix::FromInt(336); // inside the mouth (303..367)
    s.ball.pos.y = Fix::FromInt(y);
    s.ball.pos.z = Fix{};
    WireOutOfPlay(s);
    return s;
}

constexpr int16_t kPastTop = 120;  // < kPlayableMinY (129)
constexpr int16_t kPastBot = 800;  // > kPlayableMaxY (769)

} // namespace

TEST_CASE("ScorerForGoalY credits the side that attacks that byline") {
    // SIMULATION §3: team_playing_up DEFENDS the top line.
    MatchState up1 = MakeLiveState(1);
    CHECK(ScorerForGoalY(up1, kPastTop) == 2); // team 1's own net
    CHECK(ScorerForGoalY(up1, kPastBot) == 1);

    MatchState up2 = MakeLiveState(2);
    CHECK(ScorerForGoalY(up2, kPastTop) == 1);
    CHECK(ScorerForGoalY(up2, kPastBot) == 2); // team 2's own net

    // Unset — caller must skip the bump rather than guess.
    MatchState unset = MakeLiveState(0);
    CHECK(ScorerForGoalY(unset, kPastTop) == 0);
    CHECK(ScorerForGoalY(unset, kPastBot) == 0);
}

TEST_CASE("goal in a net is credited to the other side") {
    // The trace case: team 2 plays up (defends top), ball into the bottom net,
    // which team 1 defends -> team 2 scores. Before the fix this scored team 1.
    SUBCASE("up=2, bottom net") {
        MatchState s = ScoreAt(2, kPastBot);
        CHECK(s.phase == MatchPhase::Goal);
        CHECK(s.score[0] == 0);
        CHECK(s.score[1] == 1);
    }
    SUBCASE("up=2, top net") {
        MatchState s = ScoreAt(2, kPastTop);
        CHECK(s.score[0] == 1);
        CHECK(s.score[1] == 0);
    }
    SUBCASE("up=1, bottom net") {
        MatchState s = ScoreAt(1, kPastBot);
        CHECK(s.score[0] == 1);
        CHECK(s.score[1] == 0);
    }
    SUBCASE("up=1, top net") {
        MatchState s = ScoreAt(1, kPastTop);
        CHECK(s.score[0] == 0);
        CHECK(s.score[1] == 1);
    }
}

TEST_CASE("goal bumps the scoring side's squad and chronicle") {
    MatchState s = ScoreAt(2, kPastBot); // team 2 scores
    int total = 0;
    for (int i = 0; i < kMatchSquadSize; ++i)
        total += s.sides[1].squad[static_cast<size_t>(i)].goals_scored;
    CHECK(total == 1);
    for (int i = 0; i < kMatchSquadSize; ++i)
        CHECK(s.sides[0].squad[static_cast<size_t>(i)].goals_scored == 0);

    REQUIRE(s.chronicle.count >= 1);
    bool found = false;
    for (int i = 0; i < s.chronicle.count; ++i) {
        const auto& e = s.chronicle.events[static_cast<size_t>(i)];
        if (e.kind == static_cast<uint8_t>(MatchEventKind::Goal)) {
            CHECK(e.side == 1);
            found = true;
        }
    }
    CHECK(found);
}

TEST_CASE("the conceding side restarts after a goal") {
    // Team 2 scores into the bottom net, so team 1 kicks off.
    MatchState s = ScoreAt(2, kPastBot);
    CHECK(s.globals.last_team_played_before_break == 1);
    CHECK(s.ball.pos.x.Whole() == kCentreSpotX);
    CHECK(s.ball.pos.y.Whole() == kCentreSpotY);
}

TEST_CASE("IsLastManFoul measures against the offender's own goal") {
    // team_playing_up = 1 defends the top, so team 1's own goal is kPlayableMinY.
    MatchState s = MakeLiveState(1);
    // Victim parked on the top goal line; every team-1 outfielder deep at the
    // other end. Before the fix goal_y was kPlayableMaxY and this read false.
    s.players[15].pos.x = Fix::FromInt(kCentreSpotX);
    s.players[15].pos.y = Fix::FromInt(kPlayableMinY + 100);
    for (int i = 1; i < 11; ++i) {
        s.players[static_cast<size_t>(i)].pos.x = Fix::FromInt(kCentreSpotX);
        s.players[static_cast<size_t>(i)].pos.y = Fix::FromInt(kPlayableMaxY);
    }
    CHECK(IsLastManFoul(s, 0, 15));

    // One defender goal-side of the victim -> not the last man.
    s.players[5].pos.y = Fix::FromInt(kPlayableMinY + 10);
    CHECK_FALSE(IsLastManFoul(s, 0, 15));
}

TEST_CASE("cpu chaser converts a close loose ball into possession") {
    // Exact geometry from traces/match_C1A00001_01.txt t=5071..5223, where the
    // away controlled player orbited a dead ball for 153 ticks. Squared distance
    // at (418,694) is 52 — inside kDistCloseSq (72) but outside the capture
    // radius kDistVeryCloseSq (32), the annulus that used to trap him.
    MatchEngine eng;
    eng.Reset(0xC1A00001u);
    eng.Step(MatchInput{});

    MatchState s = eng.State();
    s.sides[0].control.player_number = 1; // home human, as in the trace
    s.sides[1].control.player_number = 0; // away CPU
    s.globals.team_playing_up = 2;        // away attacks the bottom net
    PlacePlayersAtKickoff(s);
    SetPl(s, GameStatePl::InProgress);
    SetGameState(s, GameState::StartingGame);
    s.phase = MatchPhase::InPlay;
    s.clock.stoppage_event_timer = 0;

    s.ball.pos.x = Fix::FromInt(424);
    s.ball.pos.y = Fix::FromInt(690);
    s.ball.pos.z = Fix{};
    s.ball.dest_x = 424;
    s.ball.dest_y = 690;
    s.ball.speed = 0;
    s.ball.delta = {};

    s.sides[1].control.controlled_slot = 21;
    s.sides[1].control.player_has_ball = 0;
    Entity& chaser = s.players[21];
    chaser.pos.x = Fix::FromInt(418);
    chaser.pos.y = Fix::FromInt(694);
    chaser.speed = 0;
    chaser.delta = {};
    for (int i = 0; i < 2; ++i) {
        auto& tc = s.sides[static_cast<size_t>(i)].control;
        tc.ball_in_play = 1;
        tc.ball_can_be_controlled = 1;
    }
    eng.LoadState(s);

    bool captured = false;
    int32_t closest = 0x7fffffff;
    for (int i = 0; i < 40 && !captured; ++i) {
        eng.Step(MatchInput{});
        const MatchState& st = eng.State();
        const int32_t d = PossessionBallDistSq(st.players[21], st.ball);
        if (d < closest) closest = d;
        if (st.sides[1].control.player_has_ball) captured = true;
    }
    CAPTURE(closest);
    CHECK(captured);
}
