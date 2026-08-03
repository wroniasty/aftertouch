# Implementation plan

The work breakdown for **aftertouch**. Twenty-four reference documents in
[../](../README.md) say *what the thing does*; [../PLAN.md](../PLAN.md) says *how the
repo is shaped and in what order the phases run*. This document sits between them: it
cuts the build into named parts, fixes their dependencies, and schedules them.

> **Status.** This is a skeleton. Every part below is a one-paragraph stub plus a
> pointer to a subfile that does not exist yet. Subfiles get written **just before
> their part starts**, not up front — a plan for Wave 4 written today would be
> fiction. The catalogue and the schedule are the stable parts; the detail is not.

---

## 0. One-paragraph version

Five layers, thirty-five parts. **Foundation** (A) is the measuring instrument and the
data pipeline — trace harness first, because you cannot tune physics you cannot
measure. **Match** (B) is the pure `src/core/` simulation, built in dependency order:
state, frame, ball, movement, possession, kicks, contests, set pieces, AI, and the
swappable result simulator for the thousands of fixtures nobody plays. It is the
only layer whose correctness has an objective test. **Presentation** (C) and **Shell**
(D) follow, and are deliberately last, because [../PLAN.md](../PLAN.md) §9 is right
that a beautiful shell around a match that feels wrong is worth nothing. **Career
depth** (E) is the layer where this game stops being a reimplementation — ageing,
ceilings, academies, transfers, wages — and it is separated from the match by a wall
as hard as the SDL one: the engine sees 0–7 integers and nothing else, forever
(§6, rule 4). The schedule runs in seven waves; each wave has a gate that must be
demonstrated, not asserted — and everything is tested to a standard set per layer in
§7, because a deterministic engine behind a hard wall is the rare case where that is
actually achievable.

---

## 1. How to use this document

**Part IDs are permanent.** `B3` is ball physics forever. Subfiles, commits, branch
names and TODOs cite the ID. Parts may be re-scoped or split (`B3a`, `B3b`); they are
never renumbered.

**One subfile per part**, named `<ID>-<slug>.md` in this directory —
`B3-ball-physics.md`, `A3-trace-harness.md`. The template is §5.

**A part is not started until its subfile exists**, and the subfile is written by
reading the reference documents in its *Sources* column, not from memory.

**The catalogue is not a schedule.** §3 says what the parts are and what they depend
on. §4 says when they run. Dependencies constrain the schedule; they do not define it.

---

## 2. Where the build actually is

| | |
|---|---|
| **Done** | **Milestone 1 is complete.** The [../PLAN.md](../PLAN.md) §3–§6 skeleton is in the tree and the §8 checklist is signed off: root CMake + presets, SDL/ImGui/doctest submodules, `src/core/` stub with `fixed.hpp` and a `MatchEngine` that links nothing, `src/app/` with the platform window, the `ui/` door, one ImGui screen, `tests/core/`, and `tools/check_walls.py` wired into the default build. Both platforms build, the tick runs at 50 Hz independent of refresh rate, the match view letterboxes crisply, and `ctest` passes. |
| **Empty** | `src/tools/` is a `.gitkeep`. No trace format, no importer, no physics — `MatchEngine::Step` is a stub. |

So Wave 0 is closed and the build starts at **Wave 1: the instrument** — A2, A3, A4.
No engine physics is written until the trace viewer can report a divergence tick.

---

## 3. The parts

Columns: **Depends** is hard prerequisites only. **Sources** are the reference
documents the subfile must be written from. **Done when** is a single demonstrable
condition, deliberately narrow — the subfile expands it into a real checklist.

### A. Foundation — the instrument and the pipeline

