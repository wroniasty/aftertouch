// Goalkeeper positioning — the Amiga arc-and-band map.
//
// Properties, not coordinates: these must survive the day the constants are
// fitted against a trace. The bug they pin against is that the keeper AI used a
// completely different rule from the one in OffBallDestination — his destination
// was the midpoint between the ball and his own goal line, on a 254-pixel arc, so
// a ball at the halfway line pulled him 160 units off his line and out of his box.
#include <doctest/doctest.h>

#include "core/goalkeeper.hpp"
#include "core/movement.hpp"

using namespace at;

namespace {

MatchState MakeKeeperScene(uint8_t playing_up) {
    MatchState s{};
    SetPl(s, GameStatePl::InProgress);
    s.globals.team_playing_up = playing_up;
    for (int side = 0; side < 2; ++side) {
        TeamControl& tc = s.sides[static_cast<size_t>(side)].control;
        tc.team_number = static_cast<uint8_t>(side + 1);
        tc.controlled_slot = static_cast<int8_t>(side * 11 + 5);
        Entity& gk = s.players[static_cast<size_t>(side * 11)];
        gk.team_number = static_cast<int16_t>(side + 1);
        gk.player_ordinal = 1;
        gk.player_state = static_cast<uint8_t>(PlayerState::Normal);
        gk.cards = 0;
        gk.pos.x = Fix::FromInt(kCentreSpotX);
        gk.pos.y = Fix::FromInt(side == 0 ? kKeeperTopBase : kKeeperBotBase);
    }
    return s;
}

} // namespace

TEST_CASE("the keeper never leaves his arc or his band, for any ball position") {
    for (uint8_t up = 1; up <= 2; ++up) {
        for (int16_t bx = kPlayableMinX; bx <= kPlayableMaxX; bx += 17) {
            for (int16_t by = kPlayableMinY; by <= kPlayableMaxY; by += 21) {
                MatchState s = MakeKeeperScene(up);
                s.ball.pos.x = Fix::FromInt(bx);
                s.ball.pos.y = Fix::FromInt(by);
                for (int side = 0; side < 2; ++side) {
                    const Dest d = KeeperRestDestination(s, side);
                    CAPTURE(up); CAPTURE(bx); CAPTURE(by); CAPTURE(side);
                    CHECK(d.x >= kKeeperArcX);
                    CHECK(d.x <= kKeeperArcX + kKeeperArcSpan);
                    const bool top = OwnGoalLineY(s, side) < kCentreSpotY;
                    if (top) {
                        CHECK(d.y >= kKeeperTopBase);
                        CHECK(d.y <= kKeeperTopLimit);
                    } else {
                        CHECK(d.y >= kKeeperBotBase);
                        CHECK(d.y <= kKeeperBotLimit);
                    }
                }
            }
        }
    }
}

TEST_CASE("the keeper stays inside his own penalty area") {
    // The old rule put him on the halfway line when the ball was at the far end.
    MatchState s = MakeKeeperScene(1);
    for (int16_t by : {kPlayableMinY, int16_t{300}, kCentreSpotY, int16_t{600},
                       kPlayableMaxY}) {
        s.ball.pos.x = Fix::FromInt(kCentreSpotX);
        s.ball.pos.y = Fix::FromInt(by);
        CAPTURE(by);
        // Side 0 defends the top when team_playing_up == 1.
        const Dest top = KeeperRestDestination(s, 0);
        CHECK(top.y <= kPenaltyBoxTopY);
        const Dest bot = KeeperRestDestination(s, 1);
        CHECK(bot.y >= kPenaltyBoxBotY);
        // And within the penalty area horizontally.
        CHECK(top.x >= kPenBoxXMin);
        CHECK(top.x <= kPenBoxXMax);
    }
}

TEST_CASE("the keeper comes off his line as the ball goes away from him") {
    MatchState s = MakeKeeperScene(1); // side 0 defends the top
    s.ball.pos.x = Fix::FromInt(kCentreSpotX);

    s.ball.pos.y = Fix::FromInt(kPlayableMinY); // ball at his feet
    const int16_t at_feet = KeeperRestDestination(s, 0).y;
    s.ball.pos.y = Fix::FromInt(kPlayableMaxY); // ball at the far end
    const int16_t far_away = KeeperRestDestination(s, 0).y;

    const int16_t line = kPlayableMinY;
    CHECK(std::abs(at_feet - line) < std::abs(far_away - line));
    CHECK(std::abs(far_away - at_feet) <= kKeeperBandSpan);
}

TEST_CASE("the keeper tracks the ball across, but only within the arc") {
    MatchState s = MakeKeeperScene(1);
    s.ball.pos.y = Fix::FromInt(kCentreSpotY);

    s.ball.pos.x = Fix::FromInt(kPlayableMinX);
    const int16_t left = KeeperRestDestination(s, 0).x;
    s.ball.pos.x = Fix::FromInt(kPlayableMaxX);
    const int16_t right = KeeperRestDestination(s, 0).x;

    CHECK(left < right);                    // he does follow the ball
    CHECK(right - left <= kKeeperArcSpan);  // but only across the arc
    // About twenty pixels past each post, not a third of the pitch.
    CHECK(left <= kGoalMouthMinX);
    CHECK(right >= kGoalMouthMaxX);
}

TEST_CASE("both keeper callers agree on the destination") {
    // OffBallDestination used to hold a second copy of this formula, and the
    // copy that actually ran (the keeper AI) implemented a different rule.
    MatchState s = MakeKeeperScene(1);
    for (int16_t bx = 100; bx < 580; bx += 60) {
        for (int16_t by = 150; by < 760; by += 70) {
            s.ball.pos.x = Fix::FromInt(bx);
            s.ball.pos.y = Fix::FromInt(by);
            for (int side = 0; side < 2; ++side) {
                CAPTURE(bx); CAPTURE(by); CAPTURE(side);
                const Dest a = KeeperRestDestination(s, side);
                const Dest b = OffBallDestination(s, side, 1);
                CHECK(a.x == b.x);
                CHECK(a.y == b.y);
            }
        }
    }
}

TEST_CASE("a stranded claim state clears once play resumes without the ball") {
    // GoalieClaimed was only ever cleared for the player who took the restart,
    // so a keeper who claimed and did not take it stayed latched — and the early
    // return in ApplyGoalkeeperAI then froze him wherever he stood.
    MatchState s = MakeKeeperScene(1);
    Entity& gk = s.players[0];
    gk.player_state = static_cast<uint8_t>(PlayerState::GoalieClaimed);
    gk.pos.x = Fix::FromInt(500); // stranded upfield
    gk.pos.y = Fix::FromInt(400);
    s.ball.pos.x = Fix::FromInt(kCentreSpotX);
    s.ball.pos.y = Fix::FromInt(600);
    s.sides[0].control.player_has_ball = 0;

    ApplyGoalkeeperAI(s, 0);

    CHECK(static_cast<PlayerState>(gk.player_state) == PlayerState::Normal);
    CHECK(gk.dest_y <= kPenaltyBoxTopY); // and he is heading home
}

TEST_CASE("a keeper actually holding the ball is left alone") {
    MatchState s = MakeKeeperScene(1);
    Entity& gk = s.players[0];
    gk.player_state = static_cast<uint8_t>(PlayerState::GoalieClaimed);
    s.sides[0].control.player_has_ball = 1;
    s.sides[0].control.controlled_slot = 0;

    ApplyGoalkeeperAI(s, 0);
    CHECK(static_cast<PlayerState>(gk.player_state) == PlayerState::GoalieClaimed);
}
