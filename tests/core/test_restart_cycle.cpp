// B8 acceptance: scripted throw-in take → HashState pin.
#include <doctest/doctest.h>

#include "core/hash.hpp"
#include "core/match_engine.hpp"
#include "core/match_input.hpp"
#include "core/movement.hpp"
#include "core/set_pieces.hpp"

using namespace at;

namespace {

uint64_t RunRestartCycle() {
    MatchEngine eng;
    eng.Reset(0xB8000001u);
    eng.Step(MatchInput{});

    MatchState s = eng.State();
    s.sides[0].control.player_number = 1;
    s.sides[0].control.controlled_slot = 9;
    s.globals.team_playing_up = 1;
    for (int r = 0; r < kMatchTacticRoles; ++r)
        for (int q = 0; q < kMatchBallQuadrants; ++q)
            s.sides[0].tactics.cells[static_cast<size_t>(r)][static_cast<size_t>(q)] =
                static_cast<uint8_t>(((r % 15) << 4) | (q % 16));
    PlacePlayersAtKickoff(s);

    BeginRestart(s, GameState::ThrowInCentreLeft, 70, 449, 0x1C, 4, 1);
    PlaceThrowInTaker(s, 0);
    s.sides[0].control.controlled_slot = 9;
    s.players[9].team_number = 1;
    s.players[9].player_ordinal = 10;
    s.players[9].pos.x = Fix::FromInt(67);
    s.players[9].pos.y = Fix::FromInt(449);
    s.players[9].player_state = static_cast<uint8_t>(PlayerState::ThrowIn);
    s.players[9].direction = static_cast<int16_t>(Dir::E);

    eng.LoadState(s);

    MatchInput in{};
    // Fire edge on a side-0 control tick.
    for (int i = 0; i < 8; ++i) {
        in.p1.dir = Dir::E;
        in.p1.fire = (i >= 2 && i <= 4);
        eng.Step(in);
    }
    for (int i = 0; i < 30; ++i) {
        in.p1.fire = false;
        eng.Step(in);
    }
    return HashState(eng.State());
}

} // namespace

TEST_CASE("restart cycle reaches InProgress") {
    MatchEngine eng;
    eng.Reset(0xB80000AAu);
    eng.Step(MatchInput{});
    MatchState s = eng.State();
    s.sides[0].control.player_number = 1;
    s.sides[0].control.controlled_slot = 1;
    BeginRestart(s, GameState::FreeKickCentre, 336, 250, kTurnFlagsAll, 4, 1);
    PlaceTakerNearSpot(s, 0);
    eng.LoadState(s);

    MatchInput in{};
    in.p1.dir = Dir::N;
    // Hold past kFireHoldThreshold so normal_fire arms (not button-down).
    in.p1.fire = true;
    for (int i = 0; i < 16; ++i) eng.Step(in);

    CHECK(GetPl(eng.State()) == GameStatePl::InProgress);
}

TEST_CASE("scripted restart cycle hash is stable") {
    const uint64_t a = RunRestartCycle();
    CHECK(a == RunRestartCycle());
    // Re-pinned B13 / R4: the corner turn masks. There are four of them, one per
    // corner flag, where this engine reused the two penalty arcs — so a taker in
    // the top-left corner was previously permitted octants that face out of the
    // pitch. A CPU taker is additionally denied the horizontal axis.
    //
    // Previously: repinned when MatchPhase::SetPiece was added — a stopped
    // throw-in used to leak the previous phase through HashState.
    // Re-pinned B13 / R6 (pass fixes): pass targeting is the Amiga's — no range
    // limit, cone anchored at the ball at +-22.5 degrees, aim ray extended past the
    // receiver — and the post-kick lockout no longer blocks team-mates from
    // receiving. Pass strength and the Passing bonus are now the sourced tables.
    // Re-pinned B13 / R7 (goalkeeper): the keeper's resting destination is the
    // Amiga arc-and-band map — a 103px arc across his goal and a 27px depth
    // band — instead of the midpoint between the ball and his own goal line on
    // a 254px arc, which sent him to the halfway line whenever the ball was at
    // the far end. Both keeper callers now share one formula.
    // Re-pinned B13 / R8 (restart placement): a restart now puts the ball on its
    // spot instead of where it crossed the line — goal kicks at the six-yard box
    // (396/276, y 154/744), corners at the flag, throw-ins pinned to the
    // touchline — and the keeper takes his own goal kick.
    constexpr uint64_t kExpected = 0x94cd3b89b64b247cull;
    CAPTURE(a);
    CHECK(a == kExpected);
}
