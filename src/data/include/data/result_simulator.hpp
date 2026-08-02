#pragma once
#include "core/match_engine.hpp"
#include "core/match_result.hpp"
#include "core/movement.hpp"
#include "core/performance_rating.hpp"
#include "core/rng.hpp"
#include "data/game_data.hpp"

#include <array>
#include <cstdint>

// B11 result simulators — doc/implementation/B11-result-simulation.md.
// Lives in data/ because Resolve needs League + ApplyKickoff.

namespace at::data {

struct IResultSimulator {
    virtual ~IResultSimulator() = default;
    virtual MatchResult Resolve(const League& league, Fixture fixture,
                                uint32_t seed) const = 0;
};

// ---------------------------------------------------------------------------
// Shared helpers
// ---------------------------------------------------------------------------

inline int TeamStrengthIndex(const TeamRecord& team) {
    int sum = 0;
    int n = 0;
    for (size_t i = 1; i < 11; ++i) {
        const auto& a = team.players[i].attrs;
        sum += static_cast<int>(a.finishing) + static_cast<int>(a.passing) +
               static_cast<int>(a.speed);
        ++n;
    }
    if (n == 0) return 0;
    const int mean = sum / (n * 3); // 0..15
    int idx = mean / 3;             // 0..5
    if (idx > 5) idx = 5;
    if (idx < 0) idx = 0;
    return idx;
}

// Rows = attack strength 0..5, cols = sample nibble 0..7 → goals 0..4.
inline constexpr std::array<std::array<uint8_t, 8>, 6> kGoalCountTable{{
    {{0, 0, 0, 1, 0, 1, 0, 1}},
    {{0, 1, 0, 1, 1, 1, 0, 2}},
    {{1, 1, 1, 2, 1, 2, 1, 2}},
    {{1, 2, 1, 2, 2, 2, 1, 3}},
    {{2, 2, 2, 3, 2, 3, 2, 3}},
    {{2, 3, 2, 3, 3, 4, 2, 4}},
}};

// Position weights for scorer pick (GK..A index into squad 0..10). Higher = likelier.
inline constexpr std::array<uint8_t, 11> kScorerWeights{
    0, 1, 1, 2, 3, 3, 4, 5, 6, 8, 8};

inline uint8_t PickScorerSquadIndex(RngStream& rng) {
    int total = 0;
    for (uint8_t w : kScorerWeights) total += w;
    int roll = static_cast<int>(rng.Draw()) % (total > 0 ? total : 1);
    for (uint8_t i = 0; i < 11; ++i) {
        roll -= kScorerWeights[static_cast<size_t>(i)];
        if (roll < 0) return i;
    }
    return 9;
}

inline void MarkStartXi(MatchState& s) {
    for (int side = 0; side < 2; ++side)
        for (int i = 0; i < 11; ++i)
            s.sides[static_cast<size_t>(side)].squad[static_cast<size_t>(i)]
                .half_played = 1;
}

// Default XI positions when league sheet is unavailable.
inline constexpr std::array<uint8_t, 11> kDefaultXiPositions{
    0, 1, 2, 3, 3, 4, 5, 6, 6, 7, 7}; // GK RB LB D D RW LW M M A A

inline void FillDefaultPositions(MatchState& s) {
    for (int side = 0; side < 2; ++side)
        for (int i = 0; i < 11; ++i)
            s.sides[static_cast<size_t>(side)].squad[static_cast<size_t>(i)].position =
                kDefaultXiPositions[static_cast<size_t>(i)];
}

inline void CopyPositionsFromTeams(MatchState& s, const TeamRecord& home,
                                   const TeamRecord& away) {
    const TeamRecord* teams[2] = {&home, &away};
    for (int side = 0; side < 2; ++side)
        for (int i = 0; i < 11; ++i)
            s.sides[static_cast<size_t>(side)].squad[static_cast<size_t>(i)].position =
                teams[side]->players[static_cast<size_t>(i)].position;
}

inline void SynthesizePlayerMatchStats(MatchState& s, RngStream& rng) {
    for (int side = 0; side < 2; ++side) {
        const uint8_t conceded = s.score[static_cast<size_t>(1 - side)];
        for (int i = 0; i < 11; ++i) {
            PlayerMatchStats& ms =
                s.sides[static_cast<size_t>(side)].match_stats[static_cast<size_t>(i)];
            const auto band =
                BandForPosition(s.sides[static_cast<size_t>(side)]
                                    .squad[static_cast<size_t>(i)]
                                    .position);
            const uint8_t r = rng.Draw();
            switch (band) {
            case RatingBand::GK:
                ms.saves = static_cast<uint16_t>((conceded == 0 ? 1 : 0) + (r & 1));
                ms.passes_attempted = static_cast<uint16_t>(1 + (r & 1));
                ms.passes_completed = ms.passes_attempted;
                break;
            case RatingBand::Def:
                ms.tackles = static_cast<uint16_t>(1 + (r & 1));
                ms.headers = static_cast<uint16_t>(r & 1);
                ms.fouls_conceded = static_cast<uint16_t>(r & 1);
                ms.passes_attempted = static_cast<uint16_t>(2 + (r & 3));
                ms.passes_completed = static_cast<uint16_t>(ms.passes_attempted / 2);
                ms.carry_distance = static_cast<uint16_t>(20 + (r & 31));
                break;
            case RatingBand::Mid:
                ms.passes_attempted = static_cast<uint16_t>(4 + (r & 7));
                ms.passes_completed = static_cast<uint16_t>(2 + (r & 3));
                ms.tackles = static_cast<uint16_t>(r & 1);
                ms.headers = static_cast<uint16_t>(r & 1);
                ms.carry_distance = static_cast<uint16_t>(40 + (r & 63));
                ms.fouls_conceded = static_cast<uint16_t>((r >> 2) & 1);
                break;
            case RatingBand::Att:
                ms.passes_attempted = static_cast<uint16_t>(1 + (r & 3));
                ms.passes_completed = static_cast<uint16_t>(r & 1);
                ms.carry_distance = static_cast<uint16_t>(60 + (r & 63));
                ms.headers = static_cast<uint16_t>(r & 1);
                ms.fouls_conceded = static_cast<uint16_t>((r >> 3) & 1);
                break;
            }
        }
    }
}

// Build a MatchState sheet with score/chronicle/team stats for B12 (no physics).
// Caller sets positions (default or league) then SynthesizePlayerMatchStats.
inline MatchState BuildSyntheticStateForRatings(const MatchResult& partial) {
    MatchState s{};
    s.score = partial.score;
    s.sides[0].stats = partial.stats[0];
    s.sides[1].stats = partial.stats[1];
    MarkStartXi(s);
    FillDefaultPositions(s);
    for (uint8_t i = 0; i < partial.scorer_count; ++i) {
        const ScorerEntry& se = partial.scorers[static_cast<size_t>(i)];
        AppendChronicle(s, MatchEventKind::Goal, se.side, se.squad_index);
        if (se.side < 2 && se.squad_index < kMatchSquadSize)
            ++s.sides[static_cast<size_t>(se.side)]
                  .squad[static_cast<size_t>(se.squad_index)]
                  .goals_scored;
    }
    for (uint8_t i = 0; i < partial.card_count; ++i) {
        const CardEntry& ce = partial.cards[static_cast<size_t>(i)];
        AppendChronicle(s,
                        ce.kind >= 2 ? MatchEventKind::Red : MatchEventKind::Yellow,
                        ce.side, ce.squad_index);
    }
    for (uint8_t i = 0; i < partial.injury_count; ++i) {
        const InjuryEntry& ie = partial.injuries[static_cast<size_t>(i)];
        AppendChronicle(s, MatchEventKind::Injury, ie.side, ie.squad_index);
        if (ie.side < 2 && ie.squad_index < kMatchSquadSize)
            s.sides[static_cast<size_t>(ie.side)]
                .squad[static_cast<size_t>(ie.squad_index)]
                .is_injured = 1;
    }
    return s;
}

inline void SynthesizeTeamStats(MatchResult& mr, RngStream& rng) {
    for (int side = 0; side < 2; ++side) {
        TeamStats& st = mr.stats[static_cast<size_t>(side)];
        const uint8_t goals = mr.score[static_cast<size_t>(side)];
        st.goal_attempts = static_cast<uint32_t>(goals + (rng.Draw() & 3));
        st.on_target =
            static_cast<uint32_t>(goals + ((rng.Draw() & 1) ? 0 : 1));
        if (st.on_target < goals) st.on_target = goals;
        if (st.goal_attempts < st.on_target) st.goal_attempts = st.on_target;
        st.corners_won = rng.Draw() & 7;
        st.fouls_conceded = 5 + (rng.Draw() & 15);
        st.bookings = mr.card_count > 0 ? 1u : 0u;
        st.possession = 1000 + static_cast<uint32_t>(rng.Draw()) * 4u;
    }
}

inline void FillScorersForScore(MatchResult& mr, RngStream& rng) {
    mr.scorer_count = 0;
    for (int side = 0; side < 2; ++side) {
        const int goals = mr.score[static_cast<size_t>(side)];
        for (int g = 0; g < goals && mr.scorer_count < kMaxResultScorers; ++g) {
            ScorerEntry& se = mr.scorers[mr.scorer_count++];
            se.side = static_cast<uint8_t>(side);
            // ~3% unattributed → pin on squad 10.
            if (rng.Draw() >= 248)
                se.squad_index = 10;
            else
                se.squad_index = PickScorerSquadIndex(rng);
            se.minute = static_cast<uint8_t>(1 + (rng.Draw() % 90));
        }
    }
}

// ---------------------------------------------------------------------------
// Scripted
// ---------------------------------------------------------------------------

struct ScriptedScore {
    uint16_t home_id = 0;
    uint16_t away_id = 0;
    uint8_t  home_goals = 0;
    uint8_t  away_goals = 0;
};

struct ScriptedResultSimulator : IResultSimulator {
    std::array<ScriptedScore, 64> table{};
    uint8_t table_count = 0;

