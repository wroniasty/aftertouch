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
| [AMIGA_CHANGES.md](AMIGA_CHANGES.md) | What the Amiga oracle changed. Every value filled in, every claim corrected, every disagreement opened — with where to look. Read this before trusting a constant. |
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
| [DATA.md](DATA.md) | Team, player, tactics, career and replay file formats. | **§3: attributes are 4-bit nibbles but the used range is 0–7** — the high bit is masked off on load, so every eight-entry table in the engine is correctly sized. (§3 previously concluded 0–15 and that four tables were undersized; that is corrected.) Also §4's tactics format, which is a very good model. |
| [RENDERING.md](RENDERING.md) | 320×200 logical space, atlas pipeline, kit colourisation, the 256-colour palette. | §4's grey-scale layer model for kits, and §5's kit-colour → palette table if we ever render authentic kits. |
| [AUDIO.md](AUDIO.md) | Commentary queue, the 28 event categories, crowd chants, modding. | §2's event taxonomy — a specification for what a football game should notice — and §4's scoreline-driven crowd. |
| [INPUT.md](INPUT.md) | Device layer, the two-level event model, configuration. | §4's one-team-per-frame alternation, which has real gameplay consequences, and §1's seven-field interface shared by human and AI. |
| [MENUS.md](MENUS.md) | Binary menu format, the entry model, the `mnu2h` authoring language. | §5's relative-layout cursor and §1's observation that eight content kinds covered the whole game. |

---

## A second oracle: the Amiga original

| Directory | What it is |
|---|---|
| [amiga/](amiga/) | Fourteen documents traced through a 68000 disassembly of the **Amiga SWOS 96/97 match module**, the game the DOS port was ported from. |

Everything above was traced through the DOS port. The Amiga set is independent, and
it is worth reading for one specific reason: **the physics and tuning constants are
present as named literal values** rather than as data-segment addresses. Gravity,
friction, the bounce factors, the player speed tables, the curl tables, the tackle
odds and the goal/save chance table are all recovered exactly. Most of what
[LEGACY.md](LEGACY.md) §15 flags as an unmeasured match-engine number now has a
candidate value — see [amiga/README.md](amiga/README.md) for the summary and the
per-subsystem documents for the derivations.

**Every applicable document above now carries an "Amiga cross-check" section** —
usually the last one — recording what the second oracle confirms, what it corrects,
and where the two disagree. [AMIGA_CHANGES.md](AMIGA_CHANGES.md) is the ledger of all
of it in one place.

The headline results:

- **Attribute range settled at 0–7.** [DATA.md](DATA.md) §3 previously read the file
  format's 4-bit nibbles as a 0–15 range and concluded four attribute-indexed tables
  were undersized. The Amiga engine masks every nibble with `7` on load and clamps to
  7, so the used range is 0–7 and eight-entry tables are correct. Corrected in
  DATA.md §3, [STATE.md](STATE.md) §5, [HEADING.md](HEADING.md) §6 and
  [TACKLING.md](TACKLING.md) §10.
- **The Heading attribute table was mis-read.** [HEADING.md](HEADING.md) §6 had it as
  thirteen entries running to +2569, and concluded Heading was the strongest
  attribute effect in the game. It has **eight** entries running `−336 … 0` — a pure
  handicap ramp with no upside. See [HEADING.md](HEADING.md) §10.
- **Attribute identity confirmed.** The skill block is `P V H T C S F`, fixed from
  read sites, with `shooting` at +28 meaning **Velocity**.
- **The passing system is a subsystem in its own right**, and none of the documents
  above covers it. [amiga/PASSING.md](amiga/PASSING.md) is new: target selection by a
  ±22.5° cone anchored at the **ball**, aim by extending the ball→receiver ray until
  it leaves the pitch, strength banded by **distance** plus a Passing bonus, a CPU-only
  accuracy roll, and the receiver state — he steps 90° into the path of an off-target
  pass, and is **excluded from control selection until the ball reaches his feet**.
  Passing *is* read in-match, twice, correcting an earlier claim here that no reader
  was found.
- **A whole shot-resolution stage is missing from our reading.** The Amiga decides
  goal-or-save from a Finishing-minus-goalieSkill lookup *before* the keeper decides
  whether to dive. Nothing in [AI.md](AI.md) §4 corresponds to it. See
  [AI.md](AI.md) §10.
- **Six live disagreements** are recorded rather than arbitrated, listed in
  [AMIGA_CHANGES.md](AMIGA_CHANGES.md) and in [EXTRACTION.md](EXTRACTION.md) §6.

The Amiga set deliberately does not cover presentation; the documents above remain
the reference for rendering, camera, audio and menus.

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
[AFTERTOUCH.md](AFTERTOUCH.md) §11 (why over half the curl lands in three ticks),
[CONTROL.md](CONTROL.md) §8 (why Ball Control is the attribute that really matters),
[INPUT.md](INPUT.md) §4 (why input latency may be two ticks, not one).

