# STATE.md

The in-memory state map: every struct the match engine reads and writes, with real
offsets, consolidated from the offsets that the other documents cite piecemeal.
Traced through the reference DOS port in
[../reference/swos-port/](../reference/swos-port/) and its annotated disassembly.

This is a **lookup table, not a narrative**. Every other engine document cites
fields as `name (+offset)`; this is where those offsets are defined once, checked
against each other, and where the two sources disagree is recorded.

> **Provenance.** Two independent descriptions of the same memory exist and this
> document reconciles them. The **IDA-derived structs in
> [swos.asm](../reference/swos-port/swos/swos.asm) are authoritative** — they carry
> real offsets, real sizes (`sizeof=0x91`) and cross-references to every read and
> write site in the binary. The C++ mirrors in
> [swos.h](../reference/swos-port/src/swos/swos.h) and
> [Sprite.h](../reference/swos-port/src/sprites/Sprite.h) are the porters'
> transcription: more readable, occasionally better-named, and **wrong in at least
> one place** (§4). Where they differ, the assembly wins.
>
> Unlike the behavioural documents, this one is about layout rather than tuning, so
> its contents are facts about the original binary rather than fitting targets.

> **A third vote.** [amiga/STATE.md](amiga/STATE.md) reconstructs the same three
> structures from the Amiga original's IDA `struc` blocks, checked against real
> access sites. **Every `Sprite` offset in §2 and every `PlayerInfo` attribute offset
> in §5 is confirmed byte for byte**, and the disagreement in §4 is settled. §5's
> claim that the attribute range is not 0–7 is **refuted**. See §11.

---

## 0. One-paragraph version

Match state lives in four structures. **`Sprite`** (110 bytes) is every moving
thing — 22 players, the ball, the referee, the booked-number indicator — carrying
16.16 fixed-point position, velocity, animation cursor and per-entity flags.
**`TeamGeneralInfo`** (145 bytes, two instances: `topTeamData` and
`bottomTeamData`) is the per-team control and decision block: input state, ball
proximity bands, aftertouch spin timers, AI scratch. **`TeamGame`** (1704 bytes) is
the squad — kit colours, name, and 16 `PlayerInfo` records of 62 bytes each holding
the attributes. **`PlayerInfo`** is where Passing, Shooting, Heading, Tackling,
Control, Speed and Finishing actually live. Around these sit a large flat block of
globals — `gameState`, `gameStatePl`, `foulXCoordinate`, `hideBall`,
`ballNextX/Y` — addressed by absolute offset throughout the decompilation.

---

## 1. Why this matters for our engine

We are not reproducing this memory layout. So the value of this document is not the
byte offsets — it is **what the offsets reveal about the design**:

- **State is flat and pre-allocated.** No allocation happens during a match. Every
  entity is a fixed slot.
- **The per-team block is where decisions live**, not the per-player one. `Sprite`
  holds physics and animation; `TeamGeneralInfo` holds *intent*. A player does not
  know it is passing — its team does (`passingBall`, `passingToPlayer`,
  `passInProgress`).
- **There is exactly one controlled player per team**, held as a pointer
  (`controlledPlayerSprite`), and the AI writes into the same input fields a human
  does ([MOVEMENT.md](MOVEMENT.md) §8).
- **Roughly a fifth of both structs is unnamed.** `field_3E`, `field_56`, `ofs108`
  and friends are read and written by the binary and nobody has worked out what
  they mean. That is the honest state of reverse engineering here, and §7 lists
  them so we know what we do not know.

---

## 2. `Sprite` — 110 bytes

Every moving object. Sources:
[swos.asm `Sprite struc`](../reference/swos-port/swos/swos.asm),
[Sprite.h](../reference/swos-port/src/sprites/Sprite.h).

