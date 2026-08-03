# doc/amiga — the Amiga original

Reference documentation extracted from a **68000 disassembly of the Amiga SWOS
96/97 match module**, held in the sibling repository `original-amiga-swos` as a
single 334 132-line file, `original-amiga-swos.asm`.

This is a *second, independent oracle*. The documents in [../](../) were traced
through the SWOS **DOS port**; these were traced through the **Amiga original**,
which is the game the DOS port was ported from. Where the two agree, a claim is
settled. Where they disagree, the Amiga is the earlier authority and the
disagreement is itself worth recording.

> **Provenance.** The disassembly is IDA output with a substantial hand-annotation
> layer: struct definitions, enum names, and — critically — **named labels on the
> gameplay data tables**. Read this and it says `gravityConstant`, `ballAirConstant`,
> `playerSpeedsGameInProgress`, `kickSpinFactor` in the source, with their values
> beside them. Control flow is read directly and is reliable. Symbol *names* are a
> human's interpretation and are occasionally wrong — §"Label skew" in
> [STATE.md](STATE.md) documents the two places we found. As always: read to
> understand the design, then write our own code. Disassembly-derived work is a
> derivative of a copyrighted binary ([../PLAN.md](../PLAN.md) §9); these documents
> cite line numbers as evidence and never paste instructions.

Source citations use the form `asm:21593`, meaning line 21593 of
`../../original-amiga-swos/original-amiga-swos.asm` relative to this repository.

---

## Why this matters: the constants are here

The single biggest gap in the DOS-port documents is that the physics and tuning
constants live in the port's *data segment* and read as opaque addresses. In this
disassembly they are **literal values with names**. Everything in
[../LEGACY.md](../LEGACY.md) §15 that was flagged "data segment, fitting target"
and belongs to the match engine now has a candidate value.

A representative sample, all read directly from the source:

| What | Symbol | Value | Where |
|---|---|---|---|
| Gravity | `gravityConstant` | 4608 (16.16 → 0.0703 px/frame²) | asm:30615 |
| Ground friction | `ballGroundConstant` | 16 | asm:30582 |
| Air friction | `ballAirConstant` | 10 | asm:30583 |
| Kick launch speed | `ballKickingSpeed` | 2208 | asm:30730 |
| Kick launch rise | `ballKickingDeltaZ` | $14000 (1.25) | asm:30729 |
| Outfield running speed | `playerSpeedsGameInProgress` | 928…1250 by Speed 0–7 | asm:34726 |
| Curl per axis | `kickSpinFactor` | ±32 / ±23 by octant | asm:30746 |
| Curl decay | `spinMultiplierFactor` | 5,4,3,2,2,2,2,1,1,1 | asm:30735 |
| Keeper save reach | `keeperSaveDistance` | 24 | asm:34835 |
| Match length tick rate | `timeDelta` table | 30, 18, 12, 9 | asm:26526 |

That last row is the one to check the derivation against: those four values feed
an accumulator that reproduces SWOS's **3 / 5 / 7 / 10 minute** match-length menu
exactly at 50 Hz, to the frame. See [TIMING.md](TIMING.md) §2. A derivation that
lands on the four numbers a player actually sees in the menu is strong evidence
that the surrounding reading of the clock is right.

---

## The documents

