# EXTRACTION.md

The plan for extracting the remaining reference documents from the SWOS DOS port in
[../reference/swos-port/](../reference/swos-port/). Companion to [PLAN.md](PLAN.md)
(what we build) and [LEGACY.md](LEGACY.md) (what the original does).

Eight documents exist. Roughly sixteen do not. This says which, in what order, to
what standard, and when to stop.

---

## 0. One-paragraph version

Documentation is the **Phase 0 deliverable** of [PLAN.md](PLAN.md) §9 — the reading
half of the trace harness. Each document converts one subsystem of the reference
port from 4000 lines of decompiled control flow into a page you can code against,
and ends by handing a list of unmeasured numbers to [LEGACY.md](LEGACY.md) §15.
The order is not "biggest file first" — it is **whatever Phase 1 blocks on**: ball
physics, then the two contests (tackle, header), then set pieces. Presentation and
shell subsystems wait for Phases 2–3 and are cheap to write when we get there.

---

## 1. Principles

**A document is finished when its unknowns are enumerated, not when its knowns are
written.** The `[UNKNOWN]` list at the end is the product. Prose that explains
structure with no measurement checklist is a summary, not a reference.

**Structure is reliable, constants are not.** The decompiled control flow can be
read with confidence. Numbers living in the original data segment cannot — every
one is a fitting target. Every document repeats this in its provenance block and
never lets a raw constant read as fact.

**Read to understand, then write our own.** Disassembly-derived work is a
derivative of a copyrighted binary ([PLAN.md](PLAN.md) §9). Documents cite line
numbers as an oracle; they never paste code.

**One subsystem per document.** The existing eight hold because each answers one
question. Resist a `MISC.md`. If a topic is three paragraphs, it is a section in a
neighbouring document, not a file.

**Every document is cross-linked both ways.** A new document that references
[MOVEMENT.md](MOVEMENT.md) also gets a link added *in* MOVEMENT.md. Deferred
promises get closed — see §6.

---

## 2. The template

Codified from [SHOOTING.md](SHOOTING.md), [CONTROL.md](CONTROL.md) and
[AFTERTOUCH.md](AFTERTOUCH.md), which are the cleanest examples. Follow it so a
reader who knows one document knows all of them.

```
# NAME.md

<2-5 line purpose: what question this answers, what it explicitly excludes and
 which document covers that instead, what it was traced through.>

> **Provenance.** <What survives as decompiled code and is therefore reliable.
>  What lives in the data segment and is therefore a fitting target per
>  LEGACY.md §15. Read to understand the design; write our own code.>

---

## 0. One-paragraph version
<The whole subsystem, dense, no hedging. A reader who stops here should be able
 to sketch the system correctly.>

## 1..N. The mechanics
<Ordered by data flow, not by source file layout. Every claim links to
 ../reference/swos-port/... at line granularity. Tables for anything with more
 than three cases. Struct fields cited as `name` (+offset).>

## N+1. Constants quick reference
<One table: symbol, value or "data segment", meaning. Values are starting
 guesses, and the table says so.>

## N+2. What this resolves, and what still needs measurement
<Two lists. "Confirmed as structure" with ✓ marks. "Open (measurement targets,
 LEGACY.md §15)" — these get copied into LEGACY §15 in the same commit.>

## N+3. Guidance for the reimplementation
<Bulleted design directives for our engine. Not a summary — decisions. What to
 model, what to keep deterministic inside the at_core tick, what to resist
 building until a trace demands it.>
```

**Confidence tags** (`[DATA]`, `[COMMUNITY]`, `[DISPUTED]`, `[UNKNOWN]`) carry over
from [LEGACY.md](LEGACY.md) wherever a claim is not straight from the port's source.
Claims read directly off decompiled control flow need no tag — the line link is the
evidence.

---

## 3. Procedure per document

Repeatable, roughly half a day to two days each depending on tier.

1. **Bound it.** List the source files and line ranges in scope. Write the
   exclusion sentence for the purpose paragraph first — it prevents the document
   sprawling into its neighbours.
2. **Map the entry points.** Find the per-tick entry and follow it. The reference
   port's control flow is the spine; the document's section order should be that
   spine, not the file order.
3. **Pull the struct fields.** Every field the subsystem reads or writes, with
   offsets, from [swos.h](../reference/swos-port/src/swos/swos.h) and
   [Sprite.h](../reference/swos-port/src/sprites/Sprite.h). This becomes a table
   early in the document and feeds `STATE.md` (§5).
4. **Separate structure from constants.** Two passes. Control flow, then the
   number tables. Anything whose value is not visible in the source gets logged as
   "data segment" and becomes a §15 item.
5. **Reconcile against LEGACY.md.** Where the port contradicts community folklore,
   say so explicitly and mark it `[DISPUTED]`. These reconciliations are among the
   most valuable paragraphs in the existing eight.
6. **Write the guidance section last**, once you know what the subsystem actually
   is. It is the only section written as an engine author rather than an
   archaeologist.