| Off | Size | Name | Meaning |
|---|---|---|---|
| 0 | 2 | `teamNumber` | 1 or 2 for player-controlled, 0 for CPU, **3 for referee sprites** ([REFEREE.md](REFEREE.md) §1) |
| 2 | 2 | `playerOrdinal` | 1–11; **1 is the goalkeeper**; 0 for non-players |
| 4 | 2 | `frameOffset` | Kit/face frame base ([PLAYER_SPRITES.md](PLAYER_SPRITES.md) §8) |
| 6 | 4 | `animationTable` | → `PlayerAnimationTable` |
| 10 | 2 | `startingDirection` | |
| 12 | 1 | `playerState` | `PlayerState` enum, §3 |
| 13 | 1 | `playerDownTimer` | Recovery countdown, **signed** |
| 14 | 2 | `field_E` | ? |
| 16 | 2 | `field_10` | ? |
| 18 | 4 | `frameIndicesTable` | Current animation's frame list |
| 22 | 2 | `frameIndex` | Cursor into it |
| 24 | 2 | `frameDelay` | Reload value |
| 26 | 2 | `cycleFramesTimer` | Countdown to next frame |
| 28 | 2 | `frameSwitchCounter` | Gates animation swaps ([HEADING.md](HEADING.md) §4) |
| 30 | 4 | `x` | **16.16 fixed point** |
| 34 | 4 | `y` | |
| 38 | 4 | `z` | Height |
| 42 | 2 | `direction` | Octant 0–7 |
| 44 | 2 | `speed` | Scalar |
| 46 | 4 | `deltaX` | Per-tick increment, 16.16 |
| 50 | 4 | `deltaY` | |
| 54 | 4 | `deltaZ` | |
| 58 | 2 | `destX` | **Whole units** — the aim point |
| 60 | 2 | `destY` | |
| 62 | 6 | `field_3E` | ? |
| 68 | 2 | `visible` | Skip rendering if false |
| 70 | 2 | `imageIndex` | `< 0` = none |
| 72 | 2 | `saveSprite` | |
| 74 | 4 | `ballDistance` | Squared distance to ball |
| 78 | 2 | `field_4E` | ? |
| 80 | 2 | `field_50` | ? |
| 82 | 2 | `fullDirection` | Fine heading 0–255 ([BALL.md](BALL.md) §2) |
| 84 | 2 | `onScreen` | |
| 86 | 2 | `field_56` | ? |
| 88 | 2 | `field_58` | ? |
| 90 | 2 | `field_5A` | ? |
| 92 | 2 | `playerDirection` | Signed, `-1` for non-players |
| 94 | 2 | `isMoving` | |
| 96 | 2 | `tackleState` | 0 / 1 / `TS_GOOD_TACKLE` ([TACKLING.md](TACKLING.md) §1) |
| 98 | 2 | `heading` | Header contact flag ([HEADING.md](HEADING.md) §1) |
| 100 | 2 | `destReachedState` | `DestinationState` enum |
| 102 | 2 | `cards` | **`-1` = sent off** ([REFEREE.md](REFEREE.md) §5) |
| 104 | 2 | `injuryLevel` | |
| 106 | 2 | `tacklingTimer` | **Signed; negative means "ending"** ([TACKLING.md](TACKLING.md) §3) |
| 108 | 2 | `sentAway` | |

**`Sprite.h` calls +98 `unk009`.** The assembly names it `heading`, and
[HEADING.md](HEADING.md) §3 confirms the usage. Prefer the assembly name.

Note the **destination is whole units while position is 16.16**. Every boundary
test in the engine compares the high word of the position against whole numbers
([BALL.md](BALL.md) §1). The fractional part exists for motion integration only and
never participates in a decision.

### `PlayerState`

```
kNormal = 0            kTackling = 1          kTackled = 3
kGoalieCatchingBall=4  kThrowIn = 5           kGoalieDivingHigh = 6
kGoalieDivingLow = 7   kStaticHeader = 8      kJumpHeader = 9
kDown = 10             kGoalieClaimed = 11    kBooked = 12
kInjured = 13          kSad = 14              kHappy = 15
kUnknown = 255
```

Note `2` is unused. Four of the sixteen states are goalkeeper-specific, and three
(`kSad`, `kHappy`, `kBooked`) are purely presentational.

---

## 3. `TeamGeneralInfo` — 145 bytes (`0x91`)

Two instances: `topTeamData`, `bottomTeamData`. This is the control and decision
block, and it is the struct the other documents cite most.