| Document | Covers |
|---|---|
| [SOURCE-MAP.md](SOURCE-MAP.md) | Where everything is. Routine → line number, for every named entry point in the match engine, plus the data-table index. Start here when you need to go look at something. |
| [STATE.md](STATE.md) | `Sprite`, `TeamGeneralInfo`, `PlayerGame` with byte offsets; the player-state and restart-state enums; the coordinate system, the fixed-point conventions, and the two places IDA's field names are skewed. |
| [TIMING.md](TIMING.md) | The 50 Hz frame, the order of the per-frame call chain, the match clock and its accumulator, half-time / full-time / extra time / shootout, and the stoppage state machine. |
| [BALL.md](BALL.md) | Ball physics: the fixed-point integrator, friction, gravity, the bounce, the seven pitch types and their three-constant tables, the dead-ball barrier, and the goal frame with post and crossbar rebounds. |
| [MOVEMENT.md](MOVEMENT.md) | The angle model (256-step heading, 8 octants), the sine and arctangent tables, player speed derivation from the Speed attribute, injury and dribble penalties, and the pitch clamps. |
| [KICKING.md](KICKING.md) | Kick and header launch: the destination-delta idiom, the shot-on-goal geography, the Velocity and Finishing bonus tables, and the two header variants. |
| [PASSING.md](PASSING.md) | The pass in full: target selection, the ray-extension aim, distance-banded strength, the CPU accuracy roll, and the receiver — why he cannot be controlled until he has the ball, and the nine ways that state comes down. |
| [AFTERTOUCH.md](AFTERTOUCH.md) | The ten-frame post-kick window: side latching, the curl tables, the decay ramp, and the frame-4 lob/drive switch. The mechanic this project is named after. |
| [CONTEST.md](CONTEST.md) | Possession: the proximity bands, the dribble touch, the tackle contest and its Tackling+Control skill-difference table, recovery timers, and the foul test. |
| [GOALKEEPER.md](GOALKEEPER.md) | Keeper positioning, the dive decision and its time-to-reach-ball comparison, save distances, and the goal/save resolution table. |
| [AI.md](AI.md) | The 5×7 zonal tactics grid and how a tactic file becomes a destination, the CPU brain's per-frame structure, and the RNG — which is a 256-byte table, not an LCG. |
| [SETPIECES.md](SETPIECES.md) | Out-of-play classification, the restart placement coordinates, the per-restart aiming-delta tables, and the turn-restriction masks. |
| [PLAYERS.md](PLAYERS.md) | The seven player attributes, their packing, and — settled here — **which offset is which skill**. Plus the goalkeeper skill derivation and injury effects. |

---

## What is *not* in this binary

The user asked about score simulation. It is not here, and that is worth stating
plainly rather than leaving as an absence.

This file is the **match module** (`swos4.dk1`/`dk2`/`dku` — asm:12845), the
overlay SWOS loads to play a game. Its string table contains match statistics,
tactics, substitutions, replay and disk I/O, and nothing else: there are no
division names, no fixture or league-table strings, no career vocabulary. A sweep
for routines that call `Rand` three or more times — the shape any goal-generating
simulation must have — returns exactly five, and each one is identifiable:
`InitGame` (asm:23679, pitch and weather selection), `maingame` (asm:30970,
kick-off coin flip), `GameSetup` (asm:41156, celebration length and restart camera),
`sub_106D8C` (asm:17042, the crowd-chant scheduler) and `sub_109FF2` (asm:22825,
crowd reactions). None of them produces a scoreline.

**Conclusion:** simulation of unplayed fixtures lives in the career executable,
which is not in this repository. What *is* here, and is the closest relative, is
the in-match goal/save resolution — a skill-difference lookup against a
16-entry chance table — documented in [GOALKEEPER.md](GOALKEEPER.md) §4. If we
want a result simulator faithful to the original we will have to either find the
career binary or derive one; nothing in this source constrains it.

Also absent, for the same reason: transfers, wages, player ageing, league tables,
and the season calendar. `AdjustPlayerSkills` (asm:102057) *is* present and is
documented in [PLAYERS.md](PLAYERS.md) §3 — it is the bridge that converts a
stored team record into the in-match `PlayerGame` block, so it belongs to the
match module even though it reads career data.

---

## How to use these alongside the DOS-port documents

The DOS-port documents in [../](../) remain the primary reference: they are
written against a decompilation with real control flow and real function names,
they are cross-linked, and they carry the project's confidence-tag vocabulary.
These Amiga documents exist to do three things for them:

1. **Supply values.** Any `[UNKNOWN]` in [../LEGACY.md](../LEGACY.md) §15 that names
   a match-engine constant should be checked here first. Most are answered.
2. **Arbitrate.** [../STATE.md](../STATE.md) records places where the project's two
   existing sources disagree on an offset. This is a third vote.
3. **Catch divergence.** The DOS port is a port. Where its behaviour differs from
   what is written here, that is a real finding about which game we are cloning —
   record it rather than silently preferring one.

They deliberately do *not* cover presentation (sprites, camera, menus, audio),
which the DOS-port documents already handle well and which the disassembly
describes far less legibly than it describes the simulation.

---

## Standing caveat on the numbers

Every constant here is a value read out of a disassembled binary. That makes it a
*measurement of the original*, not a specification we are obliged to match. Two
consequences:

- A value being present does not mean the surrounding *interpretation* is right.
  The chain "this word is 4608" → "therefore gravity is 0.0703 px/frame²" passes
  through an assumption about the fixed-point format and the frame rate. Each
  document states its chain explicitly so the assumption can be attacked.
- Our engine should reproduce the *behaviour*, and use these as the starting point
  for fitting against traces — the same discipline
  [../EXTRACTION.md](../EXTRACTION.md) §1 sets out. They are very good starting
  points. They are not gospel.
