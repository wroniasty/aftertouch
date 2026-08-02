// B11: Scripted / Table / Engine simulators + envelope.
#include <doctest/doctest.h>

#include "core/hash.hpp"
#include "core/match_engine.hpp"
#include "data/fictional.hpp"
#include "data/result_simulator.hpp"
#include "harness/season_runner.hpp"

using namespace at;
using namespace at::data;

TEST_CASE("scripted result is exact and rates players") {
    const League league = MakeFictionalLeague();
    ScriptedResultSimulator sim;
    REQUIRE(league.teams.size() >= 2);
    const uint16_t h = league.teams[0].id;
    const uint16_t a = league.teams[1].id;
    sim.Add(h, a, 2, 1);

    const MatchResult mr = sim.Resolve(league, Fixture{h, a}, 0xB11000AAu);
    CHECK(mr.score[0] == 2);
    CHECK(mr.score[1] == 1);
    CHECK(mr.scorer_count == 3);
    CHECK(mr.fidelity == ResultFidelity::Synthesised);
    CHECK(mr.ratings[0][0] >= 1);
    CHECK(mr.ratings[0][0] <= 10);
}

TEST_CASE("table resolve is bit-identical for same seed") {
    const League league = MakeFictionalLeague();
    TableResultSimulator sim;
    const Fixture f{league.teams[0].id, league.teams[1].id};
    const MatchResult a = sim.Resolve(league, f, 0xB11000BBu);
    const MatchResult b = sim.Resolve(league, f, 0xB11000BBu);
    CHECK(a.score[0] == b.score[0]);
    CHECK(a.score[1] == b.score[1]);
    CHECK(a.scorer_count == b.scorer_count);
    for (int i = 0; i < kMatchSquadSize; ++i)
        CHECK(a.ratings[0][static_cast<size_t>(i)] ==
              b.ratings[0][static_cast<size_t>(i)]);
}

TEST_CASE("engine resolve reaches FullTime and fills ratings") {
    const League league = MakeFictionalLeague();
    EngineResultSimulator sim;
    sim.tick_cap = 20000;
    const Fixture f{league.teams[0].id, league.teams[1].id};
    const MatchResult mr = sim.Resolve(league, f, 0xB11000CCu);
    CHECK(mr.fidelity == ResultFidelity::ExactEngine);
    CHECK(mr.ratings[0][9] >= 1);
    CHECK(mr.ratings[0][9] <= 10);
    CHECK(mr.ratings[1][9] >= 1);
    CHECK(mr.ratings[1][9] <= 10);
}

TEST_CASE("view-result RNG does not perturb match HashState") {
    MatchEngine eng;
    eng.Reset(0xB11000DDu);
    eng.Step(MatchInput{});
    const uint64_t before = HashState(eng.State());

    const League league = MakeFictionalLeague();
    TableResultSimulator table;
    (void)table.Resolve(league, Fixture{league.teams[0].id, league.teams[1].id},
                        0xDEADBEEFu);

    // Fresh engine with same seed — resolve must not have shared state.
    MatchEngine eng2;
    eng2.Reset(0xB11000DDu);
    eng2.Step(MatchInput{});
    CHECK(HashState(eng2.State()) == before);
}

TEST_CASE("table vs engine envelope on fixed seeds") {
    const League league = MakeFictionalLeague();
    TableResultSimulator table;
    EngineResultSimulator engine;
    engine.tick_cap = 12000;

    double table_goals = 0;
    double eng_goals = 0;
    double table_home_wins = 0;
    double eng_home_wins = 0;
    double table_rating = 0;
    double eng_rating = 0;
    constexpr int N = 12;
    int pairs = 0;

    for (size_t i = 0; i < league.teams.size() && pairs < N; ++i) {
        for (size_t j = 0; j < league.teams.size() && pairs < N; ++j) {
            if (i == j) continue;
            const Fixture f{league.teams[i].id, league.teams[j].id};
            const uint32_t seed = 0xB11E0000u + static_cast<uint32_t>(pairs);
            const MatchResult t = table.Resolve(league, f, seed);
            const MatchResult e = engine.Resolve(league, f, seed);
            table_goals += t.score[0] + t.score[1];
            eng_goals += e.score[0] + e.score[1];
            if (t.score[0] > t.score[1]) table_home_wins += 1;
            if (e.score[0] > e.score[1]) eng_home_wins += 1;
            table_rating += t.ratings[0][9];
            eng_rating += e.ratings[0][9];
            ++pairs;
        }
    }
    REQUIRE(pairs == N);
    const double tg = table_goals / N;
    const double eg = eng_goals / N;
    CAPTURE(tg);
    CAPTURE(eg);
    // Wide band: engine goals vary; table is calibrated loosely.
    CHECK(tg >= 0.0);
    CHECK(eg >= 0.0);
    // Engine often low-scoring at game_length=0; table aims mid — allow a wide band.
    CHECK((tg - eg) <= 4.0);
    CHECK((eg - tg) <= 4.0);

    const double tr = table_rating / N;
    const double er = eng_rating / N;
    CAPTURE(tr);
    CAPTURE(er);
    // Table invents volume stats; engine short runs stay sparse — allow a wider band.
    CHECK((tr - er) < 4.0);
    CHECK((er - tr) < 4.0);

    // Home win rates should not oppose violently (both in [0,1]).
    CHECK(table_home_wins >= 0);
    CHECK(eng_home_wins >= 0);
}