| ID | Part | Depends | Sources | Done when |
|---|---|---|---|---|
| **A1** | **Kernel.** *(complete)* Build parity on both platforms, the wall check in the default build, the skeleton everything else hangs off. | — | [../PLAN.md](../PLAN.md) §1–§8 | Done — every §8 box ticked on both platforms. Needs no subfile. |
| **A2** | **Determinism primitives.** 16.16 fixed point, the octant model, direction/trig tables, the seeded RNG, and the no-float/no-time discipline that makes replay exact. | A1 | [../STATE.md](../STATE.md), [../AI.md](../AI.md) §5, [../BALL.md](../BALL.md) §1 | Same seed + same input sequence ⇒ bit-identical state hash on Windows and macOS. |
| **A3** | **Trace harness.** The trace record format, engine-side emission, reference-side instrumentation, `src/tools/trace_viewer/`, and the input corpus. | A2 | [../PLAN.md](../PLAN.md) §9 Phase 0, [../STATE.md](../STATE.md) | Two traces load side by side and the viewer reports first-divergence tick for a recorded kickoff. |
| **A4** | **Asset pipeline.** `src/tools/assetc/` reading an owned original install into *our* runtime format, `IAssetSource` with placeholder and imported implementations, first-launch import prompt. | A1 | [../PLAN.md](../PLAN.md) §10, [../RENDERING.md](../RENDERING.md), [../PLAYER_SPRITES.md](../PLAYER_SPRITES.md) §1 | Clean clone with no original data builds, runs and passes tests on placeholder art; with data present, imports without touching git. |
| **A6** | **Test infrastructure.** The harness everything else is verified with: doctest wiring, fixtures, golden-trace regression, the headless season runner, offscreen render capture, the null UI backend, fuzz entry points, and CI on both platforms. See §7. | A3 | [A6-test-infrastructure.md](A6-test-infrastructure.md) | `ctest` runs the whole suite on both platforms in CI, core tests link no SDL, and a deliberately introduced physics change fails a golden trace. |
| **A5** | **Game data.** Team, player, tactics and career file readers; our own on-disk schema; the fictional default dataset. Attributes are 0–7. | A4 | [../DATA.md](../DATA.md), [A5-game-data.md](A5-game-data.md) | A league of fictional teams loads into `MatchState` and round-trips through our format. |

### B. Match engine — `src/core/`, pure, headless, testable

Built in dependency order — which is why B12 is listed before B11; IDs never move once
assigned. B1–B10 are trace-diffed against the corpus as they land, and the divergence
number is the acceptance criterion, not a code review. B11 and B12 are departures from
the original with nothing to diff against, so they are judged on distributions instead.

| ID | Part | Depends | Sources | Done when |
|---|---|---|---|---|
| **B1** | **State & layout.** Entity structs, the arena, the offset map, and the accessor discipline the rest of the engine is written against. | A2 | [../STATE.md](../STATE.md) | Full match state serialises to a trace record and back losslessly. |
| **B2** | **Frame & state machine.** The 50 Hz tick order, the match state enum, the clock, out of play, fouls and cards, statistics, period end. The spine. | B1 | [../SIMULATION.md](../SIMULATION.md) | A match runs 90 simulated minutes headless, ends, and produces a scoreline of 0–0. |
| **B3** | **Ball physics.** Friction, gravity, bounce, dead-ball barrier, goal-frame collision, the landing predictor, per-surface tables. Destination rewriting, not velocity negation. | B2 | [../BALL.md](../BALL.md), [../PITCH.md](../PITCH.md) §4 | A kicked ball's trajectory tracks the reference trace within the tolerance A3 defines. |
| **B4** | **Player movement.** The per-tick pipeline, speed, eight-way input, boundaries, turn restrictions, non-normal states. | B3 | [../MOVEMENT.md](../MOVEMENT.md), [../INPUT.md](../INPUT.md) §1 | Twenty-two players move under scripted input with no divergence in the first 200 ticks. |
| **B5** | **Possession.** Proximity bands, dribbling, losing the ball. | B4 | [../CONTROL.md](../CONTROL.md) | Dribble-and-turn sequences from the corpus reproduce. |
| **B6** | **Kicking.** Tap vs hold, launch speed and height, the shot-on-goal bonus, and the aftertouch window — lateral curl and the drive/lob switch. | B5 | [../SHOOTING.md](../SHOOTING.md), [../AFTERTOUCH.md](../AFTERTOUCH.md) | A curled shot from the corpus lands within tolerance. **This is the part the project is named after; it gets the longest tuning budget.** |
| **B7** | **Contests.** The slide tackle, deflections, the foul test, recovery, the possession contest; static and jump headers and the driven/lob switch. | B6 | [../TACKLING.md](../TACKLING.md), [../HEADING.md](../HEADING.md) | Contest outcomes match the reference distribution over the corpus, not just individual ticks. |
| **B8** | **Set pieces & referee.** Restart states, foul classification, throw-in placement, aiming tables; the referee machine, cards, sending off. | B7 | [../SETPIECES.md](../SETPIECES.md), [../REFEREE.md](../REFEREE.md) | Every restart type can be executed and returns the match to open play. |
| **B9** | **AI.** Player selection, the zonal off-ball grid driven by tactics, the goalkeeper, the CPU brain. | B8, A5 | [../AI.md](../AI.md), [../DATA.md](../DATA.md) §4 | CPU vs CPU plays a full match that produces a plausible scoreline and shot count. |
| **B10** | **Match input.** The device layer, the two-level event model, the one-team-per-frame alternation, configuration. Interface in core, devices in app. | B4 | [../INPUT.md](../INPUT.md) | Gamepad and keyboard drive the same seven-field interface the AI uses. |
| **B12** | **Performance rating.** A 1–10 rating per player per match, derived from match events. Pure computation over what the engine already emits; **changes no gameplay**. Both B11 backends must produce it. | B2 | — (a departure from the original; [../SIMULATION.md](../SIMULATION.md) §7 for the statistics that feed it) | The same match played and re-resolved by table produces ratings on one scale, and removing the rating code changes no tick of simulation. |
| **B11** | **Result simulation.** *View result* for matches nobody plays: one `IResultSimulator` interface, at least two interchangeable backends — a cheap statistical model and the real engine run headless — plus the `MatchResult` they both fill. Contract below. | B9, A5, B12 | [../SIMULATION.md](../SIMULATION.md) §10, [../AI.md](../AI.md) §5, [../DATA.md](../DATA.md) §3 | A full division's fixtures resolve under either backend with no caller change, and a season under each lands in the same statistical envelope. |
| **B13** | **Amiga oracle incorporation.** Reconciling the engine with the second oracle: two corrections (the attribute range is 0–7; the heading table has 8 entries), the confirmed constants, four mechanics we never had, and six recorded disagreements turned into A/B switches. A values-and-details pass — the architecture is *confirmed* by the Amiga reading, not overturned. | B6a, A5, A3 | [../AMIGA_CHANGES.md](../AMIGA_CHANGES.md), [../amiga/](../amiga/), [B13-amiga-oracle.md](B13-amiga-oracle.md) | No 0–15 attribute range survives anywhere; every attribute-indexed table `static_assert`s to 8 entries; a dribble past the Control-derived touch limit loses the ball; all six disagreement switches exist and each has a corpus scenario that reaches its mechanic. |