    void Add(uint16_t home, uint16_t away, uint8_t hg, uint8_t ag) {
        if (table_count >= table.size()) return;
        table[table_count++] = ScriptedScore{home, away, hg, ag};
    }

    MatchResult Resolve(const League& league, Fixture fixture,
                        uint32_t seed) const override {
        (void)league;
        MatchResult mr{};
        mr.home_id = fixture.home_id;
        mr.away_id = fixture.away_id;
        mr.fidelity = ResultFidelity::Synthesised;
        for (uint8_t i = 0; i < table_count; ++i) {
            if (table[i].home_id == fixture.home_id &&
                table[i].away_id == fixture.away_id) {
                mr.score[0] = table[i].home_goals;
                mr.score[1] = table[i].away_goals;
                break;
            }
        }
        RngStream rng;
        rng.Seed(seed ^ 0xB1100001u);
        FillScorersForScore(mr, rng);
        SynthesizeTeamStats(mr, rng);
        MatchState synth = BuildSyntheticStateForRatings(mr);
        SynthesizePlayerMatchStats(synth, rng);
        ApplyRatingsToResult(mr, synth);
        return mr;
    }
};

// ---------------------------------------------------------------------------
// Table
// ---------------------------------------------------------------------------

struct TableResultSimulator : IResultSimulator {
    MatchResult Resolve(const League& league, Fixture fixture,
                        uint32_t seed) const override {
        MatchResult mr{};
        mr.home_id = fixture.home_id;
        mr.away_id = fixture.away_id;
        mr.fidelity = ResultFidelity::Synthesised;

        const TeamRecord* home = FindTeam(league, fixture.home_id);
        const TeamRecord* away = FindTeam(league, fixture.away_id);
        if (!home || !away) return mr;

        RngStream rng;
        rng.Seed(seed ^ 0xB1100002u);

        int sh = TeamStrengthIndex(*home) + 1; // home advantage
        int sa = TeamStrengthIndex(*away);
        if (sh > 5) sh = 5;
        if (sa > 5) sa = 5;

        const uint8_t ncol = static_cast<uint8_t>(rng.Draw() & 7);
        mr.score[0] = kGoalCountTable[static_cast<size_t>(sh)][ncol];
        const uint8_t ncol2 = static_cast<uint8_t>(rng.Draw() & 7);
        // Away uses inverted strength row slightly softer.
        int away_row = sa;
        if (away_row > 0 && (rng.Draw() & 3) == 0) --away_row;
        mr.score[1] = kGoalCountTable[static_cast<size_t>(away_row)][ncol2];

        FillScorersForScore(mr, rng);
        SynthesizeTeamStats(mr, rng);
        // Light card chance.
        if ((rng.Draw() & 7) == 0 && mr.card_count < kMaxResultCards) {
            CardEntry& ce = mr.cards[mr.card_count++];
            ce.side = rng.Draw() & 1;
            ce.squad_index = static_cast<uint8_t>(1 + (rng.Draw() % 10));
            ce.kind = 1;
        }

        MatchState synth = BuildSyntheticStateForRatings(mr);
        CopyPositionsFromTeams(synth, *home, *away);
        SynthesizePlayerMatchStats(synth, rng);
        ApplyRatingsToResult(mr, synth);
        return mr;
    }
};

// ---------------------------------------------------------------------------
// Engine
// ---------------------------------------------------------------------------

struct EngineResultSimulator : IResultSimulator {
    uint32_t tick_cap = 20000;

