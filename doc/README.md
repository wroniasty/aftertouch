# Documentation index

Reference documentation for **aftertouch**, a from-scratch Sensible World of Soccer
engine. Eighteen documents, ~8,500 lines, in four groups.

Everything mechanical here was traced through the SWOS DOS port in
`../../reference/swos-port/` and its annotated disassembly. That reference is used
**as an oracle, never as a source of code** — disassembly-derived work is a
derivative of a copyrighted binary. Read it to understand behaviour, then write our
own ([PLAN.md](PLAN.md) §9).

---

## Start here

| Document | What it is |
|---|---|
| [PLAN.md](PLAN.md) | The build. Toolchain, directory structure, architectural walls, milestones, and the order to do things in. Read §0 and §9 first. |
| [LEGACY.md](LEGACY.md) | What the original game does, with a confidence tag on every claim. §15 is the measurement checklist — the Phase 0 backlog. |
| [EXTRACTION.md](EXTRACTION.md) | The plan for documenting what is left. Template, procedure, remaining waves. |

---

## The match engine

The simulation, in rough dependency order. Every one of these follows the same
shape: §0 one-paragraph version, the mechanics with source links, a constants
table, what is still unmeasured, and guidance for our implementation.

| Document | Covers |
|---|---|
| [SIMULATION.md](SIMULATION.md) | The frame, the state machine, the clock, out of play, fouls and cards, statistics, ending a period, replays, simulated matches. The spine everything else hangs off. |
| [BALL.md](BALL.md) | Ball physics: friction, gravity, the bounce, the dead-ball barrier, goal-frame collision, and the height-banded landing predictor. Pitch-type physics tables live in §9. |
| [MOVEMENT.md](MOVEMENT.md) | The per-tick player pipeline, speed, eight-way input, pitch boundaries, turn restrictions, and the non-normal movement states. |
| [CONTROL.md](CONTROL.md) | Possession: the proximity bands, dribbling, and losing the ball. |
| [SHOOTING.md](SHOOTING.md) | Tap versus hold, launch speed and height, and the shot-on-goal attribute bonus. |
| [AFTERTOUCH.md](AFTERTOUCH.md) | The post-kick window: lateral curl and the low-drive/high-lob switch. The mechanic the project is named after. |
| [TACKLING.md](TACKLING.md) | The slide, deflected tackles, the foul test, recovery time, and the possession contest. |
| [HEADING.md](HEADING.md) | Static versus jump headers, and the direction switch that selects a driven header or a lob. |
| [SETPIECES.md](SETPIECES.md) | Restart execution: the state enum, foul classification into penalty/free kick/nothing, throw-in placement, and the per-restart aiming tables. |
| [AI.md](AI.md) | Player selection, the zonal off-ball grid, the goalkeeper, the CPU brain, and the RNG. |

---

## Presentation

| Document | Covers |
|---|---|
| [PLAYER_SPRITES.md](PLAYER_SPRITES.md) | Kit compositing, the eight-octant direction model, animation tables, the frame stepper, and depth/height rendering. |
| [CAMERA.md](CAMERA.md) | Five camera modes, the lead-ahead offset that makes the camera anticipate play, easing, and two-stage clipping. |
| [PITCH.md](PITCH.md) | Surface type versus artwork, the seasonal weather table, the career pitch hash, and the tile grid. |
| [REFEREE.md](REFEREE.md) | The referee state machine, card presentation, the blinking shirt number, and sending off. |
| [BENCH.md](BENCH.md) | In-match management: the double-tap gesture, the five bench states, two-phase substitution, and tactics changes. |

---

## Reading paths

**Implementing the match engine** — [PLAN.md](PLAN.md) §9 → [SIMULATION.md](SIMULATION.md)
→ [BALL.md](BALL.md) → [MOVEMENT.md](MOVEMENT.md) → [CONTROL.md](CONTROL.md) →
[SHOOTING.md](SHOOTING.md) → [AFTERTOUCH.md](AFTERTOUCH.md), then the contests
([TACKLING.md](TACKLING.md), [HEADING.md](HEADING.md)) and
[SETPIECES.md](SETPIECES.md), with [AI.md](AI.md) last.

**Trying to make it feel right** — [CAMERA.md](CAMERA.md) §4 (the lead offset),
[BALL.md](BALL.md) §8 (why defenders misread high balls),
[TACKLING.md](TACKLING.md) §8 (why contests are near coin flips),
[HEADING.md](HEADING.md) §6 (the one attribute that really matters).

**Chasing a specific number** — [LEGACY.md](LEGACY.md) §15 lists everything
unmeasured. Each engine document's "what still needs measurement" section feeds it.

---

## Conventions

**Confidence tags** — `[DATA]`, `[COMMUNITY]`, `[DISPUTED]`, `[UNKNOWN]`, defined in
[LEGACY.md](LEGACY.md). Claims read directly off the reference's control flow carry
no tag; the source link is the evidence.

**Constants are starting guesses.** Every document says so in its provenance block.
Some are real literal values recovered from the port's source — notably the ball
physics tables in [BALL.md](BALL.md) §9, the tackle tables in
[TACKLING.md](TACKLING.md) §9 and the heading curve in [HEADING.md](HEADING.md) §6 —
but even those are the porters' transcription and want confirming against traces
([LEGACY.md](LEGACY.md) §17).

**Shared coordinate conventions**, established across several documents and worth
knowing before reading any of them:

- **Octants** are `0 = N` and clockwise: N, NE, E, SE, S, SW, W, NW. Confirmed
  independently in [SETPIECES.md](SETPIECES.md) §2 (turn-flag bitmask),
  [CAMERA.md](CAMERA.md) §5 (direction vector table) and [BALL.md](BALL.md) §2.
- **Positions are 16.16 fixed point**; the integer part is the pitch coordinate, and
  every boundary test in the engine uses only that ([BALL.md](BALL.md) §1).
- **The pitch is 672 × 848** whole units, centre `(336, 449)`
  ([PITCH.md](PITCH.md) §4).
- **Everything aims at a destination.** Collisions, deflections and reflections are
  implemented by rewriting `destX`/`destY`, not by negating velocity
  ([BALL.md](BALL.md) §5). This is the single most important structural idea in the
  engine.

---

## Not yet written

Tracked in [EXTRACTION.md](EXTRACTION.md) §4. The remaining gaps are `STATE.md`
(a consolidated struct/offset map), `DATA.md` (file formats), and the shell
subsystems — rendering, audio, input devices, replays and menus — which are Phase
2–3 work and deliberately deferred.

Two known debts inside the current set: the open questions in each document have
**not** yet been merged into [LEGACY.md](LEGACY.md) §15, and cross-links from the
older documents back to the newer ones are incomplete — [MOVEMENT.md](MOVEMENT.md)
§10 and [CONTROL.md](CONTROL.md) §6 still describe the tackle contest as
undocumented, though [TACKLING.md](TACKLING.md) now covers it.