#### B11 in detail: the pluggability contract

Called out here rather than left to the subfile, because getting the seam wrong is
what forces a rewrite later. A career plays a few dozen matches and *resolves* a few
thousand, so this code decides how a season feels far more than it looks like it
should.

**One interface, several backends.** `IResultSimulator::Resolve(Fixture, Seed) ->
MatchResult`. Backends we know we want:

| Backend | What it is | Why |
|---|---|---|
| `TableResultSimulator` | Statistical: team-strength index into a goal-count table, then scorers rolled against per-position weights from the team's tactic. The [../SIMULATION.md](../SIMULATION.md) §10 shape. | Instant. Resolves a whole league's round in microseconds. The default. |
| `EngineResultSimulator` | The real `MatchEngine` (B1–B9) run headless with AI on both sides, at speed, result read off the final state. | The reason Rule 1 exists ([../PLAN.md](../PLAN.md) §0). Results generated by the same rules the player experiences. |
| `ScriptedResultSimulator` | Fixed outcomes from a table. | Tests and D3's competition-engine work. Lets tiebreakers and promotion rules be tested without simulating anything. |

**Non-negotiables for the seam:**

- **A separate RNG stream.** Resolving other fixtures must not perturb the match
  engine's stream — the original is explicit about this, using `Rand2()`
  ([../SIMULATION.md](../SIMULATION.md) §10). Get this wrong and playing a match
  after viewing a result changes the match.
- **Pure function of `(Fixture, Seed)`.** No hidden state, no clock, no I/O — the same
  wall as every other B part. Results are then reproducible rather than stored, and
  a fixture resolved twice cannot disagree with itself. The original cached
  `CalculateViewResult` precisely because it could not guarantee this; we can.
- **One `MatchResult`, filled by everyone.** Score, scorers with minutes, cards,
  injuries, and the statistics the HUD shows. Plus a **fidelity flag** saying which
  fields are simulated in earnest and which were synthesised — so a consumer can tell
  a real shot count from a plausible one, and no consumer has to handle absent fields.
  A backend that returns only a scoreline pushes that problem into every caller.
- **Backend choice is career configuration, not a code path.** Selected once and
  applied to every fixture in a competition, including the player's own if they skip
  it. Mixing backends within a season quietly advantages whoever gets the better
  model.

