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

### The attribute range is not 0–7

[HEADING.md](HEADING.md) §6 establishes this from the 13-entry
`kPlayerHeaderSpeedIncrease` table, indexed by the raw attribute with no clamp — so
`heading` reaches at least 12. Since attributes are single bytes with no packing,
**nothing structural limits them to 0–7**.

This is the open bounds question flagged in [TACKLING.md](TACKLING.md) §10:
`kPlAvgTacklingBallControlDiffChance` has 8 entries and is indexed by a *difference*
of two attribute averages. If attributes reach 12, that difference can exceed 7 and
the table is read out of bounds. **Resolve this before implementing any
attribute-indexed table**, and check every such table's length against the real
range.

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

**Open:**

- **The real attribute range.** The single highest-value item here — it invalidates
  or validates every attribute-indexed table in the other documents.
  [LEGACY.md](LEGACY.md) §9 should be reconciled against `PlayerInfo`.
- `TeamGame::unknownTail[686]` — 40 % of the squad struct is unmapped.
- `TeamStatsData` — pointed to at `TeamGeneralInfo +14`, layout not read.
  [SIMULATION.md](SIMULATION.md) §7 covers the statistics behaviourally.
- The ~36 bytes of unnamed `Sprite` / `TeamGeneralInfo` fields in §8.
- `PlayerPosition` enum values beyond `kSubstituted`.
- `TeamTactics::unkTable[10]` and `ballOutOfPlayTactics`.
- Whether `saveSprite` (+72) relates to replay capture.
- `shotChanceTable` (+24) contents and who fills it.

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
- **Resolve the attribute range first** (§5) and size every attribute table to the
  real range with an explicit bounds policy — clamp, assert, or reproduce the
  overrun — chosen deliberately and recorded.
- **Treat the assembly as authoritative** whenever the two references disagree, and
  add a note to this document when a new discrepancy is found.
