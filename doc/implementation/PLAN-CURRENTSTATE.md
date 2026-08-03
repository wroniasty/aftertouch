# PLAN — Current state

Snapshot of foundation (A) and match-engine Waves 2–3 against [PLAN.md](PLAN.md).  
Statuses: **done** · **partial** · **not started**.  
Updated after B12/B11 closed locally; regenerate this file when a part's acceptance moves.

---

## Wave position

| Wave | Gate | Status |
|---|---|---|
| **0 — Skeleton** | A1; PLAN.md §8 on Windows and macOS | **Passed** |
| **1 — Instrument** | A2, A3, A4, A6 green; reference + engine traces in viewer | **Open** — A2/A3/A6 green; A4 first-launch prompt still open |
| **2 — The ball moves** | B1, B2, B3, B4, B10 (+ thin C1 slice) | **Gate met locally** — B1–B4, B10, C1a done |
| **3 — Possession / kicks** | B5–B9, B12, B11 | **Gate met locally** — B5–B12, B11 done; play-feel gate remains |

A5 was scheduled for Wave 3 but is implemented early. Wave 2 playable slice: keyboard/gamepad + dots. Full C1 remains Wave 4.

---

## Part A summary

| ID | Part | Status | Acceptance |
|---|---|---|---|
| **A1** | Kernel | **done** | Build, walls, skeleton — PLAN.md §8 |
| **A2** | Determinism primitives | **done** | Fix, Amiga kernel, RNG, HashState; committed hash gate |
| **A3** | Trace harness | **done** | Format, differ+drift, corpus, viewer; stub-oracle refs until SWOS recorder runs |
| **A4** | Asset pipeline | **partial** | Format + `assetc` + `IAssetSource`/placeholder/imported; first-launch prompt still open. §6.1/6.2/6.5 resolved by C3 |
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
| **B6** | Kicking | **partial** | Structure landed; B6a fixed six structural defects, timing constants still unfitted |
| **B7** | Contests | **done** | Slide, foul, contest RNG, headers; contest-sequence HashState pin |
| **B8** | Set pieces & referee | **done** | Restarts, aim tables, cards/injury, ref machine; restart-cycle HashState |
| **B9** | AI | **done** | Selection, GK, CPU brain-as-joystick; CPU-vs-CPU + HashState pin |
| **B10** | Match input | **done** | Keyboard/gamepad → MatchInput (seven-field path) |
| **B12** | Performance rating | **done** | Chronicle + match_stats + band weights; HashState unchanged by compute |
| **B11** | Result simulation | **done** | Scripted/Table/Engine `IResultSimulator`; envelope tests |
| **B13** | Amiga oracle incorporation | **partial** | R1–R5 landed, `ctest` green; restart placement coordinates outstanding, six disagreements instrumented but unsettled |

### Part C (Wave 2–4)

| ID | Part | Status | Acceptance |
|---|---|---|---|
| **C1a** | Debug match view | **done** | Follow-cam + landmarks + play HUD; A5 kickoff; FT freeze; V toggles full pitch |
| **C1** | Render core (full) | **done** | Original stadium tiles + palette sprites at integer scale |
| **C2** | Camera | **done** | Five-mode priority, lead offset, ease + cap, two-stage clipping |
| **C3** | Kits, animation, ball, pitch | **done** | Kit palettes, frame stepper in core, real ball + shadow, shirt number |
| **C1b** | Sandbox match mode | **done** | N players + attrs vs a lone keeper, chosen ends, R resets to the same kickoff |

---

## B8 — Set pieces & referee — **done**

Subfile: [B8-set-pieces.md](B8-set-pieces.md)

| Work item | State |
|---|---|
| `BeginRestart` four writes + ball park | landed |
| Foul → pen / FK / Foul classify; wired from B7 | landed |
| OOP complete-setup + throw-in ±3 place | landed |
| Aim tables + `ApplyRestartTake` → InProgress | landed |
| Cards + injury rolls via `resolve_rng` | landed |
| Referee state machine (`UpdateReferee`) | landed |
| Unit tests + `test_restart_cycle` HashState | landed |
| Golden / corpus re-pins | landed |
| Aim-only take + `PickRestartTaker` (human+CPU) + stop-all | landed |

Done-when met locally: restart families resume to open play; scripted HashState pin. Card camera art is C2/C3.

---

## B12 — Performance rating — **done**

Subfile: [B12-performance-rating.md](B12-performance-rating.md)

| Work item | State |
|---|---|
| MatchChronicle + goal/card/injury/corner events | landed |
| Goal attribution + attempts/corners latches | landed |
| `PlayerMatchStats` + ATTR / HashState | landed |
| Wire passes/tackles/headers/carry/saves/fouls | landed |
| Position-band weights + weighted `ComputePlayerRating` | landed |
| B11 synth invents `match_stats` | landed |
| Unit + HashState invariance + band tests | landed |
| Golden / corpus / hash re-pins (`match_stats` wire) | landed |

Done-when met locally: single 1–10 + expanded breakdown; compute is post-match only.

---

## B13 — Amiga oracle incorporation — **partial**