**The gate that makes the pluggability real** is calibration, not compilation: run N
seasons through each backend and compare the distributions — goals per game, home
advantage, spread of the final table, top-scorer totals. If the cheap model and the
engine disagree materially, the cheap model is wrong, and a career resolved with it
is a different game. That comparison is only possible because both sit behind one
interface, and it is the main argument for building the seam before the second
backend exists rather than after.

### C. Presentation

| ID | Part | Depends | Sources | Done when |
|---|---|---|---|---|
| **C1** | **Render core.** 320×200 logical space and the 672×848 pitch space, the atlas pipeline, tile grid, surface types and weather. | A4, B2 | [../RENDERING.md](../RENDERING.md), [../PITCH.md](../PITCH.md) | The pitch renders crisply at arbitrary window sizes with square pixels. |
| **C2** | **Camera.** Five modes, the lead-ahead offset, easing, two-stage clipping. | C1 | [../CAMERA.md](../CAMERA.md) | Following play feels anticipatory rather than reactive — a subjective gate, and it is the point. |
| **C3** | **Sprites & animation.** Layered kit compositing, the eight-octant model, animation tables, the frame stepper, depth and height. | C1, A4 | [../PLAYER_SPRITES.md](../PLAYER_SPRITES.md) | Both teams' sheets pre-generate at kickoff from arbitrary kit colours. |
| **C4** | **Match presentation.** HUD, scoreline, referee and card presentation, the blinking number, replays. | C3, B8 | [../REFEREE.md](../REFEREE.md), [../SIMULATION.md](../SIMULATION.md) | A goal plays back as a replay and returns to the restart. |
| **C5** | **Audio.** Commentary queue, the 28 event categories, scoreline-driven crowd, modding hooks. | B2 | [../AUDIO.md](../AUDIO.md) | Events raised by the engine reach the mixer through one queue with no engine-side audio dependency. |
| **C6** | **Bench.** The double-tap gesture, the five bench states, two-phase substitution, in-match tactics. | C4, B9 | [../BENCH.md](../BENCH.md) | A substitution completes mid-match without stopping the clock incorrectly. |

### D. Shell and meta-game

| ID | Part | Depends | Sources | Done when |
|---|---|---|---|---|
| **D1** | **The shell.** Retire `main.cpp`'s inline ImGui behind `IUiBackend`; screen model, navigation, the entry kinds. | A1 | [../PLAN.md](../PLAN.md) §6, [../MENUS.md](../MENUS.md) | Every screen goes through `DrawScreen(ScreenId, AppModel&) -> Intent`; `check_walls` still passes. |
| **D2** | **Persistence.** SQLite (vendored amalgamation) for career state, save/load, migrations. | D1, A5 | [../DATA.md](../DATA.md) §5 | A career survives a quit and restart. |
| **D3** | **Competition engine.** Stages as a declarative DAG, entrant sources, tiebreakers — leagues, cups, continental and international under one engine. Consumes B11 for every fixture the player does not play. | D2, B11 | — (design work; no reference document) | A domestic league and a two-legged knockout cup run from data with no engine changes. |
| **D4** | **Identity.** The visual pass: custom drawing or a hand-rolled widget layer behind the same door. | D3, C6 | [../PLAN.md](../PLAN.md) §9 Phase 3 | Deliberately unspecified until the screens that survive are known. |

### E. Career depth — the deliberate departures from the original

Everything in this layer is **new design**. The original has no ceilings, no ageing
curves, no academy worth the name, and prices driven by ability. There is no reference
document to trace and no oracle to diff against, so [../LEGACY.md](../LEGACY.md)
describes what these parts *replace*, not what they should do.

All of it is bound by Rule 4 (§6): **none of it reaches the match engine.**

