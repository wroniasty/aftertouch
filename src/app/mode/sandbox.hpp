#pragma once
#include "core/match_engine.hpp"
#include "core/match_state.hpp"

#include <cstdint>

// Sandbox match mode — doc/implementation/C1b-sandbox-mode.md.
//
// N configurable outfield players against a lone opposing goalkeeper, for
// testing play feel and engine behaviour in isolation. Pure: no SDL, no ImGui,
// no file I/O — the dialog fills a SandboxConfig, this builds the MatchState,
// and the engine never learns it is running a sandbox.

namespace at::mode {

inline constexpr int kSandboxMaxOutfield = 10;

struct SandboxPlayer {
    PlayerAttrs attrs{};
    // 0–7 like every attribute (B13 / R2). This used to default to 8, which
    // GkSkillIndex silently clamped to 7 — same behaviour, dishonest number.
    uint8_t     goalie_skill = at::kAttrMax;
};

struct SandboxConfig {
    uint8_t  outfield_count = 3;      // 1..kSandboxMaxOutfield
    bool     own_keeper     = false;  // give the test side its own keeper
    bool     attack_down    = true;   // test side attacks high y (bottom goal)
    // Which tactic roles the spawned players take. Attacking roles keep a small
    // side upfield; defensive roles are for testing a back line.
    bool     spawn_as_attackers = true;
    uint32_t seed           = 0xC1B00001u;
    uint8_t  game_length    = 0;      // MatchClock semantics, 0..3
    bool     reset_at_half_time = true;

    SandboxPlayer field[kSandboxMaxOutfield]{}; // in spawn order, not by role
    SandboxPlayer keeper{};                     // test side, if own_keeper
    SandboxPlayer opponent_keeper{};
};

// Pitch slot of the k-th spawned outfield player (0-based), or -1 if k is
// beyond outfield_count. Shared by the builder, the dialog and the tests.
int SandboxFieldSlot(const SandboxConfig& cfg, int k);

// Attributes every slider starts from: competent, not superhuman.
SandboxConfig DefaultSandboxConfig();

// Projects the config into a kickoff-ready MatchState. RNG streams are left
// untouched — the caller owns seeding policy (B1), as SeedPlayableMatch does.
void BuildSandboxState(const SandboxConfig& cfg, MatchState& out);

// Seed, build, load. The whole mode start, so main.cpp and the tests agree.
void StartSandbox(MatchEngine& engine, const SandboxConfig& cfg);

} // namespace at::mode
