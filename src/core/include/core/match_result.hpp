#pragma once
#include "core/match_state.hpp"
#include "core/performance_rating.hpp"

#include <array>
#include <cstdint>

// Shared result envelope for B11 backends — doc/implementation/B11-result-simulation.md.

namespace at {

struct Fixture {
    uint16_t home_id = 0;
    uint16_t away_id = 0;
};

enum class ResultFidelity : uint8_t {
    ExactEngine = 0,
    Synthesised = 1,
};

struct ScorerEntry {
    uint8_t side         = 0;
    uint8_t squad_index  = 0;
    uint8_t minute       = 0;
    uint8_t _pad         = 0;
};

struct CardEntry {
    uint8_t side         = 0;
    uint8_t squad_index  = 0;
    uint8_t kind         = 0; // 1 yellow, 2 red
    uint8_t _pad         = 0;
};

struct InjuryEntry {
    uint8_t  side         = 0;
    uint8_t  squad_index  = 0;
    int16_t  level        = 0;
};

inline constexpr int kMaxResultScorers  = 16;
inline constexpr int kMaxResultCards    = 16;
inline constexpr int kMaxResultInjuries = 8;

struct MatchResult {
    uint16_t home_id = 0;
    uint16_t away_id = 0;
    std::array<uint8_t, 2> score{0, 0};
    std::array<TeamStats, 2> stats{};

    uint8_t scorer_count = 0;
    uint8_t card_count = 0;
    uint8_t injury_count = 0;
    uint8_t _pad0 = 0;
    std::array<ScorerEntry, kMaxResultScorers> scorers{};
    std::array<CardEntry, kMaxResultCards> cards{};
    std::array<InjuryEntry, kMaxResultInjuries> injuries{};

    std::array<std::array<uint8_t, kMatchSquadSize>, 2> ratings{};
    std::array<std::array<RatingBreakdown, kMatchSquadSize>, 2> breakdown{};

    ResultFidelity fidelity = ResultFidelity::Synthesised;
};

// Fill ratings/breakdown from a MatchState (engine or synthetic).
inline void ApplyRatingsToResult(MatchResult& mr, const MatchState& s) {
    const MatchRatings r = ComputeMatchRatings(s);
    for (int side = 0; side < 2; ++side) {
        for (int i = 0; i < kMatchSquadSize; ++i) {
            mr.ratings[static_cast<size_t>(side)][static_cast<size_t>(i)] =
                r.by_side[static_cast<size_t>(side)][static_cast<size_t>(i)].rating;
            mr.breakdown[static_cast<size_t>(side)][static_cast<size_t>(i)] =
                r.by_side[static_cast<size_t>(side)][static_cast<size_t>(i)]
                    .breakdown;
        }
    }
}

// Extract score/stats/events/ratings from a finished MatchState.
inline MatchResult ExtractMatchResult(const MatchState& s, uint16_t home_id,
                                      uint16_t away_id) {
    MatchResult mr{};
    mr.home_id = home_id;
    mr.away_id = away_id;
    mr.score = s.score;
    mr.stats[0] = s.sides[0].stats;
    mr.stats[1] = s.sides[1].stats;
    mr.fidelity = ResultFidelity::ExactEngine;

    for (uint8_t i = 0; i < s.chronicle.count; ++i) {
        const MatchEvent& e = s.chronicle.events[static_cast<size_t>(i)];
        const auto kind = static_cast<MatchEventKind>(e.kind);
        if (kind == MatchEventKind::Goal && mr.scorer_count < kMaxResultScorers) {
            ScorerEntry& se = mr.scorers[mr.scorer_count++];
            se.side = e.side;
            se.squad_index = e.squad_index;
            se.minute = e.minute;
        } else if ((kind == MatchEventKind::Yellow || kind == MatchEventKind::Red) &&
                   mr.card_count < kMaxResultCards) {
            CardEntry& ce = mr.cards[mr.card_count++];
            ce.side = e.side;
            ce.squad_index = e.squad_index;
            ce.kind = (kind == MatchEventKind::Red) ? uint8_t{2} : uint8_t{1};
        } else if (kind == MatchEventKind::Injury &&
                   mr.injury_count < kMaxResultInjuries) {
            InjuryEntry& ie = mr.injuries[mr.injury_count++];
            ie.side = e.side;
            ie.squad_index = e.squad_index;
            ie.level = 1;
        }
    }
    ApplyRatingsToResult(mr, s);
    return mr;
}

} // namespace at