| ID | Part | Depends | Sources | Done when |
|---|---|---|---|---|
| **E1** | **Career player model + projection.** The rich record — age, continuous abilities, per-ability ceilings, the age/peak curve, injury proneness, the retirement-probability curve, the one-season retirement notice, salary, learned positions. Plus the projection down to the engine's 0–7 integers, and the presentation rules kept from the original: the three-letter best-abilities descriptor, and **no numeric attribute is ever shown**. | A5, D2 | [../DATA.md](../DATA.md) §3, [../LEGACY.md](../LEGACY.md) | A career player projects to a legal engine attribute set; `check_walls` proves no career field is reachable from `src/core/` match code. |
| **E2** | **Valuation and wages.** Price from **performance, not ability** — rolling B12 ratings, age, contract length, position scarcity. Salary demands from the same inputs. Career-only. | E1, B12, D3 | — (design work) | Two players with identical projected attributes and different season ratings carry materially different prices and wage demands. |
| **E3** | **Progression, training, injury.** Ageing along the curve; training grounds and staff; assignment and progress feedback; the ceiling rule — **training past a ceiling yields tiny gains and trade-offs** (technique up, pace down; strength up, control down). Position learning: a midfielder becoming a winger. Injury frequency driven by proneness. | E1 | — (design work) | A ten-season headless run produces plausible arcs — rise, peak, decline, retirement — with no ability ever exceeding its ceiling. |
| **E4** | **Academy — a discovery model.** A spawn is **a talent being discovered**, not a youngster enrolling; see below. Level and funding buy discovery *rate* and the quality tail — unlike the original, a discovery is **usually worth having and can occasionally be a superstar**. Vacancy pressure when a retirement leaves no cover. Rotation of the undiscovered. | E3, E6 | — (design work) | Funding measurably shifts both discovery rate and the quality tail over N seasons, and a retirement with no cover reliably produces cover without producing it instantly. |
| **E5** | **Transfers.** The market: buying from clubs and from academies. The reluctance model — a good player at a big club **resists dropping to a smaller one** — priced in prestige, wages and likely playing time, not a flat refusal. | E2, E4 | — (design work) | A top-club player rejects an equal-wage offer from a small club and accepts at some price; the price is legible to the player. |
| **E6** | **Club finances and stadium.** Income from gate receipts and player sales; costs from salaries, upkeep, academy and training funding. Stadium capacity as an upgrade. Human clubs only for upgrades. | E2, D3 | — (design work) | A season's books balance from real inputs, and a capacity upgrade visibly changes gate income the following season. |
| **E7** | **AI club policy.** The deliberate asymmetry, in **one place**: AI clubs hold near-static squads, keep a small rotating academy, never expand academy, training or stadium, and enter the market only to replace a player lost to injury. Retirement and spawning still apply to them. | E4, E5, E6 | — (design work) | Ten AI seasons run with no infrastructure growth, correct retirement/backfill, and injury-driven buying only. |

#### The academy is a discovery model

Stated here because it changes what E4 simulates, not just how it is tuned.

A club's youth setup contains any number of teenagers. **The simulation does not model
them.** A spawn event corresponds to the real-world moment a player is *found* — the
one who turns out to be worth a first-team place — not to the routine intake that
produces nobody in particular. What follows:

- **The academy roster is small by construction**, and it is a list of discoveries,
  not of enrolments. There is no undifferentiated pool to render, cull or age out.
  This is why the AI-club simplification in E7 — a handful of players, rotated — is
  not a simplification at all, but the same model with less money behind it.
- **"Do academy players have useful abilities?" is close to tautological.** The
  distribution is over *how* good a discovery is, not whether it was worth making. The
  original's academies produced filler; this one does not produce filler, it produces
  fewer players.
- **Funding and level buy two separate things**: how often a discovery happens, and
  how heavy the tail is. A well-funded top academy finding a superstar is a rare draw
  from a distribution that only it has access to — keeping those knobs distinct is
  what stops funding from being a single "quality" slider.
- **Vacancy-driven spawning needs care.** "A player retires with no cover, so the
  academy produces one" is a safety net that contradicts the framing — discovery is
  not something a club can do on demand. The coherent version is that a vacancy raises
  search intensity: the club looks harder, so a discovery becomes *likelier and
  sooner*, rather than appearing the moment it is needed. That preserves the model and
  keeps the pressure of an unfilled position real, which is the more interesting game.

#### The projection wall, in detail

The single most important thing in this layer, and the reason it is a layer at all.

The career model wants continuous, drifting, ceilinged abilities with history. The
match engine wants what [../DATA.md](../DATA.md) §3 specifies: integers, 0–7. These
are different models of the same player and **they must never be the same object.**

- **One direction only.** Career → engine, at squad selection. The engine never reads
  a career field and never writes one back. Whatever a career ability does over
  fifteen seasons, the engine sees a legal 0–7 set or the build does not link.
- **Quantise once per match, not per tick.** The engine receives a frozen snapshot at
  kickoff. A player drifting across 5.49/5.51 must not flicker between 5 and 6
  mid-match, and no engine code should ever be tempted to ask for more precision than
  the integer.
- **Career arithmetic is fixed-point too.** Not because the engine needs it — the
  engine never sees it — but because a career resolved from a seed should replay
  identically on both platforms, exactly like a match. Floats here quietly cost that.
- **The loop closes through B12.** Match events → rating → valuation and retirement
  and progression → next season's projected attributes. This is what makes the career
  respond to what actually happened rather than to a number in a table, and it is
  why B12 sits in the engine layer despite affecting no gameplay.