| Off | Size | Name (assembly) | Meaning |
|---|---|---|---|
| 0 | 4 | `opponentsTeam` | → the other `TeamGeneralInfo` |
| 4 | 2 | `playerNumber` | **0 = CPU-controlled team** |
| 6 | 2 | `playerCoachNumber` | |
| 8 | 2 | `isPlCoach` | |
| 10 | 4 | `inGameTeamPtr` | → `TeamGame` (the squad) |
| 14 | 4 | `teamStatsPtr` | → `TeamStatsData` |
| 18 | 2 | `teamNumber` | 1 or 2 |
| 20 | 4 | `spritesTable` | → array of 11 `Sprite*` |
| 24 | 4 | `shotChanceTable` | |
| 28 | 2 | `tactics` | Index into `g_tacticsTable` ([BENCH.md](BENCH.md) §6) |
| 30 | 2 | `updatePlayerIndex` | Round-robin cursor |
| 32 | 4 | `controlledPlayerSprite` | The driven player ([AI.md](AI.md) §2) |
| 36 | 4 | `passToPlayerPtr` | |
| 40 | 2 | `playerHasBall` | Gates pitch friction ([BALL.md](BALL.md) §3) |
| 42 | 2 | `allowedDirections` | |
| 44 | 2 | `currentAllowedDirection` | **Live input**; `-1` = nothing held |
| 46 | 2 | `direction` | |
| 48 | 1 | `quickFire` | Tap = pass ([SHOOTING.md](SHOOTING.md) §1) |
| 49 | 1 | `normalFire` | Hold = shot |
| 50 | 1 | `firePressed` | |
| 51 | 1 | `fireThisFrame` | |
| 52 | 2 | `headerOrTackle` | Contest triggered this tick |
| 54 | 2 | `fireCounter` | Hold duration |
| **56** | 2 | **`controlledPlDirection`** | **Disputed name — see §4** |
| 58 | 2 | `shooting` | |
| 60 | 1 | `field_3C` | ? |
| 61 | 1 | `plVeryCloseToBall` | Proximity bands ([CONTROL.md](CONTROL.md) §2) |
| 62 | 1 | `plCloseToBall` | |
| 63 | 1 | `plNotFarFromBall` | |
| 64 | 1 | `ballLessEqual4` | |
| 65 | 1 | `ball4To8` | |
| 66 | 1 | `ball8To12` | |
| 67 | 1 | `ball12To17` | |
| 68 | 1 | `ballAbove17` | |
| 69 | 1 | `prevPlVeryCloseToBall` | Edge detect |
| 70 | 2 | `field_46` | ? |
| 72 | 4 | `lastHeadingTacklingPlayer` | Last contester |
| 76 | 2 | `goalkeeperSavedCommentTimer` | |
| 78 | 2 | `field_4E` | ? |
| 80 | 2 | `goalkeeperDivingRight` | |
| 82 | 2 | `goalkeeperDivingLeft` | |
| 84 | 2 | `ballOutOfPlayOrKeeper` | |
| 86 | 2 | `goaliePlayingOrOut` | |
| 88 | 2 | `passingBall` | |
| 90 | 2 | `passingToPlayer` | |
| 92 | 2 | `playerSwitchTimer` | Selection hysteresis ([AI.md](AI.md) §2) |
| 94 | 2 | `ballInPlay` | |
| 96 | 2 | `ballOutOfPlay` | |
| 98 | 2 | `ballX` | Team's cached ball position |
| 100 | 2 | `ballY` | |
| 102 | 2 | `passKickTimer` | |
| 104 | 4 | `passingKickingPlayer` | |
| 108 | 2 | `unkBallTimer` | ? |
| 110 | 2 | `ballCanBeControlled` | Cleared during a jump header ([HEADING.md](HEADING.md) §2) |
| 112 | 2 | `ballControllingPlayerDirection` | |
| 114 | 2 | `field_72` | ? |
| 116 | 2 | `field_74` | ? |
| 118 | 2 | `spinTimer` | **Aftertouch window** ([AFTERTOUCH.md](AFTERTOUCH.md) §2); `-1` = inactive |
| 120 | 2 | `leftSpin` | |
| 122 | 2 | `rightSpin` | |
| 124 | 2 | `longPass` | |
| 126 | 2 | `longSpinPass` | |
| 128 | 2 | `passInProgress` | |
| 130 | 2 | `AI_timer` | |
| 132 | 2 | `field_84` | ? |
| 134 | 2 | `AI_afterTouchStrength` | **The CPU's virtual aftertouch** ([AFTERTOUCH.md](AFTERTOUCH.md) §8) |
| 136 | 2 | `AI_ballSpinDirection` | |
| 138 | 2 | `wonTheBallTimer` | Set to 12 on winning a contest ([TACKLING.md](TACKLING.md) §8) |
| 140 | 2 | `goalkeeperPlaying` | |
| 142 | 2 | `resetControls` | |
| 144 | 1 | `secondaryFire` | |

---

## 4. Where the two sources disagree

Three discrepancies, all traps for anyone reading the C++ header alone.

**Offset 56 has two different names.** The assembly calls it
`controlledPlDirection`, written by `PlayerTackledTheBallStrong` and `DoPass`. The
C++ header calls it `allowedPlDirection`, and [AFTERTOUCH.md](AFTERTOUCH.md) §7
cites that name as the spin-table index. **Both documents are describing the same
two bytes.** The assembly name reflects what writes it; the C++ name reflects what
reads it. Neither is wrong, but they must not be treated as separate fields.