    MatchResult Resolve(const League& league, Fixture fixture,
                        uint32_t seed) const override {
        MatchResult empty{};
        empty.home_id = fixture.home_id;
        empty.away_id = fixture.away_id;
        empty.fidelity = ResultFidelity::ExactEngine;

        MatchEngine eng;
        eng.Reset(seed);
        const RngStream g = eng.State().gameplay_rng;
        const RngStream p = eng.State().presentation_rng;
        const RngStream r = eng.State().resolve_rng;

        MatchState sheet{};
        if (!ApplyKickoff(league, fixture.home_id, fixture.away_id, sheet))
            return empty;

        sheet.gameplay_rng     = g;
        sheet.presentation_rng = p;
        sheet.resolve_rng      = r;
        sheet.clock.game_length = 0;
        sheet.clock.match_started = 1;
        sheet.clock.stoppage_event_timer = 2;
        SetGameState(sheet, GameState::StartingGame);
        SetPl(sheet, GameStatePl::Stopped);
        sheet.phase = MatchPhase::KickOff;
        sheet.sides[0].control.player_number = 0;
        sheet.sides[1].control.player_number = 0;
        PlacePlayersAtKickoff(sheet);
        eng.LoadState(sheet);

        uint32_t steps = 0;
        while (eng.State().phase != MatchPhase::FullTime && steps < tick_cap) {
            eng.Step(MatchInput{});
            ++steps;
        }

        MatchResult mr =
            ExtractMatchResult(eng.State(), fixture.home_id, fixture.away_id);
        mr.fidelity = ResultFidelity::ExactEngine;
        return mr;
    }
};

} // namespace at::data
