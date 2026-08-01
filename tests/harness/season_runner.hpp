#pragma once
#include "core/match_engine.hpp"
#include "data/game_data.hpp"

#include <cstdint>
#include <vector>

namespace at::harness {

struct MatchResult {
    uint16_t home_id    = 0;
    uint16_t away_id    = 0;
    uint8_t  home_goals = 0;
    uint8_t  away_goals = 0;
    uint32_t ticks      = 0;
    bool     squad_ok   = false;  // ApplyKickoff succeeded for this pair
};

struct SeasonReport {
    std::vector<MatchResult> matches;
};

// Round-robin every ordered pair: project squads (A5), then step the engine
// headless for a fixed tick count. Scores stay 0–0 until B2 owns the clock; the
// runner is the E/B11 hook (A6).
inline SeasonReport RunRoundRobin(const data::League& league, uint32_t ticks_per_match,
                                  uint32_t seed_base = 1) {
    SeasonReport report;
    for (size_t i = 0; i < league.teams.size(); ++i) {
        for (size_t j = 0; j < league.teams.size(); ++j) {
            if (i == j) continue;

            MatchResult mr;
            mr.home_id = league.teams[i].id;
            mr.away_id = league.teams[j].id;

            MatchState sheet;
            mr.squad_ok = data::ApplyKickoff(league, mr.home_id, mr.away_id, sheet);
            if (!mr.squad_ok) {
                report.matches.push_back(mr);
                continue;
            }

            MatchEngine sim;
            sim.Reset(seed_base + static_cast<uint32_t>(report.matches.size()));
            for (uint32_t t = 0; t < ticks_per_match; ++t) {
                sim.Step(MatchInput{});
            }
            mr.home_goals = sim.State().score[0];
            mr.away_goals = sim.State().score[1];
            mr.ticks      = sim.State().tick;
            report.matches.push_back(mr);
        }
    }
    return report;
}

} // namespace at::harness