Subfile: [B13-amiga-oracle.md](B13-amiga-oracle.md)

| Work item | State |
|---|---|
| R1 doc propagation 0–15 → 0–7 across `doc/implementation/` | landed |
| R2 heading table 13 → 8; attribute range 0–7; squad projection | landed |
| R2 `static_assert` one entry per attribute on every indexed table | landed |
| R3 Amiga launch / curl / decay / keeper-reach values; signed shot bonuses | landed |
| R3 distance-banded pass strength | landed (banding structure sourced, interior ours) |
| R4 dribble touch-count (+ `ATTR` **v6**) | landed |
| R4 goal-versus-save resolution stage (consumes no RNG) | landed |
| R4 goalmouth scatter; near-miss whistle flag | landed |
| R4 four corner turn masks + CPU horizontal-axis denial | landed |
| R4 celebration two-draw RNG cost | landed |
| R4 restart placement coordinates (goal-kick X, symmetric state encoding) | **not started** |
| R5 six disagreement switches in `profile.hpp`, defaulting to reading A | landed |
| Re-pins: 5 hash pins, golden, both corpus pairs, four cycles | landed |

B13 returns to **done** when the restart placement coordinates land. The six
disagreements are *instrumented, not settled* — each needs one targeted trace
([A3](A3-trace-harness.md) item 4), and that is deliberate.

Two pre-existing defects fell out of the re-pin discipline and are fixed: the
committed corpus had been `ATTR` v4 since B6a bumped the format to v5 and was
never regenerated (the corpus check compares committed files against each other,
so it cannot detect its own staleness), and `attempt_latched` had never reached
the wire despite B1's acceptance claiming full-state round-trip.

---

## B11 — Result simulation — **done**

Subfile: [B11-result-simulation.md](B11-result-simulation.md)

| Work item | State |
|---|---|
| `MatchResult` + fidelity + extract helpers | landed |
| `IResultSimulator` + Scripted / Table / Engine | landed (`data/result_simulator.hpp`) |
| Harness season runner uses Engine backend | landed |
| Fixed-seed Table vs Engine envelope | landed |
| Golden / corpus / hash re-pins (chronicle wire) | landed |

Done-when met locally: interchangeable backends; resolve RNG isolated from match HashState.

---

## B9 — AI — **done**

Subfile: [B9-ai.md](B9-ai.md)

| Work item | State |
|---|---|
| Pass-target + controlled selection exclusions / switch lockout | landed |
| `AI_SetControlsDirection` shoot/pass/dribble/chase | landed |
| CPU aftertouch + restart taker | landed |
| Goalkeeper rest/claim/dive/catch | landed |
| Wire `ApplyTeamControls` / fire / aftertouch (CPU vs human) | landed |
| Goal → centre stoppage resume (unblocks full match under AI) | landed |
| Unit tests + CPU-vs-CPU + `test_ai_b9` HashState | landed |
| Golden / corpus / hash re-pins | landed |

Done-when met locally: CPU vs CPU short match moves the ball; scripted HashState pin. Marking/pressing and difficulty tiers remain out of scope.

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

Done-when met locally via scripted HashState pin. Corpus contest *distribution* remains A3 follow-up.

---

## B6 — Kicking — **partial**

Subfiles: [B6-shooting.md](B6-shooting.md), [B6a-kick-fidelity.md](B6a-kick-fidelity.md)

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

### B6a — Kick & aftertouch fidelity — **partial**

Subfile: [B6a-kick-fidelity.md](B6a-kick-fidelity.md)

| Work item | State |
|---|---|
| S1 curl geometry (E/W rows) + `spin_cw`/`spin_ccw` rename | landed |
| S2 window opens the Step after the strike (tick 0 reachable) | landed |
| S3 `normal_fire` as a level; contest entry on the press edge | landed |
| S4 possession during the fire charge | landed |
| S5 `ClassifyShotOnGoal` position gate + real penalty area | landed |
| S6 `restart_shortfall` split out; pass loft implemented | landed |
| Behavioural suite (7 files) + property-style `test_aftertouch` | landed |
| `KickProbe` + C1a control HUD + transcript `kick:` line | landed |
| `shot_curl` corpus scenario reaches the ball; ATIN v2 setup ids | landed |
| Fit the timing constants against the oracle (Track M) | **not started** — needs [A3](A3-trace-harness.md) item 4 |

B6 returns to **done** when Track M closes. The engine's kick constants are
tagged `[PROVISIONAL: LEGACY §15 …]` in code until then.

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
| Ball-follow `DebugView` + clamp + `V` toggle | landed |
| Landmarks (boxes, centre circle, goal mouths) | landed |
| Has-ball / pass-target rings; GK tint | landed |
| Play HUD (clock, phase, restart name, HT/FT) | landed |
| A5 `ApplyKickoff` bootstrap (human home / CPU away) | landed |
| Freeze Stepping at FullTime; HT auto-resumes | landed |
| Live MATCH `.atin` + sparse `.txt` under `traces/` | landed |
| `tracegen --transcript` (ATTR / ATIN) | landed |

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
| Types: player / team / tactic (10×35 grid), attrs **0–7** (B13 / R2) | landed |
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

