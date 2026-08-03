// Restart take: stick aims in place — no translation (B8 / play-feel).
#include <doctest/doctest.h>

#include "core/movement.hpp"
#include "core/set_pieces.hpp"

using namespace at;

TEST_CASE("restart take: held direction aims without moving") {
    MatchState s{};
    BeginRestart(s, GameState::FreeKickCentre, 336, 400, kTurnFlagsAll, 0, 1);
    PickRestartTaker(s, 0);
    PlaceTakerNearSpot(s, 0);

    TeamControl& tc = s.sides[0].control;
    tc.player_number = 1;
    const int slot = tc.controlled_slot;
    REQUIRE(slot >= 0);
    REQUIRE(slot != 0); // outfield, not GK
    Entity& e = s.players[static_cast<size_t>(slot)];
    const int16_t x0 = e.pos.x.Whole();
    const int16_t y0 = e.pos.y.Whole();

    MatchInput in{};
    in.p1.dir = Dir::E;
    for (int i = 0; i < 10; ++i) {
        s.globals.team_switch_counter = 1; // home side next
        ApplyTeamControls(s, in);
        MovePlayers(s);
    }

    CHECK(e.pos.x.Whole() == x0);
    CHECK(e.pos.y.Whole() == y0);
    CHECK(e.dest_x == x0);
    CHECK(e.dest_y == y0);
    CHECK(tc.current_allowed_direction == static_cast<int16_t>(Dir::E));
    CHECK(e.direction == static_cast<int16_t>(Dir::E));
}

// B13 / R8 reversed this case. It used to assert a goal kick is taken by an
// outfielder, which was never sourced: B8 §7 lists "goal-kick / keeper-holds
// release nuance" as an *open question* and SETPIECES.md records the release as
// unread, so "nearest outfield" was the fallback branch of PickRestartTaker
// rather than a decision. Meanwhile the Amiga places a goal kick at the six-yard
// box (396/276, y 154/744) — where the keeper takes it from.
//
// The visible defect was worse than the choice of taker: with the ball left
// where it crossed the byline, an outfielder was teleported *off the pitch* onto
// the goal line to take it.
TEST_CASE("goal kick is taken by the keeper") {
    MatchState s{};
    s.sides[0].control.player_number = 1;
    s.sides[0].control.controlled_slot = 0; // stuck on GK
    s.players[0].team_number = 1;
    s.players[0].player_ordinal = 1;
    for (int i = 1; i < 11; ++i) {
        s.players[static_cast<size_t>(i)].team_number = 1;
        s.players[static_cast<size_t>(i)].player_ordinal =
            static_cast<int16_t>(i + 1);
        s.players[static_cast<size_t>(i)].pos.x = Fix::FromInt(200 + i * 10);
        s.players[static_cast<size_t>(i)].pos.y = Fix::FromInt(150);
    }

    BeginRestart(s, GameState::GoalOutLeft, 336, 129, kTurnFlagsAll, 0, 1);
    PickRestartTaker(s, 0);
    CHECK(s.sides[0].control.controlled_slot == 0);
}

// The placement table (amiga SETPIECES §2). The ball used to be restarted where
// it crossed the line, which for a goal kick is a point on the byline, off the
// pitch — the C1A trace shows one sitting at (439,770) for 145 ticks.
TEST_CASE("restarts are placed at their spot, not where the ball went out") {
    MatchState s{};

    SUBCASE("goal kick, bottom goal, right half") {
        const Dest d = RestartSpot(s, GameState::GoalOutRight, 439, 770);
        CHECK(d.x == kGoalKickXRight);
        CHECK(d.y == kGoalKickYBot);
        CHECK(d.y < kPlayableMaxY); // on the pitch, not on the byline
    }
    SUBCASE("goal kick, top goal, left half") {
        const Dest d = RestartSpot(s, GameState::GoalOutLeft, 300, 120);
        CHECK(d.x == kGoalKickXLeft);
        CHECK(d.y == kGoalKickYTop);
        CHECK(d.y > kPlayableMinY);
    }
    SUBCASE("corners go to the flag") {
        CHECK(RestartSpot(s, GameState::CornerLeft, 200, 120).x == kCornerXLeft);
        CHECK(RestartSpot(s, GameState::CornerLeft, 200, 120).y == kCornerYTop);
        CHECK(RestartSpot(s, GameState::CornerRight, 500, 790).x == kCornerXRight);
        CHECK(RestartSpot(s, GameState::CornerRight, 500, 790).y == kCornerYBot);
    }
    SUBCASE("a throw-in keeps its own y but is pinned to the touchline") {
        const Dest d = RestartSpot(s, GameState::ThrowInCentreLeft, 60, 400);
        CHECK(d.x == kThrowInXLeft);
        CHECK(d.y == 400);
    }
}

TEST_CASE("keeper holds keeps GK as taker") {
    MatchState s{};
    BeginRestart(s, GameState::KeeperHoldsBall, 336, 180, 0xFF, 0, 1);
    PickRestartTaker(s, 0);
    CHECK(s.sides[0].control.controlled_slot == 0);
}
