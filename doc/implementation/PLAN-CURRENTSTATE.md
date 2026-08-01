# PLAN — Current state

Snapshot of foundation (A) and match-engine Waves 2–3 against [PLAN.md](PLAN.md).  
Statuses: **done** · **partial** · **not started**.  
Updated after B7 closed locally; regenerate this file when a part's acceptance moves.

---

## Wave position

| Wave | Gate | Status |
|---|---|---|
| **0 — Skeleton** | A1; PLAN.md §8 on Windows and macOS | **Passed** |
| **1 — Instrument** | A2, A3, A4, A6 green; reference + engine traces in viewer | **Open** — A2/A3/A6 green; A4 first-launch prompt still open |
| **2 — The ball moves** | B1, B2, B3, B4, B10 (+ thin C1 slice) | **Gate met locally** — B1–B4, B10, C1a done |
| **3 — Possession / kicks** | B5, B6, B7, … | **In progress** — B5–B7 done; set pieces are B8 |

A5 was scheduled for Wave 3 but is implemented early. Wave 2 playable slice: keyboard/gamepad + dots. Full C1 remains Wave 4.

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
| **B4** | Player movement | **done** | 22 players, scripted 200-tick HashState pin |
| **B5** | Possession | **done** | Bands, capture/dribble, dribble-turn HashState pin |
| **B6** | Kicking | **done** | Tap/hold launch, aftertouch, curled-shot HashState pin |
| **B7** | Contests | **done** | Slide, foul, contest RNG, headers; contest-sequence HashState pin |
| **B10** | Match input | **done** | Keyboard/gamepad → MatchInput (seven-field path) |
| **B8+** | Set pieces / … | **not started** | — |

### Part C (Wave 2 slice)

| ID | Part | Status | Acceptance |
|---|---|---|---|
| **C1a** | Debug match view | **done** | Pitch + dots in 320×200; walkable with B10 |
| **C1** | Render core (full) | **not started** | Atlas / tiles / weather — Wave 4 |

---

## B7 — Contests — **done**

Subfile: [B7-contests.md](B7-contests.md)

| Work item | State |
|---|---|
| Fire fork (kick iff `player_has_ball`) | landed |
| Slide begin / early release / recovery tables | landed |
| Ball contact deflect + good-tackle | landed |
| Foul test (keeper exempt; from-behind) | landed |
| Possession contest via `resolve_rng` | landed |
| Static / jump headers + lob/drive switch | landed |
| `ProcessContestContacts` after `MovePlayers` | landed |
| Unit tests + `test_contest_sequence` HashState | landed |
| Golden / corpus / hash re-pins | landed |

Done-when met locally via scripted HashState pin. Corpus contest *distribution* remains A3 follow-up. Foul consequences (cards, free kicks, injury) are B8.

---

## B6 — Kicking — **done**

Subfile: [B6-shooting.md](B6-shooting.md)

| Work item | State |
|---|---|
| Tap/hold → `quick_fire` / `normal_fire` | landed |
| `ApplyKickOrPass` launch + lockout + spin open | landed |
| Shot Finishing/Velocity bonus; pass cone | landed |
| Aftertouch window (curl + tick-4 drive/lob) | landed |
| Wired in `ApplyTeamControls` / `UpdateBall` | landed |
| Unit tests + `test_curled_shot` HashState | landed |
| Golden / corpus / movement re-pinned | landed |

Done-when met: scripted curled-shot `HashState` stable under Amiga profile. Table values are provisional fit targets; real SWOS ATTR remains an A3 follow-up.

---

## B5 — Possession — **done**

Subfile: [B5-possession.md](B5-possession.md)

| Work item | State |
|---|---|
| Planar + z proximity bands (`possession.hpp`) | landed |
| Capture / release + `pass_kick_timer` lockout tick | landed |
| Dribble aim-ahead + Control speed trim | landed |
| Wired in `ApplyTeamControls` before dest/speed | landed |
| `test_proximity_bands` / `test_possession_capture` / `test_dribble` | landed |
| `test_dribble_turn` HashState acceptance | landed |
| Golden / corpus / determinism re-pinned | landed |

Done-when met: scripted dribble-and-turn `HashState` stable under Amiga profile. Real SWOS ATTR remains an A3 follow-up.

---

## B10 — Match input — **done**

Subfile: [B10-match-input.md](B10-match-input.md)

| Work item | State |
|---|---|
| Core `GameControlEvents` + `EventsToDir` + opposite-axis filter | landed |
| App `MatchInputSource` (keyboard + gamepad) | landed |
| Main loop polls into `MatchEngine::Step` | landed |
| MATCH bootstrap: home human / away CPU + tactics seed | landed |
| `test_game_events` | landed |

Done-when met: arrows/WASD drive home controlled player through the same seven-field interface. Config UI deferred.

---

## C1a — Debug match view — **done**

Subfile: [C1a-debug-match-view.md](C1a-debug-match-view.md)

| Work item | State |
|---|---|
| `PitchToScreen` fixed frustum → 320×200 | landed |
| `DrawMatch` pitch + ball + 22 dots | landed |
| `main` uses `DrawMatch` (stub removed) | landed |
| `test_pitch_to_screen` harness | landed |

Explicitly not full C1. Camera modes = C2; sprites = C3.

---

## B4 — Player movement — **done**

Subfile: [B4-player-movement.md](B4-player-movement.md)

| Work item | State |
|---|---|
| `PlacePlayersAtKickoff` via tactics grid | landed |
| `ApplyTeamControls` one team/tick; `MatchInput` → seven fields | landed |
| Controlled dest (`kDefaultDestinations` / stop); boundary + turn flags | landed |
| Off-ball tactics destinations + bottom mirror | landed |
| Speed tables + on-ball / injury modifiers; `frameDelay` | landed |
| `MovePlayers` axis-snap integrate all 22 | landed |
| Non-normal speed decay (no state entry) | landed |
| Golden unit tests + `test_move_players` acceptance | landed |
| Golden / corpus / determinism re-pinned | landed |

Done-when met: twenty-two players move under scripted input for 200 ticks with a committed `HashState` in `core_tests`. CPU joystick is B9; devices are B10.

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

## What Wave 2 / 3 needs next

1. **B8** — set pieces & referee  
2. Full **C1** (Wave 4) when presentation work starts — do not grow C1a  

---

## Key paths

| Area | Location |
|---|---|
| Core / state | `src/core/include/core/{match_state,match_clock,ball,movement,possession,shooting,aftertouch,tackling,heading,game_events,out_of_play,hash,trace,match_*}.hpp` |
| B1–B7 / B10 / C1a plans | `doc/implementation/B1-…`, `B5-possession.md`, `B6-shooting.md`, `B7-contests.md`, `B10-match-input.md`, `C1a-debug-match-view.md` |
| App input / debug draw | `src/app/input/match_input_source.*`, `src/app/render/{match_renderer,pitch_view}.*` |
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