7. **Close the loop.** Add the open questions to [LEGACY.md](LEGACY.md) §15, add
   the trace scenarios the new unknowns require, add the backlinks (§6), add the
   index row (§5).

---

## 4. The documents

Line counts are the reference source in scope. Estimates are target document
length, calibrated against the existing eight (AFTERTOUCH.md is 296 lines over
~650 lines of `ball.cpp`; AI.md is 725 lines over a much larger asm surface).

### Wave 1 — blocks Phase 1 of PLAN.md

These four are prerequisites for the match engine. Nothing in Waves 2–3 should
start before all four land.

| Doc | Scope | Primary sources | Est. |
|---|---|---|---|
| **BALL.md** | `updateBall` tick order, `calculateNextBallPosition`, gravity and `deltaZ` decay, ground friction, bounce restitution, boundary reflection, deflection off players, goal-frame collision, `getBallDestCoordinatesTable` | [ball.cpp](../reference/swos-port/src/game/ball/ball.cpp) :13, :4029–4583 (excl. 2248–2900, 3007) | ~400 |
| **TACKLING.md** | Slide vs running tackle, `tacklingTimer` lifecycle, reach and recovery, the possession-transfer contest and its RNG, `headerOrTackle` / `lastHeadingTacklingPlayer` bookkeeping | `playerBeginTackling`, `playerTackled`, `playerTacklingTestFoul`, `playersTackledTheBallStrong` in [updatePlayers.cpp](../reference/swos-port/src/game/updatePlayers/updatePlayers.cpp) | ~280 |
| **HEADING.md** | Jump vs standing header, trigger z-window, jump arc, resulting ball velocity and direction, Heading attribute's role, interaction with aftertouch | `kJumpHeader` / `kStaticHeader` paths in updatePlayers.cpp; [ball.cpp](../reference/swos-port/src/game/ball/ball.cpp) header branch | ~250 |
| **SETPIECES.md** | Taker selection, throw-in release, corner delivery and box occupancy, free-kick wall assembly, penalty run-up and keeper dive, kick-off shape, goal kick and keeper distribution | updatePlayers.cpp restart paths; [gameLoop.cpp](../reference/swos-port/src/game/gameLoop.cpp) | ~350 |

**Dependencies.** BALL.md first — TACKLING and HEADING both describe events that
write ball state, and SETPIECES describes launches. Write BALL.md, then the other
three can go in parallel.

**LEGACY §15 items these must close:** the entire **Ball** block (gravity,
restitution and rolling friction per pitch type, capture radius, dribble kick
distance) and the entire **Contests** block (slide reach/duration/recovery,
running tackle resolution, header trigger range and arc, deflection rules).

### Wave 2 — shapes match feel

Written during Phase 1 iteration, when the match exists and feels wrong.

| Doc | Scope | Primary sources | Est. |
|---|---|---|---|
| **CAMERA.md** | Five modes (standard, bench, leaving-bench, booking, penalty shootout), lead/lookahead, three-stage clipping, fixed-point movement, zoom coupling | [camera.cpp](../reference/swos-port/src/game/camera.cpp) (390) | ~220 |
| **PITCH.md** | Pitch types and numbers, 55×42 tile matrix, animated patterns, zoom system, surface→physics coupling | [pitch.cpp](../reference/swos-port/src/game/pitch/pitch.cpp) (431), `pitchConstants.h` | ~200 |
| **BENCH.md** | Substitution state machine, in-match tactics changes, going-to-bench delay, camera hand-off, bench UI | [bench/](../reference/swos-port/src/game/bench/) (~1500) | ~280 |
| **REFEREE.md** | Activation, state machine, card handing and the booked-number sprite, `sendPlayerAway`, animation table, leaving state | [referee.cpp](../reference/swos-port/src/game/referee.cpp) (287) | ~160 |

**Also in Wave 2, as a section not a file:** the Amiga-vs-DOS divergence in
[amigaMode.cpp](../reference/swos-port/src/game/amigaMode.cpp) (81 lines — the
direction-flip ban). Add to [MOVEMENT.md](MOVEMENT.md). It is the concrete form of
the choice [LEGACY.md](LEGACY.md) §1 forces, and it deserves to sit next to the
movement rules it modifies.

### Wave 3 — shell, presentation, data

Phase 2–3 of [PLAN.md](PLAN.md). Cheap to write, low risk, no reason to do early.

