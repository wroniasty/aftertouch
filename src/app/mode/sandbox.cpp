#include "mode/sandbox.hpp"

#include "core/match_clock.hpp"
#include "core/set_pieces.hpp"
#include "data/fictional.hpp"
#include "data/game_data.hpp"

#include <cstdio>
#include <cstring>

namespace at::mode {

namespace {

constexpr uint16_t kTestTeamId = 1;      // Northbridge FC — kit + tactics donor
constexpr uint16_t kOppTeamId  = 2;      // Port Meridian

int ClampOutfield(uint8_t n) {
    if (n < 1) return 1;
    if (n > kSandboxMaxOutfield) return kSandboxMaxOutfield;
    return static_cast<int>(n);
}

void SetChars(char* dst, size_t cap, const char* src) {
    std::memset(dst, 0, cap);
    for (size_t i = 0; i + 1 < cap && src[i] != '\0'; ++i) dst[i] = src[i];
}

void WriteSquadEntry(SquadPlayer& sp, int index, const SandboxPlayer& p,
                     const char* name, uint8_t position) {
    sp.index        = static_cast<uint8_t>(index);
    sp.shirt_number = static_cast<uint8_t>(index + 1);
    sp.position     = position;
    sp.attrs        = p.attrs;
    sp.goalie_skill = p.goalie_skill;
    SetChars(sp.full_name.data(), kMatchNameLen, name);
    SetChars(sp.short_name.data(), 15, name);
}

// Sheet, kits and tactics come from the fictional league so colours and the
// off-ball grid are real data; only the names are ours.
void ApplySheet(MatchSide& ms, const data::League& league, uint16_t team_id,
                const char* display_name) {
    const data::TeamRecord* team = data::FindTeam(league, team_id);
    if (!team) return;
    SetChars(ms.sheet.name.data(), kMatchNameLen, display_name);
    ms.sheet.tactics_id = team->tactics_id;
    ms.sheet.primary    = data::detail::ToKitSpec(team->primary);
    ms.sheet.secondary  = data::detail::ToKitSpec(team->secondary);
    if (const data::TacticRecord* tac = data::FindTactic(league, team->tactics_id)) {
        ms.tactics.out_of_play = tac->out_of_play;
        ms.tactics.cells       = tac->cells;
    }
}

} // namespace

int SandboxFieldSlot(const SandboxConfig& cfg, int k) {
    const int n = ClampOutfield(cfg.outfield_count);
    if (k < 0 || k >= n) return -1;
    // Ordinal 1 is the keeper; outfield ordinals run 2..11.
    const int first_ordinal = cfg.spawn_as_attackers ? (12 - n) : 2;
    return (first_ordinal + k) - 1; // side 0 base is slot 0
}

SandboxConfig DefaultSandboxConfig() {
    SandboxConfig cfg;
    // B13 / R2: this was {8,…} on the old 0–15 reading, which every consumer
    // clamped to 7 — so "mid" silently meant "max". On the real 0–7 range the
    // midpoint is 4, and a default sandbox player is now genuinely average.
    constexpr uint8_t m = (kAttrMax + 1) / 2;
    const PlayerAttrs mid{m, m, m, m, m, m, m, 0};
    for (int i = 0; i < kSandboxMaxOutfield; ++i) {
        cfg.field[i].attrs = mid;
        cfg.field[i].goalie_skill = m;
    }
    cfg.keeper.attrs = mid;
    cfg.opponent_keeper.attrs = mid;
    return cfg;
}

void BuildSandboxState(const SandboxConfig& cfg, MatchState& out) {
    const int n = ClampOutfield(cfg.outfield_count);
    const data::League league = data::MakeFictionalLeague();

    out = MatchState{};
    out.phase = MatchPhase::KickOff;
    out.referee.team_number = 3;
    out.booked_indicator.team_number = 3;

    ApplySheet(out.sides[0], league, kTestTeamId, "TEST XI");
    ApplySheet(out.sides[1], league, kOppTeamId, "KEEPER ONLY");

    // --- squads -------------------------------------------------------------
    char name[kMatchNameLen];
    WriteSquadEntry(out.sides[0].squad[0], 0, cfg.keeper, "TEST KEEPER",
                    static_cast<uint8_t>(data::Position::GK));
    for (int k = 0; k < n; ++k) {
        const int slot = SandboxFieldSlot(cfg, k);
        std::snprintf(name, sizeof name, "TEST %d", k + 1);
        WriteSquadEntry(out.sides[0].squad[static_cast<size_t>(slot)], slot,
                        cfg.field[k],
                        name, static_cast<uint8_t>(data::Position::M));
    }
    WriteSquadEntry(out.sides[1].squad[0], 0, cfg.opponent_keeper, "OPP KEEPER",
                    static_cast<uint8_t>(data::Position::GK));

    // --- pitch entities -----------------------------------------------------
    for (int side = 0; side < 2; ++side) {
        out.sides[static_cast<size_t>(side)].control.team_number =
            static_cast<uint8_t>(side + 1);
        for (int i = 0; i < 11; ++i) {
            const int slot = side * 11 + i;
            Entity& e = out.players[static_cast<size_t>(slot)];
            e.team_number      = static_cast<int16_t>(side + 1);
            e.player_ordinal   = static_cast<int16_t>(i + 1);
            e.player_direction = 0;

            bool present = false;
            if (side == 0) {
                if (i == 0) present = cfg.own_keeper;
                else
                    for (int k = 0; k < n; ++k)
                        if (SandboxFieldSlot(cfg, k) == slot) present = true;
            } else {
                present = (i == 0); // the lone opposing keeper
            }
            if (!present) ParkOffPitch(e);
        }
    }

    // --- bootstrap ----------------------------------------------------------
    // BeginMatchIfNeeded rolls the direction of play from the RNG and only runs
    // while match_started == 0, so the mode does its own kickoff and hands the
    // engine an already-started match with the chosen ends.
    out.clock.match_started = 1;
    out.clock.game_length   = static_cast<uint8_t>(cfg.game_length & 3u);
    // team_playing_up defends the top goal, so it attacks high y.
    out.globals.team_playing_up = cfg.attack_down ? uint8_t{1} : uint8_t{2};
    out.globals.team_starting   = 1;
    out.surface = MatchSurface{};
    PlaceBallAtCentre(out);
    SetGameState(out, GameState::StartingGame);
    SetPl(out, GameStatePl::Stopped);
    out.clock.stoppage_event_timer = 2;

    PlacePlayersAtKickoff(out); // fills a slot < 0, so control comes after

    // --- control ------------------------------------------------------------
    out.sides[0].control.player_number = 1; // human
    out.sides[0].control.controlled_slot =
        static_cast<int8_t>(SandboxFieldSlot(cfg, 0));
    out.sides[1].control.player_number = 0; // CPU
    // No eligible field player: leaving the slot unset keeps slot 11 on
    // ApplyGoalkeeperAI instead of the outfield brain (C1b §2.2).
    out.sides[1].control.controlled_slot = -1;
}

void StartSandbox(MatchEngine& engine, const SandboxConfig& cfg) {
    engine.Reset(cfg.seed);
    MatchState s{};
    BuildSandboxState(cfg, s);
    s.gameplay_rng     = engine.State().gameplay_rng;
    s.presentation_rng = engine.State().presentation_rng;
    s.resolve_rng      = engine.State().resolve_rng;
    engine.LoadState(s);
}

} // namespace at::mode