**The C++ header's tail placeholder names are misnumbered by two.** `swos.h` has
`ofs134`, `ofs136`, `ofs138` at actual offsets **132, 134, 136**:

| Actual offset | Assembly | `swos.h` |
|---|---|---|
| 132 | `field_84` | `ofs134` |
| 134 | `AI_afterTouchStrength` | `ofs136` |
| 136 | `AI_ballSpinDirection` | `ofs138` |
| 138 | `wonTheBallTimer` | `unkTimer` |

The earlier placeholders (`ofs60`, `ofs70`, `ofs78`) *are* correctly numbered, so
the drift begins somewhere after offset 104. **Do not read `swos.h`'s `ofsNNN`
names as offsets.**

Note also that the assembly recovers two meaningful names the C++ loses:
`AI_afterTouchStrength` and `AI_ballSpinDirection` are exactly the CPU virtual-joystick
fields [AFTERTOUCH.md](AFTERTOUCH.md) §8 discusses, sitting anonymously in the C++
mirror as `ofs136`/`ofs138`.

**`Sprite` +98** is `heading` in the assembly, `unk009` in `Sprite.h`.

---

## 5. `PlayerInfo` — the attributes

62 bytes. This is where player quality lives.

| Off | Size | Name | Notes |
|---|---|---|---|
| 0 | 1 | `substituted` | |
| 1 | 1 | `index` | |
| 2 | 1 | `goalsScored` | |
| 3 | 1 | `shirtNumber` | |
| 4 | 1 | `position` | `PlayerPosition`; `kSubstituted` marks a bench slot |
| 5 | 1 | `face` | `kWhite` / `kGinger` / `kBlack` |
| 6 | 1 | `isInjured` | |
| 7–11 | 5 | `field_7`…`cards`, `field_B` | `cards` at +10 |
| 12 | 15 | `shortName` | |
| 27 | 1 | **`passing`** | |
| 28 | 1 | **`shooting`** | "Velocity" — long shots ([SHOOTING.md](SHOOTING.md) §3) |
| 29 | 1 | **`heading`** | Indexes the ±table in [HEADING.md](HEADING.md) §6 |
| 30 | 1 | **`tackling`** | [TACKLING.md](TACKLING.md) §6, §8 |
| 31 | 1 | **`ballControl`** | "Control" — [CONTROL.md](CONTROL.md) §3 |
| 32 | 1 | **`speed`** | [MOVEMENT.md](MOVEMENT.md) §2 |
| 33 | 1 | **`finishing`** | In-the-box shots |
| 34 | 1 | `goalieSkill` | |
| 35 | 1 | `injuriesBitfield` | |
| 36 | 1 | `halfPlayed` | |
| 37 | 1 | `face2` | |
| 38 | 23 | `fullName` | |

Accessed in the decompilation as `PlayerGameHeader`, which is `PlayerInfo` offset by
`kTeamGameHeaderSize` — hence the large offsets quoted elsewhere
(`shooting +70`, `tackling +72`, `ballControl +73`, `finishing +75`). Those are
`PlayerGameHeader` offsets; the table above is `PlayerInfo`-relative. Subtract
`kTeamGameHeaderSize` (42) to convert: `72 − 42 = 30` = `tackling`. ✓

### The attribute range ~~is not 0–7~~ **is 0–7**

> ⚠️ **This subsection was wrong and is retained only so the error is traceable.**
> It rested on [HEADING.md](HEADING.md) §6's thirteen-entry
> `kPlayerHeaderSpeedIncrease`, which is a mis-read of an eight-entry table — the
> five positive values belong to the next data item in the segment
> ([HEADING.md](HEADING.md) §10). The Amiga original masks each stored nibble with
> `7` and clamps to 7 explicitly on load, and every attribute-indexed table in the
> engine has exactly eight entries ([amiga/PLAYERS.md](amiga/PLAYERS.md) §1).
> **The range is 0–7.** The 1–8 the interface shows is a display offset.

The original text, for the record: *"[HEADING.md](HEADING.md) §6 establishes this
from the 13-entry `kPlayerHeaderSpeedIncrease` table, indexed by the raw attribute
with no clamp — so `heading` reaches at least 12. Since attributes are single bytes
with no packing, nothing structural limits them to 0–7."*

The structural argument was sound but the premise was not. Attributes *are* single
bytes in `PlayerInfo`, so nothing limits them **there** — but they are unpacked into
those bytes from packed nibbles by a routine that clamps
([amiga/PLAYERS.md](amiga/PLAYERS.md) §3), so the reachable range is 0–7 regardless
of the storage width.