| Doc | Scope | Primary sources | Est. |
|---|---|---|---|
| **STATE.md** | The global state map: `TeamGeneralInfo` (1704 B), `Sprite`, `PlayerGameHeader`, offsets currently cited ad hoc across eight documents | [swos.h](../reference/swos-port/src/swos/swos.h), `SwosPointer.h`, `docs/contiguous-variables.txt` | ~250 |
| **DATA.md** | Team/player database layout, `.TAC` tactics encoding and the 35-cell grid, attribute packing, pitch files, competition data | file formats; [LEGACY.md](LEGACY.md) §9–10 give meaning, this gives layout | ~300 |
| **RENDERING.md** | Atlas pipeline, three resolutions, layer colorization, palette, depth sort, overlay, window modes | [src/video/](../reference/swos-port/src/video/), `renderSprites.cpp`, `docs/rendering.txt` | ~280 |
| **AUDIO.md** | Event-driven commentary queue, custom/zip commentary packs, crowd chant selection by match state, music, sfx | [comments.cpp](../reference/swos-port/src/audio/comments.cpp) (562), [chants.cpp](../reference/swos-port/src/audio/chants.cpp) (358), music, sfx | ~220 |
| **INPUT.md** | Device layer: Joypad/JoypadConfig/VirtualJoypad, keyboard scancodes and KeyConfig, per-match control assignment, the virtual-joypad abstraction the AI shares | [controls/](../reference/swos-port/src/controls/) (~3500) | ~250 |
| **REPLAYS.md** | Storage format, HIL2 highlight format, scene buffer, playback | [replays/](../reference/swos-port/src/replays/), `docs/replays.txt`, `docs/highlights.txt` | ~200 |
| **MENUS.md** | Menu engine, `.mnu` format, `mnu2h`, menu codes | [menus/engine/](../reference/swos-port/src/menus/engine/) | ~180 |

**STATE.md is the exception to "Wave 3 waits".** It is pure consolidation of
offsets the existing documents already cite, it gets denser and more useful with
every Wave 1 document, and it removes the per-document struct table duplication.
Write it **at the end of Wave 1**, from what Wave 1 produced.

### Audit, not extraction

- **Goalkeeper actions.** [AI.md](AI.md) §4 is ~160 lines on the keeper. Whether it
  covers *actions* — dive, catch, punch, distribution — as opposed to positioning,
  is unverified. Read it before deciding whether a `GOALKEEPER.md` is warranted or
  whether AI.md §4 just needs extending. Half an hour, do it before Wave 1 ends.

---

## 5. Cross-cutting deliverables

**`doc/README.md` — the index.** Nine documents and no table of contents. One row
per document: name, one-line scope, which phase it serves. Written now, updated in
the same commit as each new document. This is the cheapest high-value item in the
plan.

**LEGACY.md §15 growth.** The checklist is the Phase 0 backlog and every new
document feeds it. Each extraction commit touches two files minimum: the new
document and §15.

**Trace corpus scenarios.** [PLAN.md](PLAN.md) §9 Phase 0 step 5 lists the
recording corpus. Waves 1–2 will add scenarios — set-piece variants, header
duels, tackle contests at differing attribute spreads, camera behaviour at pitch
edges. Append to that list as each document reveals what needs measuring.

---

## 6. Debts to close

Existing documents make promises. Closing them is part of the relevant extraction,
not a separate task.

| Promise | Closed by |
|---|---|
| [MOVEMENT.md](MOVEMENT.md):537 — *"Full resolution belongs in a tackle document, not here"* | TACKLING.md |
| [CONTROL.md](CONTROL.md):183 — running-tackle/slide contest listed as open | TACKLING.md |
| Heading fragments scattered across MOVEMENT §9, [AI.md](AI.md) §5.7, PLAYER_SPRITES | HEADING.md, with backlinks replacing the fragments |
| [SHOOTING.md](SHOOTING.md) §7 — *"The `getBallDestCoordinatesTable` offsets per game state"* | BALL.md |
| Ball-physics assumptions in AFTERTOUCH/SHOOTING/CONTROL with no document behind them | BALL.md |

---

## 7. Definition of done, per document

- [ ] Purpose paragraph states what the document excludes and links where that lives
- [ ] Provenance block distinguishes reliable structure from data-segment constants
- [ ] §0 one-paragraph version is complete enough to sketch the system from
- [ ] Every mechanical claim links to reference source at line granularity
- [ ] Struct fields cited with offsets
- [ ] Constants table present, and says its values are starting guesses
- [ ] Open-questions section written, and its items copied into LEGACY.md §15
- [ ] Guidance section gives decisions, not a summary
- [ ] Backlinks added in every document referenced
- [ ] Row added to `doc/README.md`
- [ ] No code copied from the reference port

---

## 8. Sequencing

```
now         doc/README.md  (index)
            AI.md §4 audit  (goalkeeper: extend or split?)

Wave 1      BALL.md
            ├── TACKLING.md   ┐
            ├── HEADING.md    ├─ parallel after BALL.md
            └── SETPIECES.md  ┘
            STATE.md  (consolidates what Wave 1 produced)
                                        → Phase 1 unblocked

Wave 2      CAMERA.md, PITCH.md, BENCH.md, REFEREE.md
            MOVEMENT.md += Amiga mode section
                                        → during Phase 1 iteration

Wave 3      DATA.md, RENDERING.md, AUDIO.md, INPUT.md,
            REPLAYS.md, MENUS.md
                                        → Phases 2-3
```

**Stop rule.** Wave 3 documents are reference material for work that has not
started. If Phase 1 is going badly, Wave 3 is not what is wrong. Writing them early
is the same mistake as starting with the management UI ([PLAN.md](PLAN.md) §9).
