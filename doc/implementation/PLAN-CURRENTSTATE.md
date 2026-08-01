# PLAN — Current state

Snapshot of foundation (A) and match-engine Wave 2 against [PLAN.md](PLAN.md).  
Statuses: **done** · **partial** · **not started**.  
Updated after B3 closed locally; regenerate this file when a part's acceptance moves.

---

## Wave position

| Wave | Gate | Status |
|---|---|---|
| **0 — Skeleton** | A1; PLAN.md §8 on Windows and macOS | **Passed** |
| **1 — Instrument** | A2, A3, A4, A6 green; reference + engine traces in viewer | **Open** — A2/A3/A6 green; A4 first-launch prompt still open |
| **2 — The ball moves** | B1, B2, B3, B4, B10 (+ thin C1 slice) | **In progress** — B1–B3 done |
| **3+** | rest of B / C / D / E | Not started |

A5 was scheduled for Wave 3 but is implemented early; B9 can consume tactics data when it arrives. Wave 2 continues with B4 while the A4 residual remains D1-shaped.

---

## Part A summary

| ID | Part | Status | Acceptance |
|---|---|---|---|
| **A1** | Kernel | **done** | Build, walls, skeleton — PLAN.md §8 |
| **A2** | Determinism primitives | **done** | Fix, Amiga kernel, RNG, HashState; committed hash gate |
| **A3** | Trace harness | **done** | Format, differ+drift, corpus, viewer; stub-oracle refs until SWOS recorder runs |
| **A4** | Asset pipeline | **partial** | Format + `assetc` + `IAssetSource`/placeholder/imported; first-launch prompt still open |
| **A5** | Game data | **done** | Fictional league → `MatchState`; `ATGD` round-trip |
| **A6** | Test infrastructure | **done** | `ctest` suite, golden wedge, CI workflow, core tests sans SDL |

---

## Part B summary

| ID | Part | Status | Acceptance |
|---|---|---|---|
| **B1** | State & layout | **done** | Full match state serialises to ATTR and back losslessly |
| **B2** | Frame & state machine | **done** | 90′ headless ends FullTime at 0–0 |
| **B3** | Ball physics | **done** | Scripted trajectory hash + OOP wire green |
| **B4+** | Movement / … | **not started** | — |

---

## B3 — Ball physics — **done**

Subfile: [B3-ball-physics.md](B3-ball-physics.md)

| Work item | State |
|---|---|
| Amiga ground/air/gravity constants behind profile | landed |
| `MatchSurface` (Normal defaults); ATTR format version **4** | landed |
| `UpdateBall`: deltas → friction → integrate → bounce → barrier → frame → predictor | landed |
| A2 controls ball walk removed; `LaunchBall` helper | landed |
| `ClassifyBallOutOfPlay` wired on leave-play | landed |
| Golden friction / bounce / barrier / predictor / trajectory / OOP-wire tests | landed |
| Golden / corpus / determinism re-pinned | landed |

Done-when met: `test_ball_trajectory` + `test_ball_oop_wire` green in `core_tests`. Real SWOS ATTR remains an A3 follow-up. Kick launch is B6.

---

## B2 — Frame & state machine — **done**

Subfile: [B2-frame-state-machine.md](B2-frame-state-machine.md)

| Work item | State |
|---|---|
| `GameState` / `GameStatePl` enums; `MatchClock`; `TeamStats` | landed |
| Amiga-profile clock (refill 49, `game_length` 0 → 8820 ticks / 90′) | landed |
| Kick-off → InPlay → HalfTime → InPlay → FullTime | landed |
| Injury-time gate (`prolongLastMinute`) | landed |
| Step spine: time → controls stub → ball → stats | landed (B3 filled ball) |
| Pure `ClassifyBallOutOfPlay` (wired by B3) | landed |
| ATTR format version **3** (superseded by v4 in B3) | landed |
| `test_full_match` 0–0 acceptance | landed |

Done-when met: headless `game_length=0` run reaches `MatchPhase::FullTime` with score `{0,0}` in `core_tests`.

---

## B1 — State & layout — **done**