## Play-feel fixes (post B11)

| Fix | State |
|---|---|
| `MarkBallLoose` — auto-switch when ball is loose | landed |
| Fictional tactics own-half shapes + `team_playing_up` mirror | landed |

## C1 — Render core — **partial**

Subfile: [C1-render-core.md](C1-render-core.md)

| Work item | State |
|---|---|
| Subfile + CURRENTSTATE | landed |
| `PitchTiles` palette + optional `ball.atp` on import | landed |
| Pure grid/expand helpers + `test_pitch_tiles` | landed |
| `PitchAtlas` + `DrawMatch` wired from `main` | landed |
| Seasonal pitch-number / weather art | not started |
| Retire landmark overlay / player dots | C2 / C3 |

Wave 3 feel gate accepted as good-enough (not complete). A4 original import present under `assets/generated/` (no `ball.atp` — synthesised).

## C1b — Sandbox match mode — **done**

Subfile: [C1b-sandbox-mode.md](C1b-sandbox-mode.md)

| Work item | State |
|---|---|
| Off-pitch rule (`IsOffPitch` / `ParkOffPitch`) honoured at kickoff + in team controls | landed |
| Restart taker falls back to the keeper; control slot released when no field player | landed |
| `SandboxConfig` + `BuildSandboxState` + `StartSandbox` (`src/app/mode/`) | landed |
| SANDBOX button, config dialog, `R` / `Shift+R` reset, half-time reset | landed |
| Arrow-key movement, ImGui keyboard gating (`PollNeutral`) | landed |
| `test_offpitch_players`, `test_restart_one_man_side`, `test_sandbox_setup` | landed |

Two engine defects fell out of it and are fixed: a sent-off player was put back on the
pitch by the next kickoff and then walked to his tactics cell, and a side with no
outfielder left sent an off-pitch player to take restarts. Goldens and corpus unmoved —
no pinned scenario contains a sending-off.

---

## C2 — Camera — **done**

Subfile: [C2-camera.md](C2-camera.md)

| Work item | State |
|---|---|
| `Camera` outside `at_core`; kick-off end from `presentation_rng` | landed |
| Mode priority: frozen / booking / shootout / substitution / standard | landed |
| Lead offset ±2 → ±40 on the ball delta, taker facing when stopped | landed |
| `/16` ease + 5-unit cap; destination clip then position clip | landed |
| `DrawMatch` takes a camera; C1a's follow/full toggle retired | landed |
| `test_camera` — ramp, ease, clips, freeze, shootout, corner margin, hash invariance | landed |

Bench mode and the left-edge slide stay C6; CAMERA.md §6's gate is unresolved even in the
reference and was not invented here.

## C3 — Kits, animation, ball, pitch — **done**

Subfile: [C3-sprites-animation.md](C3-sprites-animation.md)

| Work item | State |
|---|---|
| Pitch from `pitchN.blk` + `pitchN.dat` (8-bit tiles, 6 pitches); row mapping by `SourceKind` | landed |
| `assetc` geometry banks from `team1/2/3.dat`; ball, numbers; kit ordinals in `palette.atl` aux | landed |
| `KitBank` — 2 sides × 3 faces + 2 keeper kits, baked at kickoff | landed |
| Frame taxonomy measured off the art (facing, mirror pairing, foot spread, body axis) | landed |
| `core/animation.hpp` stepper + tables; wired into `Step`; renderer draws `image_index` | landed |
| Real ball: 4 rotation frames + separate shadow sprite, speed-driven spin | landed |
| Shirt number above the controlled player; white ring gone; HUD behind `F1` | landed |
| Hash re-pin: 8 scenarios + golden + both corpus chains | landed |
| `test_animation`, `test_kit_palette`, extended `test_pitch_tiles` / parity | landed |

Left explicitly unfinished and marked `[PROVISIONAL]` in `animation_tables.hpp`: header,
throw-in, celebration and keeper-dive frame ranges are identified but not mapped, and
those states fall back to standing rather than to a guess.

## What Wave 4 needs next

1. **C4** match presentation — scoreboard, cards, replays (reuses the number sprite)
2. Finish the C3 frame taxonomy: headers, throw-ins, keeper dives  

---

## Key paths

| Area | Location |
|---|---|
| Core / state | `src/core/include/core/{match_state,match_clock,ball,movement,possession,shooting,aftertouch,tackling,heading,set_pieces,referee,game_events,out_of_play,hash,trace,match_*}.hpp` |
| B1–B8 / B10 / C1a plans | `doc/implementation/B1-…`, `B5-possession.md`, `B6-shooting.md`, `B7-contests.md`, `B8-set-pieces.md`, `B10-match-input.md`, `C1a-debug-match-view.md` |
| App input / debug draw | `src/app/input/match_input_source.*`, `src/app/render/{match_renderer,pitch_view}.*` |
| Sandbox mode | `src/app/mode/sandbox.*`, `src/app/ui_imgui/screens/sandbox_menu.*` |
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