**Chasing a specific number** — [LEGACY.md](LEGACY.md) §15 lists everything
unmeasured. Each engine document's "what still needs measurement" section feeds it.

---

## Conventions

**Confidence tags** — `[DATA]`, `[COMMUNITY]`, `[DISPUTED]`, `[UNKNOWN]`, defined in
[LEGACY.md](LEGACY.md). Claims read directly off the reference's control flow carry
no tag; the source link is the evidence.

**Constants are starting guesses.** Every document says so in its provenance block.
Many are now real literal values, recovered from the port's source *and*
independently from the Amiga original's data segment, where they carry descriptive
names — the ball physics tables, the player speed tables, the curl tables, the
tackle odds. Where both oracles agree on a number, it is as close to settled as this
project gets; where only one has it, it is still one transcription
([LEGACY.md](LEGACY.md) §17).

**Three reference sources now, and they disagree.** Within the port, the IDA-derived
structs in `swos.asm` are authoritative and the C++ mirrors in `swos.h` contain at
least one misnumbering — [STATE.md](STATE.md) §4 records those. The Amiga
disassembly is a third vote that settles several of them, and opens a few of its own;
[STATE.md](STATE.md) §11 records those. Note that the Amiga's own IDA struct is
misleading in two places its documents flag explicitly.

**Shared coordinate conventions**, established across several documents and worth
knowing before reading any of them:

- **Octants** are `0 = N` and clockwise: N, NE, E, SE, S, SW, W, NW. Confirmed
  independently in [SETPIECES.md](SETPIECES.md) §2 (turn-flag bitmask),
  [CAMERA.md](CAMERA.md) §5 (direction vector table) and [BALL.md](BALL.md) §2.
- **Positions are 16.16 fixed point**; the integer part is the pitch coordinate, and
  every boundary test in the engine uses only that ([BALL.md](BALL.md) §1).
  Destinations are whole units.
- **`speed` is a separate integer scalar**, not a vector, in units of ~1/512 unit per
  tick — so 512 ≈ 1 unit/tick. The velocity vector is re-derived from speed and the
  aim point every tick and is never authoritative ([STATE.md](STATE.md) §11).
- **The pitch is 672 × 848** whole units, centre `(336, 449)`
  ([PITCH.md](PITCH.md) §4), with the *playable* area 510 × 641 at origin (81, 129).
  Menus and HUD are a separate 320 × 200 space ([RENDERING.md](RENDERING.md) §1).
- **Attributes are 0–7** ([DATA.md](DATA.md) §3). They are stored as 4-bit nibbles on
  disk, but the high bit is masked off on load, so every attribute-indexed table in
  the engine has exactly eight entries.
- **The Amiga simulation runs at a fixed 50 Hz**, one tick per vertical blank, no
  sub-stepping and no interpolation ([SIMULATION.md](SIMULATION.md) §14). Every
  per-tick constant in these documents is per-frame at that rate.
- **Everything aims at a destination.** Collisions, deflections and reflections are
  implemented by rewriting `destX`/`destY`, not by negating velocity
  ([BALL.md](BALL.md) §5). This is the single most important structural idea in the
  engine.

---

## Known debts

- **Open questions have not been fully merged into [LEGACY.md](LEGACY.md) §15.** The
  §15 checklist has now been reconciled against the Amiga findings — most items are
  struck through with a candidate value — but each document still carries its own
  list and the two are not yet one thing.
- **Six live disagreements between the two oracles**, listed in
  [AMIGA_CHANGES.md](AMIGA_CHANGES.md) and [EXTRACTION.md](EXTRACTION.md) §6. Each
  is a real question about which game we are cloning, and each is cheap to settle
  with one targeted trace. The two that most affect feel are the **foul-from-behind
  test** and the **aftertouch side latch** — both come out as exact complements in
  the two readings.
- **The one-team-per-frame alternation is single-sourced.** It doubles input latency
  and halves the player-selection rate, and nothing in the Amiga reading confirms it
  ([INPUT.md](INPUT.md) §4). Highest-value open item in the corpus.
- **Cross-links from the older documents are incomplete.**
  [MOVEMENT.md](MOVEMENT.md) §10 and [CONTROL.md](CONTROL.md) §6 still describe the
  tackle contest as undocumented, though [TACKLING.md](TACKLING.md) now covers it.
- ~~**Not yet written**: `GOALKEEPER.md`~~ — the audit is done and the answer came
  from the other oracle: [amiga/GOALKEEPER.md](amiga/GOALKEEPER.md) exists, and what
  [AI.md](AI.md) §4 is missing is not positioning but the goal-versus-save
  resolution stage ([AI.md](AI.md) §10). The follow-up is a search of the port for
  that stage, not a new document.
