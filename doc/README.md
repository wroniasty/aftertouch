# Documentation index

Reference documentation for **aftertouch**, a from-scratch Sensible World of Soccer
engine. Twenty-four documents, ~11,000 lines, in five groups.

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
| [implementation/PLAN.md](implementation/PLAN.md) | The work breakdown: thirty-five named parts across five layers, their dependencies, the seven-wave schedule, and the per-layer testing standard. Also where the departures from the original are recorded — the career layer and its wall. Each part is filled out in a subfile alongside it. |

---

## The match engine

The simulation, in rough dependency order. Every one of these follows the same
shape: §0 one-paragraph version, the mechanics with source links, a constants
table, what is still unmeasured, and guidance for our implementation.

| Document | Covers |
|---|---|
| [SIMULATION.md](SIMULATION.md) | The frame, the state machine, the clock, out of play, fouls and cards, statistics, ending a period, replays, simulated matches. The spine everything else hangs off. |
| [BALL.md](BALL.md) | Ball physics: friction, gravity, the bounce, the dead-ball barrier, goal-frame collision, and the height-banded landing predictor. Pitch-type physics tables in §9. |
| [MOVEMENT.md](MOVEMENT.md) | The per-tick player pipeline, speed, eight-way input, pitch boundaries, turn restrictions, and the non-normal movement states. |
| [CONTROL.md](CONTROL.md) | Possession: the proximity bands, dribbling, and losing the ball. |
| [SHOOTING.md](SHOOTING.md) | Tap versus hold, launch speed and height, and the shot-on-goal attribute bonus. |
| [AFTERTOUCH.md](AFTERTOUCH.md) | The post-kick window: lateral curl and the low-drive/high-lob switch. The mechanic the project is named after. |
| [TACKLING.md](TACKLING.md) | The slide, deflected tackles, the foul test, recovery time, and the possession contest. |
| [HEADING.md](HEADING.md) | Static versus jump headers, and the direction switch that selects a driven header or a lob. |
| [SETPIECES.md](SETPIECES.md) | Restart execution: the state enum, foul classification into penalty/free kick/nothing, throw-in placement, and the per-restart aiming tables. |
| [AI.md](AI.md) | Player selection, the zonal off-ball grid, the goalkeeper, the CPU brain, and the RNG. |
| [STATE.md](STATE.md) | The struct and offset map underneath all of the above. Where `Sprite +44` and `TeamGeneralInfo +118` are defined, and where the two reference sources disagree. |

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

## Reference only

**We are building our own engine and our own UI, so these five are unlikely to
serve as an implementation basis.** They are documented because knowing what the
original did is useful anyway — and because each contains one or two things that
*do* carry across, noted below.

| Document | Covers | Worth taking |
|---|---|---|
| [DATA.md](DATA.md) | Team, player, tactics, career and replay file formats. | **§3: attributes are 4-bit nibbles, range 0–15.** This settles an open question in three engine documents and means four attribute-indexed tables are undersized. Also §4's tactics format, which is a very good model. |
| [RENDERING.md](RENDERING.md) | 320×200 logical space, atlas pipeline, kit colourisation, the 256-colour palette. | §4's grey-scale layer model for kits, and §5's kit-colour → palette table if we ever render authentic kits. |
| [AUDIO.md](AUDIO.md) | Commentary queue, the 28 event categories, crowd chants, modding. | §2's event taxonomy — a specification for what a football game should notice — and §4's scoreline-driven crowd. |
| [INPUT.md](INPUT.md) | Device layer, the two-level event model, configuration. | §4's one-team-per-frame alternation, which has real gameplay consequences, and §1's seven-field interface shared by human and AI. |
| [MENUS.md](MENUS.md) | Binary menu format, the entry model, the `mnu2h` authoring language. | §5's relative-layout cursor and §1's observation that eight content kinds covered the whole game. |

---

## Reading paths

**Implementing the match engine** — [PLAN.md](PLAN.md) §9 → [SIMULATION.md](SIMULATION.md)
→ [BALL.md](BALL.md) → [MOVEMENT.md](MOVEMENT.md) → [CONTROL.md](CONTROL.md) →
[SHOOTING.md](SHOOTING.md) → [AFTERTOUCH.md](AFTERTOUCH.md), then the contests
([TACKLING.md](TACKLING.md), [HEADING.md](HEADING.md)) and
[SETPIECES.md](SETPIECES.md), with [AI.md](AI.md) last.
[STATE.md](STATE.md) is a lookup table to keep open alongside.

**Trying to make it feel right** — [CAMERA.md](CAMERA.md) §4 (the lead offset),
[BALL.md](BALL.md) §8 (why defenders misread high balls),
[TACKLING.md](TACKLING.md) §8 (why contests are near coin flips),
[HEADING.md](HEADING.md) §6 (the one attribute that really matters),
[INPUT.md](INPUT.md) §4 (why input latency is two ticks, not one).

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

**Two reference sources, and they disagree.** The IDA-derived structs in
`swos.asm` are authoritative; the C++ mirrors in `swos.h` are more readable but
contain at least one misnumbering. [STATE.md](STATE.md) §4 records every known
discrepancy.

**Shared coordinate conventions**, established across several documents and worth
knowing before reading any of them:

- **Octants** are `0 = N` and clockwise: N, NE, E, SE, S, SW, W, NW. Confirmed
  independently in [SETPIECES.md](SETPIECES.md) §2 (turn-flag bitmask),
  [CAMERA.md](CAMERA.md) §5 (direction vector table) and [BALL.md](BALL.md) §2.
- **Positions are 16.16 fixed point**; the integer part is the pitch coordinate, and
  every boundary test in the engine uses only that ([BALL.md](BALL.md) §1).
  Destinations are whole units.
- **The pitch is 672 × 848** whole units, centre `(336, 449)`
  ([PITCH.md](PITCH.md) §4). Menus and HUD are a separate 320 × 200 space
  ([RENDERING.md](RENDERING.md) §1).
- **Attributes are 0–15** ([DATA.md](DATA.md) §3), packed as nibbles on disk.
- **Everything aims at a destination.** Collisions, deflections and reflections are
  implemented by rewriting `destX`/`destY`, not by negating velocity
  ([BALL.md](BALL.md) §5). This is the single most important structural idea in the
  engine.

---

## Known debts

- **Open questions have not been merged into [LEGACY.md](LEGACY.md) §15.** Each
  document carries its own list; the central checklist has not grown to match.
- **Cross-links from the older documents are incomplete.**
  [MOVEMENT.md](MOVEMENT.md) §10 and [CONTROL.md](CONTROL.md) §6 still describe the
  tackle contest as undocumented, though [TACKLING.md](TACKLING.md) now covers it.
  The attribute-range question is now answered in [DATA.md](DATA.md) §3 but still
  listed as open in [HEADING.md](HEADING.md) §8, [TACKLING.md](TACKLING.md) §10 and
  [STATE.md](STATE.md) §9.
- **Not yet written**, per [EXTRACTION.md](EXTRACTION.md) §4: `GOALKEEPER.md` —
  pending an audit of whether [AI.md](AI.md) §4 already covers keeper *actions* as
  opposed to positioning.