- **B11's cheap backend must feed this loop honestly.** A season resolved by
  `TableResultSimulator` still has to yield per-player ratings, or every career
  outcome silently depends on which fixtures the player chose to watch. This is the
  fidelity flag from B11 earning its place.

---

## 4. Schedule

Seven waves. **A wave ends at its gate, and the gate is a demonstration** — a tagged
commit, a passing test, a recorded match — never a judgement that the work looks
finished. Waves overlap only where the table says so.

**Every gate additionally requires green CI on both platforms** (§6 rule 5). The gates
below state what is new; they all inherit that.

| Wave | Parts | Phase | Gate |
|---|---|---|---|
| **0 — Skeleton** ✔ | A1 | Bootstrap | **Passed.** [../PLAN.md](../PLAN.md) §8 fully ticked on Windows and macOS. |
| **1 — Instrument** ← *current* | A2, A3, **A6**, A4 | Phase 0 | A reference trace and an engine trace load into the viewer and report a divergence tick, and the test harness that will verify every later part runs green in CI on both platforms. Nothing in Wave 2 starts before this. |
| **2 — The ball moves** | B1, B2, B3, B4, B10 | Phase 1 | A human can run a player around a pitch and kick a ball that behaves, with divergence measured in hundreds of ticks. Needs a thin slice of C1 to be watchable — that slice is scheduled here, the rest of C1 is not. |
| **3 — The match** | B5, B6, B7, B8, B9, A5, **B12**, **B11**, **B13** | Phase 1 | Eleven-a-side, full 90 minutes, all restarts, CPU opponent. **Then play it.** If it does not feel right, the wave has not ended — [../PLAN.md](../PLAN.md) §9 is explicit that nothing downstream rescues this. |
| **4 — The game** | C1–C6, D1 | Phase 2 | A match is playable end to end with camera, sprites, sound, replays and substitutions, driven from a real shell. |
| **5 — The career** | D2, D3, **E1**, D4 | Phase 2–3 | A season completes: fixtures, results, table, and a visual pass on the screens that survived. E1 lands here rather than in Wave 6 because D2's save schema has to be built around the rich player record, not retrofitted to it. |
| **6 — The club** | E2, E3, E4, E5, E6, E7 | Phase 4 (new) | Ten seasons run headless and stay coherent: players age, peak, decline and retire; academies replace them; the market moves; the books balance; AI clubs stay static without stagnating into nonsense. Then play a career and see whether any of it is legible from inside the game. |

**Sequencing notes.**

- **A4 is in Wave 1, not Wave 4.** Trace-diffing wants pixel-identical sprites so a
  divergence at tick 340 can be *seen*, not just counted ([../PLAN.md](../PLAN.md)
  §10). The importer is pulled forward for that reason and pays for itself twice.
- **A5 waits until Wave 3.** Tactics data is what B9 consumes; before that, hardcoded
  test formations are enough and are less work to change.
- **C1 is split.** A rectangle-and-dot renderer in Wave 2 is a debugging tool. The
  real one is Wave 4. Do not let the debug view accrete features.
- **B11 rides in Wave 3, not Wave 5 with its consumer.** B9's gate — CPU versus CPU
  playing a full match headless — *is* the engine backend, one wrapper away. Building
  the interface while that is fresh costs almost nothing; retrofitting it once D3 has
  been calling a concrete simulator for a month costs a lot. It also means D3 starts
  against `ScriptedResultSimulator` and never blocks on engine work.
- **The engine targets the reference's Amiga profile at 50 Hz.** Decided in
  [A2-determinism-primitives.md](A2-determinism-primitives.md) §2.4a: at 50 Hz our tick
  *N* is the reference's tick *N*, so A3's diff is an equality test rather than a
  resampling argument, and §6 rule 3 is satisfied natively. It is a gameplay decision,
  not a units conversion — the Amiga build is ~11 % quicker with a markedly heavier
  ball — so A3, B2 and B3 all inherit it, and the PC profile stays implemented behind
  the same switch the reference uses.
- **B6 owns the schedule risk.** It is the mechanic the game is named after and the
  one most likely to be *almost* right. Budget for it to overrun and do not let a
  wave gate hide that.
- **B13 lands before the play-feel gate, not after it.** It changes felt values —
  total aftertouch curl roughly halves, the shot bonuses become signed, and the
  dribble acquires a touch limit. Playing the wave gate against pre-B13 values and
  then changing them means the gate measured a build nobody will ship. It is
  sequenced last within Wave 3 because it needs B6a's structural repairs underneath
  it, and its own §4 R5 batch is the only part that blocks on A3.
