# PLAN — Current state (Part A)

Snapshot of foundation layer **A1–A6** against [PLAN.md](PLAN.md).  
Statuses: **done** · **partial** · **not started**.  
Updated after A5/A6 landed; regenerate this file when a part's acceptance moves.

---

## Wave position

| Wave | Gate | Status |
|---|---|---|
| **0 — Skeleton** | A1; PLAN.md §8 on Windows and macOS | **Passed** |
| **1 — Instrument** | A2, A3, A4, A6 green; reference + engine traces in viewer | **Open** — harness and CI exist; A2/A3/A4 acceptance not fully closed |
| **2+** | B / C / D / E | Not started |

A5 was scheduled for Wave 3 but is implemented early; B9 can consume tactics data when it arrives.

---

## Part A summary

| ID | Part | Status | Acceptance |
|---|---|---|---|
| **A1** | Kernel | **done** | Build, walls, skeleton — PLAN.md §8 |
| **A2** | Determinism primitives | **partial** | Fix + wall tokens; not yet full Amiga kernel / RNG / HashState gate |
| **A3** | Trace harness | **partial** | Record format + generate/diff; no viewer, no reference corpus |
| **A4** | Asset pipeline | **partial** | `ATAP` + `assetc` import path; runtime `IAssetSource` / first-launch incomplete |
| **A5** | Game data | **done** | Fictional league → `MatchState`; `ATGD` round-trip |
| **A6** | Test infrastructure | **done** | `ctest` suite, golden wedge, CI workflow, core tests sans SDL |

---

## A1 — Kernel — **done**

CMake presets (win/mac), `at_core` / `at_app` skeleton, SDL3 + ImGui + doctest vendored, `tools/check_walls.py` on the default build. No subfile (by plan).

---

## A2 — Determinism primitives — **partial**

Subfile: [A2-determinism-primitives.md](A2-determinism-primitives.md)

| Work item | State |
|---|---|
| `Fix` pinned + golden vectors (`test_fixed.cpp`) | landed |
| Wall check: no float / clock / unseeded rand in `src/core/` | landed |
| Angle model, trig tables, `CalculateDeltaXAndY`, Amiga profile | **not landed** |
| Three-stream RNG in state | **not landed** (engine holds a stub `rng_` only) |
| `HashState` + padding-free `MatchState` | **not landed** |
| Cross-platform committed hash gate | **not landed** |

`MatchEngine::Step` still only advances `tick`. Bit-identical physics is not yet demonstrable.

---

## A3 — Trace harness — **partial**

Subfile: [A3-trace-harness.md](A3-trace-harness.md)

| Work item | State |
|---|---|
| `core/trace.hpp` — header + per-tick record, LE, hashed payload | landed |
| Round-trip / corruption / endian tests | landed |
| Generate + first-divergence diff (`at_tracekit`, `tracegen`, `tracediff`) | landed (via A6) |
| Reference port instrumentation + `SWOS_TEST` / Amiga corpus | **not landed** |
| `trace_viewer` | **not landed** |
| Committed reference corpus / `record_corpus.py` | **not landed** |
| `HIL2` reader | **not landed** |

Done-when (“two traces in the viewer, first divergence tick”) is **not met**. Engine golden regression (A6) covers “did we change?”; faithfulness vs reference is still open.

---

## A4 — Asset pipeline — **partial**

Subfile: [A4-asset-pipeline.md](A4-asset-pipeline.md)

| Work item | State |
|---|---|
| `ATAP` container + validate (`assets/asset_pack.hpp`) | landed |
| `assetc` — original / ref-tree import into `assets/generated/` | landed (in tree) |
| Format tests (`asset_tests`) | landed |
| `IAssetSource` / `PlaceholderAssets` / `ImportedAssets` | **incomplete** |
| Placeholder generator parity tests | **incomplete** |
| First-launch import prompt (shell screen) | **not landed** (D1-shaped) |
| Write-location guard / fuzz of decode | partial (A6 fuzz hits `Validate`) |

Clean-clone-without-original-data is the intended bar; runtime asset *serving* is not finished.

---

## A5 — Game data — **done**

Subfile: [A5-game-data.md](A5-game-data.md)

| Deliverable | State |
|---|---|
| Own `ATGD` schema (league + career snapshot by team id) | landed |
| Types: player / team / tactic (10×35 grid), attrs **0–15** | landed |
| `MakeFictionalLeague()` — 8 teams, 3 tactics | landed |
| `ApplyKickoff` → `MatchState` projection (sheets, attrs, kits) | landed |
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

Acceptance met locally (`ctest` six suites). CI green depends on the workflow running on the remote; not verified in this snapshot.

**Test binaries:** `core_tests`, `asset_tests`, `data_tests`, `harness_tests`, `fuzz_tests`, `wall_check_tests`.

---

## What Part A has unlocked

- Walls and CI discipline for every later part  
- Trace *format* and golden *regression* (change detection)  
- Import path for art bytes and a playable fictional league  
- Headless hooks for D (null UI) and E/B11 (season runner)

## What still blocks Wave 1 close

1. **A2** — finish kernel, RNG, `HashState`, cross-platform hash gate  
2. **A3** — reference instrumentation + corpus + viewer (faithfulness instrument)  
3. **A4** — runtime `IAssetSource` path so placeholder/import are interchangeable at draw time  

Until those close, Wave 2 (B1+) should not start under the plan's sequencing rules — even though A5/A6 are ahead of schedule.

---

## Key paths

| Area | Location |
|---|---|
| Core | `src/core/include/core/{fixed,trace,match_*}.hpp` |
| Assets | `src/assets/`, `src/tools/assetc/` |
| Game data | `src/data/include/data/{game_data,fictional}.hpp` |
| Trace tools | `src/tools/tracekit/`, `tracegen/`, `tracediff/` |
| Null UI | `src/app/ui/null_backend.hpp` |
| Tests / golden | `tests/`, `tests/golden/kickoff.attr` |
| CI | `.github/workflows/ci.yml` |
| Walls | `tools/check_walls.py` |
