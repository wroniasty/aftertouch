#pragma once
#include "core/match_engine.hpp"
#include "core/match_result.hpp"
#include "data/game_data.hpp"
#include "data/result_simulator.hpp"

#include <cstdint>
#include <vector>

namespace at::harness {

// Thin report wrapper around core MatchResult (B11).
struct SeasonReport {
    std::vector<MatchResult> matches;
};

// Round-robin via IResultSimulator (default: Engine, short tick cap for A6).
inline SeasonReport RunRoundRobin(const data::League& league, uint32_t ticks_per_match,
                                  uint32_t seed_base = 1) {
    data::EngineResultSimulator sim;
    sim.tick_cap = ticks_per_match; // may end before FullTime — still extracts score

    SeasonReport report;
    for (size_t i = 0; i < league.teams.size(); ++i) {
        for (size_t j = 0; j < league.teams.size(); ++j) {
            if (i == j) continue;
            Fixture f{league.teams[i].id, league.teams[j].id};
            const uint32_t seed =
                seed_base + static_cast<uint32_t>(report.matches.size());
            MatchResult mr = sim.Resolve(league, f, seed);
            report.matches.push_back(mr);
        }
    }
    return report;
}

// Table-backend round-robin for distribution tests.
inline SeasonReport RunRoundRobinTable(const data::League& league,
                                       uint32_t seed_base = 1) {
    data::TableResultSimulator sim;
    SeasonReport report;
    for (size_t i = 0; i < league.teams.size(); ++i) {
        for (size_t j = 0; j < league.teams.size(); ++j) {
            if (i == j) continue;
            Fixture f{league.teams[i].id, league.teams[j].id};
            const uint32_t seed =
                seed_base + static_cast<uint32_t>(report.matches.size());
            report.matches.push_back(sim.Resolve(league, f, seed));
        }
    }
    return report;
}

} // namespace at::harness