Consequently the bounds worry in [TACKLING.md](TACKLING.md) §10 does not arise:
`kPlAvgTacklingBallControlDiffChance` is indexed by a difference of two 0–7 averages,
maximum 7, and eight entries is exactly right.

### A documented original bug

`PlayerInfo::canBeSubstituted()` carries a porters' comment: the original returns an
**uninitialised register** when `substituted` is false and `position` is
`kSubstituted`, yielding `true` where `false` was presumably intended. They
reproduce it under `SWOS_TEST` and use the clean negation otherwise. One for
[LEGACY.md](LEGACY.md) §14.

---

## 6. `TeamGame`, tactics, and files

**`TeamGame`** — 1704 bytes, the in-match squad:

```
prShirtType, prShirtCol, prStripesCol, prShortsCol, prSocksCol   // primary kit
secShirtType, secShirtCol, secStripesCol, secShortsCol, secSocksCol
markedPlayer          // int16, -1 = none   (REFEREE.md §5, BENCH.md §5)
teamName[17]
numOwnGoals
PlayerInfo players[16]        // 11 on the pitch + 5 subs
unknownTail[686]
```

`kTeamGameHeaderSize = offsetof(TeamGame, players)` = 42. The 1704-byte figure is
confirmed independently by the highlight file header
(`docs/highlights.txt` reserves 1704 bytes per team).

**`TeamTactics`** — 370 bytes:

```
name[9]
PlayerPositions positions[10]     // 10 outfield roles
    byte positions[35]            // the 35-cell zonal grid  (AI.md §3)
unkTable[10]
ballOutOfPlayTactics
```

**35 cells × 10 players** is the whole off-ball system ([AI.md](AI.md) §3).

**File-format structs** (on disk, not in play): `PlayerFile` = 38 bytes,
`TeamFile` = 684 bytes = header + 16 × `PlayerFile`. Covered in
[DATA.md](DATA.md).

**Other enums** in [swos.h](../reference/swos-port/src/swos/swos.h):
`ShirtTypes` (ordinary / coloured sleeves / vertical / horizontal stripes — four
kit patterns, [PLAYER_SPRITES.md](PLAYER_SPRITES.md) §3), `FaceTypes` (three),
`GameTypes` (no game / DIY competition / preset competition / season / career).

---

## 7. Globals

The decompilation addresses globals by absolute VM offset (`g_memByte[523118]`).
The ones the engine documents actually cite:

| Global | Meaning |
|---|---|
| `gameState` | The 21-value restart enum ([SETPIECES.md](SETPIECES.md) §1) |
| `gameStatePl` | Physics gate: 100 = in progress, 101 = foul processing |
| `foulXCoordinate`, `foulYCoordinate` | The restart spot, reused for every stoppage |
| `cameraDirection` | Octant the camera faces after a restart |
| `playerTurnFlags` | Turn-direction bitmask ([SETPIECES.md](SETPIECES.md) §2) |
| `lastTeamPlayedBeforeBreak` | Which team restarts |
| `breakCameraMode` | Written by every restart; **no reader found** ([CAMERA.md](CAMERA.md) §10) |
| `hideBall` | Suppresses the ball sprite (throw-ins) |
| `ballNextX`, `ballNextY` | Predicted landing point ([BALL.md](BALL.md) §8) |
| `ballNextYGroundY` | A second, unexplained prediction |
| `teamStarting`, `teamPlayingUp` | Rolled at kick-off |
| `kBallGroundConstant`, `kBallAirConstant`, `kGravityConstant` | Amiga/DOS switched ([BALL.md](BALL.md) §9) |
| `pitchBallSpeedFactor`, `ballSpeedBounceFactor`, `ballBounceFactor` | Per pitch type |
| `refState`, `whichCard`, `bookedPlayer`, `lastTeamBooked`, `refTimer` | [REFEREE.md](REFEREE.md) §1 |
| `g_substituteInProgress`, `g_waitForPlayerToGoInTimer`, `teamThatSubstitutes` | [BENCH.md](BENCH.md) §3 |
| `showFansCounter` | Freezes the camera; decremented in `DrawAnimatedPatterns` |
| `injuriesForever` | **Dead variable** — annotated *"probably something for testing"* |

`docs/contiguous-variables.txt` in the reference is the porters' own working notes
on this block and is the place to start if a specific global needs locating.

---

## 8. Unnamed fields

Recorded so the gaps are explicit rather than invisible.