- **Wave 6 is the one that can be cut.** Everything before it is a football game.
  Wave 6 is the depth that makes a career worth replaying, and it is also the layer
  most likely to consume unbounded time, because it has no oracle and its acceptance
  criteria are all "does this feel like a career". Shipping without E3–E7 is a real
  option; shipping without E1 is not, which is why E1 sits in Wave 5.
- **The whole E layer must be tunable headless.** Ten seasons at a time, no window,
  no rendering — the same discipline as A3's trace harness, applied to careers instead
  of ticks. Curves, ceilings and spawn distributions cannot be balanced by playing.
- **D3 has no reference document.** It is original design work, and it is the part
  most likely to be got wrong by hardcoding one confederation's rules
  ([../PLAN.md](../PLAN.md) §9 Phase 2). Its subfile is a design document, not a
  transcription.

---

## 5. Subfile template

Each part's subfile answers the same questions in the same order, so a reader who
knows one knows all of them. Mirrors [../EXTRACTION.md](../EXTRACTION.md) §2 for the
reference documents, adapted from *what the original does* to *what we build*.

```
# <ID> — <Part name>

<2-4 lines: what this part delivers, what it explicitly excludes and which part
 covers that instead.>

Depends on: <IDs>   Blocks: <IDs>   Wave: <n>

## 0. One-paragraph version
## 1. Scope
   In / out, as two lists. Out-of-scope items name the part that owns them.
## 2. Design
   The structures and the flow. Our design, from the reference documents —
   never a transcription of reference control flow.
## 3. Interfaces
   What other parts see. The wall this part must not cross.
## 4. Work items
   Ordered, each independently committable, each with its test.
## 5. Tests and acceptance
   Named tests, not "unit tests will be written". Which §7 technique applies,
   which invariants this part adds to the always-on set, what its golden data is
   and how it is regenerated, and the demonstration that closes the part.
   A part with an empty §5 is not planned, it is hoped for.
## 6. Open questions
   Decisions deferred, with who or what resolves them. Unmeasured constants go to
   ../LEGACY.md §15, not here.
```

**Rules carried over from the reference documents:** constants are starting guesses
until traced; the reference port is an oracle and never a source of code
([../PLAN.md](../PLAN.md) §9); a part is finished when its unknowns are enumerated,
not when its knowns are written.

---

## 6. Standing constraints

The three [../PLAN.md](../PLAN.md) §0 rules apply to every part and are not restated
in the subfiles:

1. **`src/core/` knows nothing about the outside world.** No SDL, no ImGui, no I/O,
   no clock, no float, no unseeded randomness. Every B part inherits this.
2. **ImGui lives only in `src/app/ui_imgui/`.** Abstraction is at screen granularity.
   Every C and D part inherits this.
3. **Fixed 50 Hz timestep.** Never retrofitted, never negotiated.

A fourth is added here, because the E layer cannot exist safely without it:

4. **The career model never reaches the match engine.** The engine sees 0–7
   integers, projected once at squad selection, and nothing else — no age, no
   ceiling, no form, no contract, no continuous ability. Whatever the career layer
   grows into over the project's life, the engine's view of a player does not change,
   which is what keeps traces comparable and the match tunable in isolation.

And a fifth, which is a working rule rather than an architectural one:

5. **Nothing is done until it is tested, and the test runs in CI on both platforms.**
   A part whose "done when" was demonstrated by hand once is not done; it is
   undemonstrated as of the next commit. What *tested* means differs sharply by layer
   — §7 says what it means where.

Rules 1–3 are enforced by `tools/check_walls.py`, which must stay wired into the
default build; **E1 extends it to cover rule 4**, and **A6 is rule 5's
infrastructure.** A part that needs an exception has a design problem, not a build
problem.

---

## 7. Testing

The project is unusually testable and should be held to it. A deterministic,
float-free, I/O-free engine behind a hard wall is the ideal case for automated
verification — Rule 1 was worth paying for largely because of this. The techniques
differ by layer, and applying the wrong one is how test suites become expensive and
useless at once.

### What each layer is tested with

