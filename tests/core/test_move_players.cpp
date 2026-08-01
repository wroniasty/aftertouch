// B4 acceptance: 22 players under scripted input, 200 ticks, HashState pin.
#include <doctest/doctest.h>

#include "core/hash.hpp"
#include "core/match_engine.hpp"
#include "core/movement.hpp"

using namespace at;

namespace {

void SeedFormation(MatchState& s) {
    for (int side = 0; side < 2; ++side) {
        for (int r = 0; r < kMatchTacticRoles; ++r) {
            for (int q = 0; q < kMatchBallQuadrants; ++q) {
                const uint8_t x = static_cast<uint8_t>((r + q) % 15);
                const uint8_t y = static_cast<uint8_t>((r * 2 + q / 5) % 16);
                s.sides[static_cast<size_t>(side)].tactics.cells
                    [static_cast<size_t>(r)][static_cast<size_t>(q)] =
                    static_cast<uint8_t>((x << 4) | y);
            }
        }
        s.sides[static_cast<size_t>(side)].control.player_number =
            static_cast<uint8_t>(side + 1);
        s.sides[static_cast<size_t>(side)].control.controlled_slot =
            static_cast<int8_t>(side * 11 + 9); // a striker
        for (int i = 0; i < 11; ++i) {
            s.sides[static_cast<size_t>(side)].squad[static_cast<size_t>(i)].attrs.speed =
                static_cast<uint8_t>(3 + (i % 5));
        }
    }
}

uint64_t RunScenario() {
    MatchEngine eng;
    eng.Reset(0xB4000001u);

    // Boot place + seed formation/human flags before further steps.
    eng.Step(MatchInput{});
    MatchState s = eng.State();
    SeedFormation(s);
    PlacePlayersAtKickoff(s);
    SetPl(s, GameStatePl::InProgress);
    SetGameState(s, GameState::StartingGame);
    s.clock.stoppage_event_timer = 0;
    s.phase = MatchPhase::InPlay;
    eng.LoadState(s);

    MatchInput in{};
    for (int i = 0; i < 200; ++i) {
        in.p1.dir = static_cast<Dir>(i % 8);
        in.p1.fire = (i % 7) == 0;
        in.p2.dir = static_cast<Dir>((i * 3) % 8);
        in.p2.fire = false;
        eng.Step(in);
    }
    return HashState(eng.State());
}

} // namespace

TEST_CASE("twenty-two players place and controlled player moves") {
    MatchEngine eng;
    eng.Reset(0xB40000AAu);
    eng.Step(MatchInput{});
    MatchState s = eng.State();
    SeedFormation(s);
    PlacePlayersAtKickoff(s);
    SetPl(s, GameStatePl::InProgress);
    SetGameState(s, GameState::StartingGame);
    s.clock.stoppage_event_timer = 0;
    s.phase = MatchPhase::InPlay;
    // All 22 off origin.
    for (int i = 0; i < kPitchPlayers; ++i) {
        CHECK(s.players[static_cast<size_t>(i)].pos.x.Whole() != 0);
        CHECK(s.players[static_cast<size_t>(i)].pos.y.Whole() != 0);
    }
    eng.LoadState(s);

    const int16_t x0 = eng.State().players[9].pos.x.Whole();
    MatchInput in{};
    in.p1.dir = Dir::E;
    for (int i = 0; i < 40; ++i) eng.Step(in);
    // Controlled home striker should have moved (side 0 updates every other tick).
    CHECK(eng.State().players[9].pos.x.Whole() != x0);
}

TEST_CASE("scripted 200-tick movement hash is stable") {
    const uint64_t a = RunScenario();
    CHECK(a == RunScenario());

    // Pinned after B4. If movement changes on purpose, print and update.
    constexpr uint64_t kExpected = 0x84f9b3fa55ca8c13ull;
    CHECK(a == kExpected);
}