**`Sprite`**: `field_E` (+14), `field_10` (+16), `field_3E` (+62, 6 bytes),
`field_4E` (+78), `field_50` (+80), `field_56` (+86), `field_58` (+88),
`field_5A` (+90), `saveSprite` (+72, named but unexplained).

**`TeamGeneralInfo`**: `field_3C` (+60), `field_46` (+70), `field_4E` (+78),
`unkBallTimer` (+108), `field_72` (+114), `field_74` (+116), `field_84` (+132).

**`TeamGame`**: `unknownTail[686]` — **40 % of the struct**. Almost certainly the
per-match statistics and the substitute bookkeeping, but unmapped.

**`PlayerInfo`**: `field_7`, `field_8`, `field_9`, `field_B`.

Roughly 20 bytes of `Sprite`, 16 of `TeamGeneralInfo` and 686 of `TeamGame` are
written by the binary with no known meaning.

---

## 9. What this resolves, and what still needs measurement

**Confirmed as structure:**

- `Sprite` is 110 bytes; full field map with offsets, cross-checked against every
  citation in the engine documents. ✓
- `TeamGeneralInfo` is 145 bytes (`0x91`); full field map. ✓
- Position is 16.16 fixed point, destination is whole units, and only the integer
  part is ever compared. ✓
- Decisions live in the per-team struct, physics in the per-sprite struct. ✓
- `PlayerInfo` is 62 bytes with the seven outfield attributes at +27…+33. ✓
- `PlayerGameHeader` offsets are `PlayerInfo` offsets plus 42. ✓
- Attributes are unpacked bytes with no structural 0–7 limit. ✓
- Offset 56 is one field with two names; `swos.h`'s tail placeholders are
  misnumbered by 2. ✓
- Tactics are 10 roles × 35 grid cells. ✓
- `TeamGame` is 1704 bytes, confirmed independently by the highlight file format. ✓

**Resolved by the Amiga oracle** (see §11):

- ~~**The real attribute range.**~~ **0–7**, masked and clamped on load. Every
  eight-entry table in the corpus is correctly sized.
- ~~`TeamTactics::ballOutOfPlayTactics`.~~ It is byte `$171` of the record and holds
  the index of **another tactic** to use at restarts — tactics carry their own
  set-piece variant rather than the engine computing one ([AI.md](AI.md) §10).
- ~~`shotChanceTable` (+24) — who fills it.~~ Partially: the Amiga's counterpart at
  the same offset is a pointer to a 60-byte per-side tuning block, selected by
  `UpdateTeamOfs24Table` (asm:35548), which uses a **different block for
  goalkeepers**. Contents still largely unmapped on both sides.
- ~~`Sprite` +24 (`frameDelay`).~~ Confirmed as the frame-cycle reload, and the
  Amiga gives its writer: it is computed from `speed` inside the simulation
  ([MOVEMENT.md](MOVEMENT.md) §13).

**Open:**

- `TeamGame::unknownTail[686]` — 40 % of the squad struct is unmapped.
- `TeamStatsData` — pointed to at `TeamGeneralInfo +14`, layout not read.
  [SIMULATION.md](SIMULATION.md) §7 covers the statistics behaviourally.
- The remaining unnamed `Sprite` / `TeamGeneralInfo` fields in §8. The Amiga
  independently finds most of the same gaps, which is itself informative — they are
  genuinely unread rather than merely un-transcribed.
- `PlayerPosition` enum values beyond `kSubstituted`.
- `TeamTactics::unkTable[10]` — bounded to bytes `$167`–`$170` of the record, still
  unexplained on both oracles.
- Whether `saveSprite` (+72) relates to replay capture.
- **`Sprite` +96 and +106.** Both offsets are confirmed to exist and to be
  meaningful, but the two oracles read them differently — see §11 and
  [TACKLING.md](TACKLING.md) §12. These are the only two places in the whole struct
  map where the readings conflict.

---

## 10. Guidance for the reimplementation

- **Do not reproduce this layout.** Byte offsets, `SwosDataPointer`, and a flat VM
  memory image are 1994 constraints. Use ordinary structs and let the compiler lay
  them out. This document exists so that when [BALL.md](BALL.md) says
  `Sprite.speed (+44)` you know what it means, not so we can match it.
- **Do keep the division of responsibility.** Physics and animation per entity;
  intent, input and contest state per team. It is the single most portable idea in
  the layout, and it is why one code path serves both human and CPU
  ([MOVEMENT.md](MOVEMENT.md) §8).
- **Keep position fixed-point and destinations integral.** Not for compatibility —
  because every gameplay comparison in the reference is integral, and using floats
  for boundary tests will produce different behaviour at exactly the edges where it
  is most visible. [PLAN.md](PLAN.md) §0 already commits to fixed point.
