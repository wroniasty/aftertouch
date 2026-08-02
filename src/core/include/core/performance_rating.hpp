#pragma once
#include "core/match_state.hpp"

#include <array>
#include <cstdint>

// Post-match 1–10 ratings — doc/implementation/B12-performance-rating.md.
// Pure: never called from MatchEngine::Step.

namespace at {

// Matches data::Position numeric values (GK=0 … A=7).
enum class RatingBand : uint8_t { GK = 0, Def = 1, Mid = 2, Att = 3 };

struct RatingBreakdown {
    uint8_t  goals          = 0;
    uint8_t  yellows        = 0;
    uint8_t  reds           = 0;
    uint8_t  injured        = 0;
    uint8_t  minutes_proxy  = 0;
    uint8_t  _pad0          = 0;
    uint16_t passes_attempted = 0;
    uint16_t passes_completed = 0;
    uint16_t tackles          = 0;
    uint16_t headers          = 0;
    uint16_t carry_distance   = 0;
    uint16_t saves            = 0;
    uint16_t fouls_conceded   = 0;
    uint16_t _pad1            = 0;
};

static_assert(std::has_unique_object_representations_v<RatingBreakdown>);

struct PlayerRating {
    uint8_t         rating = 5; // 1..10
    RatingBreakdown breakdown{};
};

struct MatchRatings {
    std::array<std::array<PlayerRating, kMatchSquadSize>, 2> by_side{};
};

struct BandWeights {
    int8_t goals       = 0;
    int8_t pass_comp   = 0;
    int8_t pass_att    = 0;
    int8_t tackles     = 0;
    int8_t headers     = 0;
    int8_t carry       = 0;
    int8_t saves       = 0;
    int8_t fouls       = 0; // typically negative
    int8_t clean_sheet = 0;
};

inline RatingBand BandForPosition(uint8_t pos) {
    // data::Position: GK=0, RB=1, LB=2, D=3, RW=4, LW=5, M=6, A=7
    if (pos == 0) return RatingBand::GK;
    if (pos <= 3) return RatingBand::Def;
    if (pos <= 6) return RatingBand::Mid;
    return RatingBand::Att;
}

inline constexpr std::array<BandWeights, 4> kBandWeights{{
    // GK
    {0, 1, 0, 0, 0, 0, 2, -1, 2},
    // Def
    {1, 1, 0, 2, 2, 1, 0, -2, 0},
    // Mid
    {2, 2, 1, 1, 1, 1, 0, -1, 0},
    // Att
    {3, 1, 0, 0, 1, 2, 0, -1, 0},
}};

inline PlayerRating ComputePlayerRating(const MatchState& s, int side,
                                        int squad_index) {
    PlayerRating out{};
    if (side < 0 || side > 1 || squad_index < 0 ||
        squad_index >= kMatchSquadSize)
        return out;

    const SquadPlayer& sp =
        s.sides[static_cast<size_t>(side)].squad[static_cast<size_t>(squad_index)];
    const PlayerMatchStats& ms =
        s.sides[static_cast<size_t>(side)].match_stats[static_cast<size_t>(squad_index)];

    for (uint8_t i = 0; i < s.chronicle.count; ++i) {
        const MatchEvent& e = s.chronicle.events[static_cast<size_t>(i)];
        if (e.side != static_cast<uint8_t>(side) ||
            e.squad_index != static_cast<uint8_t>(squad_index))
            continue;
        switch (static_cast<MatchEventKind>(e.kind)) {
        case MatchEventKind::Goal:
            ++out.breakdown.goals;
            break;
        case MatchEventKind::Yellow:
            ++out.breakdown.yellows;
            break;
        case MatchEventKind::Red:
            ++out.breakdown.reds;
            break;
        case MatchEventKind::Injury:
            out.breakdown.injured = 1;
            break;
        default:
            break;
        }
    }
    if (sp.goals_scored > out.breakdown.goals)
        out.breakdown.goals = sp.goals_scored;
    if (sp.is_injured) out.breakdown.injured = 1;
    if (sp.cards >= 2 && out.breakdown.reds == 0) out.breakdown.reds = 1;
    else if (sp.cards == 1 && out.breakdown.yellows == 0)
        out.breakdown.yellows = 1;

    out.breakdown.passes_attempted = ms.passes_attempted;
    out.breakdown.passes_completed = ms.passes_completed;
    out.breakdown.tackles          = ms.tackles;
    out.breakdown.headers          = ms.headers;
    out.breakdown.carry_distance   = ms.carry_distance;
    out.breakdown.saves            = ms.saves;
    out.breakdown.fouls_conceded   = ms.fouls_conceded;

    const bool played = sp.half_played != 0 || squad_index < 11;
    out.breakdown.minutes_proxy = played ? uint8_t{90} : uint8_t{0};

    const BandWeights& W =
        kBandWeights[static_cast<size_t>(BandForPosition(sp.position))];

    int score = played ? 5 : 4;
    int raw = 0;
    raw += static_cast<int>(out.breakdown.goals) * W.goals;
    raw += (static_cast<int>(out.breakdown.passes_completed) * W.pass_comp) / 3;
    raw += (static_cast<int>(out.breakdown.passes_attempted) * W.pass_att) / 8;
    raw += static_cast<int>(out.breakdown.tackles) * W.tackles;
    raw += static_cast<int>(out.breakdown.headers) * W.headers;
    raw += (static_cast<int>(out.breakdown.carry_distance) * W.carry) / 200;
    raw += static_cast<int>(out.breakdown.saves) * W.saves;
    raw += static_cast<int>(out.breakdown.fouls_conceded) * W.fouls;

    if (BandForPosition(sp.position) == RatingBand::GK && played) {
        const uint8_t conceded = s.score[static_cast<size_t>(1 - side)];
        if (conceded == 0) raw += W.clean_sheet;
    }

    // Soft saturate so huge raw doesn't explode past ±5 before clamp.
    if (raw > 5) raw = 5;
    if (raw < -4) raw = -4;
    score += raw;
    score -= static_cast<int>(out.breakdown.yellows);
    score -= static_cast<int>(out.breakdown.reds) * 3;
    if (out.breakdown.injured) score -= 1;

    if (score < 1) score = 1;
    if (score > 10) score = 10;
    out.rating = static_cast<uint8_t>(score);
    return out;
}

inline MatchRatings ComputeMatchRatings(const MatchState& s) {
    MatchRatings mr{};
    for (int side = 0; side < 2; ++side)
        for (int i = 0; i < kMatchSquadSize; ++i)
            mr.by_side[static_cast<size_t>(side)][static_cast<size_t>(i)] =
                ComputePlayerRating(s, side, i);
    return mr;
}

} // namespace at