Subfile: [B1-state-layout.md](B1-state-layout.md)

| Work item | State |
|---|---|
| `Entity` / `TeamControl` / `SquadPlayer` / `TacticsSnapshot` / `MatchGlobals` / `MatchSide` | landed |
| Fixed arena: ball, 22 players, referee, booked indicator; pointer-free slots | landed |
| Accessors (`Ball`, `Player`, `Side`, `Controlled`, …) | landed |
| Named STATE.md fields only; unnamed / `unknownTail` omitted | landed |
| `HashState` over new layout (`presentation_rng` excluded) | landed |
| `ApplyKickoff` → 16×2 squad + tactics cells + pitch identity | landed |
| Season runner applies projected sheet via `LoadState` | landed |
| ATTR full-state field-by-field record (superseded by v3 in B2) | landed |
| Round-trip + unique-rep tests (`test_match_state`, `test_trace`) | landed |
| Golden / corpus / determinism hash re-pinned | landed |

Done-when met: busy `MatchState` round-trips through ATTR with `memcmp` equality in `core_tests`.

---

## A1 — Kernel — **done**

CMake presets (win/mac), `at_core` / `at_app` skeleton, SDL3 + ImGui + doctest vendored, `tools/check_walls.py` on the default build. No subfile (by plan).

---

## A2 — Determinism primitives — **done**

Subfile: [A2-determinism-primitives.md](A2-determinism-primitives.md)

| Work item | State |
|---|---|
| `Fix` pinned + golden vectors (`test_fixed.cpp`) | landed |
| Wall check: no float / clock / unseeded rand in `src/core/` | landed |
| Angle model, trig tables, `CalculateDeltaXAndY`, Amiga profile | landed |
| Three-stream RNG in state (`kRandomTable` = own permutation, §6.1) | landed |
| `HashState` + padding-free `MatchState` (presentation RNG excluded) | landed |
| Cross-platform committed hash gate (`test_determinism.cpp`) | landed |

---

## A3 — Trace harness — **done**

Subfile: [A3-trace-harness.md](A3-trace-harness.md)

| Work item | State |
|---|---|
| `core/trace.hpp` — header + per-tick record, LE, hashed payload | landed (now ATTR **v3** / B2 clock+stats) |
| Round-trip / corruption / endian tests | landed |
| `tracediff` — zero tolerance, field class, drift profile (`--drift`) | landed (Side/Globals/Rng classes) |
| `tracegen` — scenarios, `.atin` I/O, `--stub-oracle` | landed |
| Reference patch + `apply.py` + ATTR overlay recorder | landed (apply locally; not auto-run against sibling checkout) |
| Corpus: `kickoff` + `shot_curl` inputs, ATTR pair, hash chains | landed (reference = stub oracle; refreshed for v2) |
| `trace_viewer` — overlay, scrub, jump-to-divergence | landed |
| `HIL2` reader + minimal fixture test | landed |

Done-when met with Wave-1 honesty: engine vs stub-oracle `kickoff` loads in the viewer and reports divergence at tick 1. Real SWOS_TEST/Amiga ATTR files replace the stub when `tools/reference/apply.py` + a match harness are used; chains refresh via `gen-corpus` / `record_corpus.py`.

---

## A4 — Asset pipeline — **partial**

Subfile: [A4-asset-pipeline.md](A4-asset-pipeline.md)

| Work item | State |
|---|---|
| `ATAP` container + validate (`assets/asset_pack.hpp`) | landed |
| `assetc` — original / ref-tree import into `assets/generated/` | landed (in tree) |
| Format tests (`asset_tests`) | landed |
| `IAssetSource` / `PlaceholderAssets` / `ImportedAssets` | landed (`at_asset_source`) |
| Placeholder generator + parity tests | landed (`tools/gen_placeholder.py`, `assets/placeholder/`, `test_placeholder_parity.cpp`) |
| First-launch import prompt (shell screen) | **not landed** (D1-shaped) |
| Write-location guard / fuzz of decode | partial (A6 fuzz hits `Validate`; guard in `assetc`) |