- **Pre-allocate everything.** 22 players, one ball, one referee, fixed. No
  allocation inside the tick keeps determinism cheap and replays exact.
- **Name the unknowns explicitly in our structs too.** If we carry a field forward
  because the reference has one, call it `unknown_field_3E` and comment where it is
  written. Silently dropping it is fine; silently renaming it to a guess is not.
- **Model attributes as 0–7 internally** and offset only at the presentation
  boundary (§5, §11). Every table in the engine is eight entries wide; anything else
  means reindexing all of them.
- **Treat the assembly as authoritative** whenever the two references disagree, and
  add a note to this document when a new discrepancy is found. With the Amiga
  disassembly available, a third vote is usually cheap — use it before recording a
  discrepancy as unresolvable.
- **Do not port the label names.** Name our fields after what the code does with
  them — `velocity` not `shooting`, `x` not `xFraction`. Both oracles carry
  mislabelled fields, and every one of them is a trap someone inherited by copying
  a name instead of a behaviour (§11).

---

## 11. Amiga cross-check

A third independent reconstruction — [amiga/STATE.md](amiga/STATE.md), from the
Amiga original's IDA `struc` blocks at asm:1–305, with every offset checked against
a real access site.

### `Sprite`: every offset in §2 confirmed

| Field | §2 | Amiga | |
|---|---|---|---|
| `playerState` | 12 | $0C | ✓ |
| `playerDownTimer` | 13 | $0D | ✓ |
| `frameIndicesTable` | 18 | $12 | ✓ |
| `frameIndex` | 22 | $16 | ✓ |
| `frameDelay` | 24 | $18 | ✓ (reload, set from speed) |
| `cycleFramesTimer` | 26 | $1A | ✓ |
| `x` / `y` / `z` | 30 / 34 / 38 | $1E / $22 / $26 | ✓ 16.16 |
| `direction` | 42 | $2A | ✓ |
| `speed` | 44 | $2C | ✓ |
| `deltaX` / `Y` / `Z` | 46 / 50 / 54 | $2E / $32 / $36 | ✓ |
| `destX` / `destY` | 58 / 60 | $3A / $3C | ✓ whole units |
| `imageIndex` | 70 | $46 | ✓ (−1 hides) |
| `ballDistance` | 74 | $4A | ✓ squared |
| `fullDirection` | 82 | $52 | ✓ 0–255 |
| `heading` | 98 | $62 | ✓ header-in-progress marker |
| `cards` | 102 | $66 | ✓ |
| `injuryLevel` | 104 | $68 | ✓ |
| `sentAway` | 108 | $6C | ✓ |
| **Total size** | **110** | **$6E = 110** | ✓ |

Two things this settles:

- **`Sprite.h`'s `unk009` at +98 really is `heading`.** §4's preference for the
  assembly name is confirmed by a third source.
- **The port's position layout is right where IDA's raw struct is wrong.** The Amiga
  `struc` declares `x` at $1C and `xFraction` at $1E, i.e. it believed each
  coordinate is an integer word followed by a fraction word. The code disagrees —
  `UpdateBall` does a **longword** add into $1E, and the boundary tests compare $1E
  against pixel values like 53 and 618. So $1E *is* X as 16.16, exactly as §2 has it.
  IDA's `x`/`y`/`z` labels sit one word early and are not the coordinate at all.
  Anyone reading the Amiga listing directly must read `Sprite.xFraction` as "X".
  Delta fields are *not* skewed.

  A corollary worth keeping: the ground-versus-air friction test reads the **integer
  height**, so friction flips the instant the ball leaves the turf, not at some
  fractional threshold ([BALL.md](BALL.md) §3).

### §4's disagreement, settled

§4 records offsets 44 and 56 as one field with two names apiece and declines to
choose. The Amiga has both, and they are different fields with different jobs:

| Offset | Amiga name | Amiga meaning |
|---|---|---|
| 44 | `currentDirection` | **Joystick octant this frame**; −1 = neutral |
| 56 | `field_38` | **The direction the ball was kicked in** — the axis curl is measured against |

That matches §3's own gloss on +44 ("live input; −1 = nothing held") and
[MOVEMENT.md](MOVEMENT.md) §3.1, and it makes +56 the *kick* direction — which is
what both of §4's names for it are groping at. It also means
[AFTERTOUCH.md](AFTERTOUCH.md) §2 has these two rows swapped; see
[AFTERTOUCH.md](AFTERTOUCH.md) §11.