| Layer | Technique | Why this one |
|---|---|---|
| **A2** determinism | Golden vectors for fixed-point ops; property tests over the arithmetic; **cross-platform hash equality in CI**. | If this layer is wrong, every test below it is measuring the wrong thing. |
| **B1–B10** engine | Unit tests per subsystem; **golden traces committed to the repo** and diffed every build; invariant assertions run every tick under test; input fuzzing; reference-diff against the A3 corpus. | Golden traces and reference diffs answer different questions — *did we change* versus *are we faithful*. Both are needed and neither substitutes. |
| **B11, B12** | Fixed-seed distribution tests over large N with tolerance bands. Never a fresh random seed per run. | Statistical assertions are the classic source of tests that fail once a fortnight and get deleted. Fix the seed and the distribution test becomes an exact test. |
| **C** presentation | Offscreen render to a surface, hash or compare against committed reference images; headless unit tests for camera and animation maths. | Kit compositing and atlas packing are exactly the code that breaks silently. Camera *feel* is not testable — its arithmetic is. |
| **D** shell | A null `IUiBackend` that replays `Intent` sequences headlessly. | The second real payoff of Rule 2: navigation, save/load and whole career flows become testable with no window and no ImGui. |
| **E** career | Headless multi-season runs with invariants asserted continuously, plus distribution snapshots on the outcomes. | You cannot balance a career by playing one. Ten seasons in a second, asserted, is the only practical instrument. |

### The invariants worth asserting continuously

Cheap, always-on checks under a test build catch more than targeted unit tests do,
because they fire on inputs nobody thought to write a test for:

- **Engine** — player count constant; every position within pitch bounds plus margin;
  state serialises and round-trips losslessly every tick; no unreachable state-machine
  transition; the same seed and inputs produce the same state hash *twice in one
  process and once across two*.
- **Career** — no ability above its ceiling; no player registered to two clubs; squad
  sizes legal; ages monotonic; money conserved across every transaction; no player
  both retired and selected.
- **Both** — no float anywhere in `src/core/` (a wall check, not a test); no unseeded
  randomness reachable from a resolve path.

### Discipline

- **Core tests stay fast and link nothing.** The suite that runs on every save must be
  seconds, or it stops being run. This is what Wall 1 buys.
- **Fixtures are generated, not hand-written**, wherever the data is large — a
  committed 40 KB trace is fine; a hand-maintained one is not.
- **Every bug gets a failing test first.** Especially in B, where "fixed" and "fixed
  for the case I looked at" are indistinguishable without one.
- **Fuzz the importer** (A4) against truncated and malformed originals. It reads
  third-party binary data and is the one place a crash is likely and a silent
  misparse is worse.
- **CI is the gate, not a notification.** Both platforms, every commit, and a red
  build blocks the wave.

---

## 8. Not decided yet

Recorded so they are not silently decided by whoever gets there first:

- **Networking / multiplayer.** Absent from every document. A deterministic
  fixed-step engine with replayable input makes rollback netcode plausible later, but
  nothing is being designed for it now.
- **Platform targets beyond Windows and macOS.** Linux is likely free; nothing is
  being verified.
- **Where the divergence tolerance line sits.** A3 must define it numerically before
  Wave 2 starts, or every B part will argue about it separately.
- **What B11's cheap backend is calibrated *against*.** The engine (internally
  consistent, but inherits every flaw the engine has) or real-world football
  distributions (defensible, but then the engine is the thing out of line). Pick one
  deliberately; the calibration test is meaningless until this is settled.
- **Whether `GOALKEEPER.md` is needed** before B9, pending the audit in
  [../EXTRACTION.md](../EXTRACTION.md) §4.
- **What happens to a discovered player left in the academy.** The brief stopped
  mid-sentence here, and the discovery framing narrows but does not close it: since
  everyone in the academy was worth finding, the question is whether leaving them
  there develops them, stalls them for want of matches, or exposes them to being
  poached. Related, and possibly the more interesting half: **how much of a discovery's
  ceiling is visible at discovery.** If it is fully known, the academy is a lottery you
  read the ticket of; if it emerges over seasons, keeping a player is a judgement call.
  The second fits E1's no-numbers rule better. E4's subfile is blocked on both.
- **Whether B12's rating is one number or several.** A single 1–10 is what the brief
  asks for and what a career screen wants. But valuation (E2), progression (E3) and
  transfer interest (E5) all arguably want *why* the rating was high. Deciding late
  means E2–E5 each invent their own answer.
- **How visible the E layer is to the player.** The original's rule — you never see
  numeric attributes, only the three-letter descriptor — is kept in E1. Whether it
  extends to ceilings, peak age and injury proneness is undecided, and it is the
  difference between a management game about judgement and one about arithmetic.