`OpenAssetSource(generated, placeholder)` prefers import when `slotA_blk0`/`ball`/`pitch1` are present; otherwise placeholder. App logs which path loaded. Real `assetc` output still lacks `ball.atp` until that bank is written — clean clones use placeholder.

---

## A5 — Game data — **done**

Subfile: [A5-game-data.md](A5-game-data.md)

| Deliverable | State |
|---|---|
| Own `ATGD` schema (league + career snapshot by team id) | landed |
| Types: player / team / tactic (10×35 grid), attrs **0–15** | landed |
| `MakeFictionalLeague()` — 8 teams, 3 tactics | landed |
| `ApplyKickoff` → `MatchState` projection (sheets, 16-player squads, tactics snapshot, kits) | landed (extended by B1) |
| Round-trip + kickoff + malformation tests (`data_tests`) | landed |
| SWOS `team.*` / `.tac` runtime importer | deferred (optional later) |

Acceptance met. SQLite career persistence remains D2; rich career fields remain E1.

---

## A6 — Test infrastructure — **done**

Subfile: [A6-test-infrastructure.md](A6-test-infrastructure.md)

| Deliverable | State |
|---|---|
| `at_tracekit` + `tracegen` / `tracediff` | landed |
| Golden `tests/golden/kickoff.attr` + regen target `gen-golden` | landed |
| Physics-wedge test (one-raw-unit ball bump fails golden) | landed |
| `NullUiBackend`, season runner, offscreen RGBA hash | landed |
| Fuzz mutation suites for `ATAP` / `ATGD` | landed |
| CMake guard: `core_tests` must not link SDL | landed |
| `.github/workflows/ci.yml` — Windows + macOS | landed |

Acceptance met locally (`ctest` seven suites including `corpus_python`). CI green depends on the workflow running on the remote; not verified in this snapshot.

**Test binaries:** `core_tests`, `asset_tests`, `data_tests`, `harness_tests`, `fuzz_tests`, `wall_check_tests`, plus `corpus_python`.

---

## What Part A + B1 have unlocked

- Walls and CI discipline for every later part  
- Bit-identical arithmetic layer (Fix, kernel, RNG, HashState)  
- Trace *format* v2 covering full match state, differ with drift, corpus, viewer  
- Fixed arena + accessor discipline for B2–B10  
- Import path for art bytes and a playable fictional league projected into the arena  
- Headless hooks for D (null UI) and E/B11 (season runner with `LoadState`)

## What still blocks Wave 1 close

1. **A4** — first-launch import prompt (and optionally `ball.atp` from `assetc`) so the §3 done-when is fully met end-to-end  

Runtime serving is interchangeable; the shell prompt and a complete generated pack set are what remain. Optional follow-up: replace stub-oracle `reference.attr` with a real SWOS_TEST recording.

## What Wave 2 needs next

1. **B4** — player movement under scripted input  
2. Thin C1 debug view when B3/B4 want to be watched  

---

## Key paths

| Area | Location |
|---|---|
| Core / state | `src/core/include/core/{match_state,match_clock,ball,out_of_play,hash,trace,match_*}.hpp` |
| B1–B3 plans | `doc/implementation/B1-state-layout.md`, `B2-frame-state-machine.md`, `B3-ball-physics.md` |
| Assets | `src/assets/`, `src/tools/assetc/`, `src/app/render/{asset_source,placeholder,imported}_*` |
| Placeholder art | `assets/placeholder/` (`gen-placeholder`) |
| Game data | `src/data/include/data/{game_data,fictional}.hpp` |
| Trace tools | `src/tools/tracekit/`, `tracegen/`, `tracediff/`, `trace_viewer/` |
| Reference instrumentation | `tools/reference/` |
| Corpus | `tests/corpus/{kickoff,shot_curl}/` |
| Null UI | `src/app/ui/null_backend.hpp` |
| Tests / golden | `tests/`, `tests/golden/kickoff.attr` |
| CI | `.github/workflows/ci.yml` |
| Walls | `tools/check_walls.py` |
| Trig gen | `tools/gen_trig.py`, `tools/verify_trig.py` |