Beware one further name collision: on the Amiga, `controlledPlDirection` is IDA's
label for the *fraction word of `Sprite.deltaZ`*, an unrelated field in an unrelated
struct. The name travels; the meaning does not.

### `TeamGeneralInfo`: confirmed, with several `?` filled in

Size is $90 = **144** on the Amiga against §3's 145 — the port's extra byte is the
`secondaryFire` at +144, which has no Amiga counterpart.

Offsets confirmed: `opponentsTeam` 0, `playerNumber` 4, `inGameTeamPtr` 10,
`spritesTable` 20, `tactics` 28, `updatePlayerIndex` 30, `controlledPlayerSprite`
32, `passToPlayerPtr` 36, `playerHasBall` 40, `allowedDirections` 42, the whole
proximity/height band run at 61–68, `goalkeeperSavedCommentTimer` 76,
`goalkeeperDivingRight/Left` 80/82, `goaliePlayingOrOut` 86, `passingBall` 88,
`passingToPlayer` 90, `playerSwitchTimer` 92, `ballInPlay` 94, `ballOutOfPlay` 96,
`passKickTimer` 102, `ballCanBeControlled` 110, `spinTimer` 118, `leftSpin` 120,
`rightSpin` 122, the `longPass` pair 124/126, `passInProgress` 128, `AI_timer` 130,
`wonTheBallTimer` 138, `goalkeeperPlaying` 140, `resetControls` 142.

Three of §8's unknowns get meanings:

| §8 unknown | Amiga | Meaning |
|---|---|---|
| `unkBallTimer` (+108) | `unkBallTimer` $6C | **Dribble touch counter** — [CONTROL.md](CONTROL.md) §8 |
| `shotChanceTable` (+24) | `teamPlOfs24Table` $18 | Pointer to a 60-byte per-side tuning block; different for keepers |
| `Sprite` +24 | `field_18` | Frame-cycle reload, written from `speed` |

And one field gets a much sharper reading: `goalkeeperSavedCommentTimer` (+76) is a
**±5 latch** — set to +5 on a save and −5 on a goal — which is how the rest of the
engine learns what just happened, and what [AFTERTOUCH.md](AFTERTOUCH.md) §3's first
guard reads.

### `PlayerInfo`: the attribute block confirmed, and one name corrected

§5's order at +27…+33 — Passing, Shooting, Heading, Tackling, Control, Speed,
Finishing — is exactly the Amiga's `PlayerGame` $45…$4B, and the Amiga derives it
from read sites rather than from labels: $46 for the long-shot bonus, $4B for the
close-range bonus, $47 for headers, $48 and $49 averaged for the tackle contest,
$49 for the dribble, $4A for running speed, $4C for the keeper's save odds. Seven
independent constraints landing on SWOS's own published **P V H T C S F** ordering.

The correction: **+28 `shooting` is Velocity, not a second finishing stat.** §5's
parenthetical already says so; the Amiga makes it a named finding because IDA's own
label at that offset is `shooting` and it is the easiest error in the corpus to
inherit. Name our field `velocity`.

`goalieSkill` (+34), `injuriesBitfield` (+35) and `halfPlayed` (+36) all confirm at
$4C, $4D and $4E. Two further findings about them:

- **The goalkeeper rating is derived, not stored** — `(value + 3) / 7` plus 1 or 2
  plus two competition-context ±1 adjustments, clamped 0–7. Non-keepers get 0. A
  keeper has no stored goalkeeping attribute at all; his transfer value *is* the
  rating ([amiga/PLAYERS.md](amiga/PLAYERS.md) §3).
- **The stored nibbles are not the final ratings.** They pass through a
  value-derived transform on unpack, so tuning fitted against raw extracted team
  data will carry a systematic bias. Model the transform explicitly, even as a stub.

### One more coordinate fact worth having here

The Amiga records the pitch geometry as pixel constants checked against access
sites, which is the cleanest statement of it anywhere in the corpus:

| Quantity | Value |
|---|---|
| Playable X / Y | 81 … 591 (width 510) / 129 … 770 (height 641) |
| Centre | (336, 449) |
| Goal mouth X | 302 … 366 inner, 296 … 372 including posts |
| Crossbar Z | 15 … 19 |
| Dead-ball barrier | X 53 … 618, Y 100 … 799 |
| Player movement clamp | X 81 … 590, Y 129 … 769 |

Y increases downward; the left team defends the top goal. And `speed` is in units of
**~1/512 px/frame**, derived from the Q15→Q7 shift in the trig routine — so 512 ≈ 1
px/frame, which is the conversion every constant in the corpus needs.
