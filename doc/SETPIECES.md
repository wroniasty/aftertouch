# SETPIECES.md

Restarts as executed: what the game state actually becomes after a stoppage, where
the ball and the taker are placed, how a foul is classified into a penalty, a free
kick or nothing much, and what the taker is allowed to do. Traced through the
reference DOS port in [../reference/swos-port/](../reference/swos-port/) and its
annotated disassembly.

[SIMULATION.md](SIMULATION.md) §5 covers **detection** — how the engine notices
the ball has gone out and which restart is owed. This document is the other half:
**execution**. The kick that ends a set piece is [SHOOTING.md](SHOOTING.md); the
foul that causes one is [TACKLING.md](TACKLING.md) §5; the cards and injuries that
follow are [SIMULATION.md](SIMULATION.md) §6.

> **Provenance.** Read from the annotated disassembly, which preserves the
> original's own comments on this code (`; lower white spot`, `; only allow E, SE,
> S, SW, W`, `; both penalty areas left bound`). Those comments are the source of
> several geometry constants below and are unusually trustworthy. The **ball
> destination delta tables are real data**
> ([swos.asm:245586-245598](../reference/swos-port/swos/swos.asm#L245586)). Read to
> understand the design; write our own code.

> **Second oracle.** [amiga/SETPIECES.md](amiga/SETPIECES.md) traces `GameSetup`
> (asm:41156) and `GetBallDestCoordinatesTable` (asm:39793) in the Amiga original.
> The two readings agree on the state enum, the turn-mask bit order, the penalty-area
> bounds and every aiming-table bias that both cover — an unusually clean match. The
> Amiga adds the **restart placement coordinates**, which this document does not
> have, four **corner and throw-in turn masks**, and a notable absence: **no offside
> test exists**. See §12.

---

## 0. One-paragraph version

Every restart is a **`gameState` value plus four writes**: the restart spot
(`foulXCoordinate` / `foulYCoordinate`), the camera facing (`cameraDirection`), the
directions the taker may turn (`playerTurnFlags`), and which team has the ball
(`lastTeamPlayedBeforeBreak`). There are **twenty-one distinct restart states** —
seven free-kick zones, six throw-in variants, two corners, a penalty and the rest —
because SWOS encodes *where* the restart is into the state enum rather than into
data. A foul is classified by pure geometry: inside `x ∈ [193, 478]` and beyond the
penalty-area line it is a penalty, taken from a hardcoded white spot; in a band
outside that it becomes one of seven free-kick states chosen by x-position and
mirrored by which team offended; anywhere else it is a plain `ST_FOUL`. While a
restart is live the ball is fenced by the invisible barrier of
[BALL.md](BALL.md) §5, the taker's turning is restricted by a bitmask, and the kick
itself reads a **per-state table of eight directional deltas** that biases where the
ball can be aimed. There is **no wall-assembly code anywhere in the game**.

---

## 1. The state enum

[swos.asm:1404-1433](../reference/swos-port/swos/swos.asm#L1404-L1433):

| Value | State | |
|---|---|---|
| 0 | Goal scored → kick-off | *(from the Amiga, §12)* |
| 1, 2 | Goal kick, the two halves of the goal area | *(from the Amiga, §12)* |
| 3 | `ST_KEEPER_HOLDS_BALL` | |
| 4, 5 | `ST_CORNER_LEFT`, `ST_CORNER_RIGHT` | |
| 6–8 | `ST_FREE_KICK_LEFT1/2/3` | |
| 9 | `ST_FREE_KICK_CENTER` | |
| 10–12 | `ST_FREE_KICK_RIGHT1/2/3` | |
| 13 | `ST_FOUL` | §4 "otherwise" |
| 14 | `ST_PENALTY` | |
| 15–17 | `ST_THROW_IN_FORWARD/CENTER/BACK_RIGHT` | |
| 18–20 | `ST_THROW_IN_FORWARD/CENTER/BACK_LEFT` | |
| 21–30 | Period transitions and result screens | *(from the Amiga, §12)* |
| 31 | `ST_PENALTIES` (shootout) | *(from the Amiga, §12)* |
| 100 | `ST_GAME_IN_PROGRESS` | |

Note the design decision: **position is encoded in the state, not carried as data
alongside it.** Seven separate free-kick states exist because the game wants to
select a different set of aiming deltas and player arrangements for each zone, and
the cheapest way to do that in 1994 was more enum values. Anything reading
"which restart is this" is a comparison against a range —
`gameState >= 15 && gameState <= 20` is the throw-in test in
[getBallDestCoordinatesTable](../reference/swos-port/src/game/ball/ball.cpp#L4051).

`gameStatePl` is a **separate** variable (`ST_GAME_IN_PROGRESS` = 100,
`ST_STOPPED`, and 101 during foul processing) that gates the physics — see
[BALL.md](BALL.md) §5 and [SIMULATION.md](SIMULATION.md) §2.

---

## 2. The four writes

Every restart setup writes the same four things together. From
[PrepareForInitialKick](../reference/swos-port/swos/swos.asm#L104005) and
[TestFoulForPenaltyAndFreeKick](../reference/swos-port/swos/swos.asm#L107571):

```
gameState                 = <the restart>
foulXCoordinate           = <spot x>
foulYCoordinate           = <spot y>
cameraDirection           = <octant the camera faces>
playerTurnFlags           = <bitmask of allowed turn directions>
lastTeamPlayedBeforeBreak = <team taking it>
breakCameraMode           = -1
```

`foulXCoordinate` / `foulYCoordinate` are reused as the generic **restart spot** for
every stoppage, not just fouls — kick-off writes 336, 449 into them. The naming is
historical.

### `playerTurnFlags` is a direction bitmask

[MOVEMENT.md](MOVEMENT.md) §5 describes this field; the original's own comments
here pin down the bit order:

| Value | Comment in source | Bits set |
|---|---|---|
| `0x7C` | *"only allow E, SE, S, SW, W"* | 2,3,4,5,6 |
| `0xC7` | *"only allow E, NE, N, NW, W"* | 0,1,2,6,7 |
| `0x38` | *"allow only SW, S, SE"* | 3,4,5 |
| `0x83` | *"allow only NW, N, NE"* | 0,1,7 |
| `0xFF` | *"it's a free kick, allow all directions"* | all |

So bit *n* is octant *n*, with **0 = N, 1 = NE, 2 = E, 3 = SE, 4 = S, 5 = SW,
6 = W, 7 = NW** — the same octant numbering as
[PLAYER_SPRITES.md](PLAYER_SPRITES.md) §5 and [BALL.md](BALL.md) §2. This settles
the bit order, which [MOVEMENT.md](MOVEMENT.md) §5 leaves open.

The pattern is consistent: **a taker may face into the pitch and along the line,
never back out of play.** A penalty is the tightest, allowing only the three
octants pointing at goal.

---

## 3. Kick-off

[PrepareForInitialKick](../reference/swos-port/swos/swos.asm#L104005):

```
SetBallPosition(336, 449)                   // centre spot; also zeroes speed and z

if (teamStarting == teamPlayingUp) {
    team = topTeamData;    cameraDirection = 4 (south); playerTurnFlags = 0x7C
} else {
    team = bottomTeamData; cameraDirection = 0 (north); playerTurnFlags = 0xC7
}

gameState       = ST_PLAYERS_TO_INITIAL_POSITIONS
gameStatePl     = ST_STOPPED
breakCameraMode = -1
foulX, foulY    = 336, 449
lastTeamPlayedBeforeBreak = team
stoppageTimerTotal = stoppageTimerActive = 0
StopAllPlayers()
cameraXVelocity = cameraYVelocity = 0
```

`kPitchCenterX = 336`, `kPitchCenterY = 449` from
[pitchConstants.h](../reference/swos-port/src/game/pitch/pitchConstants.h).
`SetBallPosition` also zeroes `speed`, `z` and `deltaZ` and collapses the
destination onto the position ([BALL.md](BALL.md) §7).

Note kick-off goes to `ST_PLAYERS_TO_INITIAL_POSITIONS` first — a **walk-on state**
— rather than straight to a playable restart. Which team kicks off is decided by
`teamStarting` and `teamPlayingUp`, both rolled at match start
([SIMULATION.md](SIMULATION.md) §4).

---

## 4. Fouls: penalty, free kick, or neither

[TestFoulForPenaltyAndFreeKick](../reference/swos-port/swos/swos.asm#L107571),
called from [playerTacklingTestFoul](../reference/swos-port/src/game/updatePlayers/updatePlayers.cpp#L12708)
([TACKLING.md](TACKLING.md) §5). Guarded by `gameStatePl != 101` so a foul cannot
be registered while one is already being processed.

It reads the **fouled player's** position (`D1`, `D2`) and classifies:

### Penalty

| Bound | Value |
|---|---|
| Penalty areas, left | `x >= 193` |
| Penalty areas, right | `x <= 478` |
| Upper penalty area | `y <= 216` |
| Lower penalty area | `y >= 682` |

```
gameState       = ST_PENALTY
playerTurnFlags = 0x83 (upper) / 0x38 (lower)
foulX, foulY    = 336, 187   (upper white spot)
                  336, 711   (lower white spot)
PlayPenaltyComment()
```

The penalty spots are **hardcoded**, not derived from the foul position, and the
disassembly labels them *"upper white spot"* / *"lower white spot"*.

### Free kick

Only inside a band **outside** the penalty area:

| | y range |
|---|---|
| Upper free-kick band | `216 <= y < 331` |
| Lower free-kick band | `567 < y <= 682` |

Within the band, x selects one of seven zones:

| x | Zone (top team fouled) | Zone (bottom team fouled) |
|---|---|---|
| `< 153` | `LEFT1` | `RIGHT3` |
| `< 261` | `LEFT2` | `RIGHT2` |
| `< 309` | `LEFT3` | `RIGHT1` |
| `< 362` | `CENTER` | `CENTER` |
| `< 410` | `RIGHT1` | `LEFT3` |
| `< 518` | `RIGHT2` | `LEFT2` |
| else | `RIGHT3` | `LEFT1` |

The mapping is **mirrored by which team offended**, so "LEFT" and "RIGHT" are
relative to the attacking direction rather than to the pitch. `playerTurnFlags` is
set to `0xFF` — a free-kick taker may face anywhere.

### Otherwise

`gameState = ST_FOUL`. A foul outside both bands — in midfield, or right on the
byline — is a plain restart with no special state and no shooting geometry.

**These bands are the whole of SWOS's "dangerous free kick" model.** There is no
distance-to-goal calculation, no angle model, no direct/indirect distinction. Two
y-bands and seven x-slices.

---

## 5. Throw-ins

**Taker placement** —
[SetThrowInPlayerDestinationCoordinates](../reference/swos-port/swos/swos.asm#L109389):

```
if (ball.x < 336) player.x = ball.x - 3     // left half:  further left
else              player.x = ball.x + 3     // right half: further right
player.y = ball.y
player.destX, destY = player.x, player.y
```

The taker is placed **three units further from the centre line than the ball** —
that is, *off the pitch*, standing behind the touchline as a real thrower does. The
ball stays where it went out; the player steps outside to it.

**Six throw-in states** (`FORWARD` / `CENTER` / `BACK`, `LEFT` / `RIGHT`) encode
both which touchline and which direction the throw is oriented. The `LEFT`/`RIGHT`
split feeds directly into the aiming table selection in §7:
`getBallDestCoordinatesTable` picks `kLeftThrowInBallDestDelta` or
`kRightThrowInBallDestDelta` by testing `foulXCoordinate > 336`
([ball.cpp:4077-4094](../reference/swos-port/src/game/ball/ball.cpp#L4077-L4094)).

**During the throw** the player is in `PL_THROW_IN` and `hideBall` is set — the
ball sprite is suppressed because it is notionally in the thrower's hands
([BALL.md](BALL.md) §2 step 1). On release,
[CheckForThrowInAndGoalkeepersBall](../reference/swos-port/swos/swos.asm#L92759)
clears it:

```
if (controlledPlayer.state == PL_THROW_IN) {
    hideBall = 0
    state    = PL_NORMAL
    animation = playerNormalStandingAnimTable
}
CheckIfGoalkeeperClaimedTheBall()
```

Throw-in movement restrictions during the run-up are
[MOVEMENT.md](MOVEMENT.md) §5. `ThrowInDeadProc` sets a single flag
`throwInDeadVar = 1` and does nothing else.

---

## 6. Corners

Four corner geometries — upper-left, upper-right, lower-left, lower-right —
selected in [getBallDestCoordinatesTable](../reference/swos-port/src/game/ball/ball.cpp#L4150)
by comparing the restart spot against the pitch centre:

```
if (foulYCoordinate > 449) → lower corner  else upper corner
if (foulXCoordinate > 336) → right corner  else left corner
```

But note the **game state** only distinguishes `ST_CORNER_LEFT` and
`ST_CORNER_RIGHT` — two states, four aiming tables. The near/far end is recovered
from the spot coordinates at kick time rather than being carried in the state.

Corner detection itself (`l_check_for_corner_goal_out` and the corner/goal-out
subdivision) is in
[checkIfBallOutOfPlay](../reference/swos-port/src/game/ball/ball.cpp#L3522) and
belongs to [SIMULATION.md](SIMULATION.md) §5.

---

## 7. The aiming tables

[getBallDestCoordinatesTable()](../reference/swos-port/src/game/ball/ball.cpp#L4051).
Its own comment: *"Even indices are delta x, odd are delta y. They are added to ball
delta x and y. Tables are different for each type of game halt. Called when player
kicks the ball or does a pass."*

So each restart state supplies **eight (dx, dy) pairs, one per octant**, which bias
the ball's destination when the taker kicks. This is how a corner "curls in" and a
throw-in has limited range without any special-case kick code —
[SHOOTING.md](SHOOTING.md) launches the ball normally and this table shapes it.

| State | Table |
|---|---|
| Throw-in, `foulX <= 336` | `kLeftThrowInBallDestDelta` |
| Throw-in, `foulX > 336` | `kRightThrowInBallDestDelta` |
| `ST_PENALTY` / `ST_PENALTIES` | `kPenaltyBallDestDelta` |
| Corner, upper-left | `kUpperLeftCornerBallDestDelta` |
| Corner, upper-right | `kUpperRightCornerBallDestDelta` |
| Corner, lower-left | `kLowerLeftCornerBallDestDelta` |
| Corner, lower-right | `kLowerRightCornerBallDestDelta` |
| Everything else | `kDefaultDestinations` |

Values ([swos.asm:245583-245600](../reference/swos-port/swos/swos.asm#L245583)):

```
kLeftThrowInBallDestDelta   dw  250,-1000, 1000,-1000, 1000,   0, 1000,1000,
                                250, 1000,-1000, 1000,-1000,   0,-1000,-1000
kRightThrowInBallDestDelta  dw -250,-1000, 1000,-1000, 1000,   0, 1000,1000,
                               -250, 1000,-1000, 1000,-1000,   0,-1000,-1000
kPenaltyBallDestDelta       dw    0,-1000,  500,-1000, 1000,   0,  500,1000,
                                  0, 1000, -500, 1000,-1000,   0, -500,-1000
kUpperLeftCornerBallDestDelta  dw 0,-1000, 1000,-1000, 1000, 150, 1000, 300,
                                250, 1000,-1000, 1000,-1000,   0,-1000,-1000
kUpperRightCornerBallDestDelta dw 0,-1000, 1000,-1000, 1000,   0, 1000,1000,
                               -250, 1000,-1000, 1000,-1000, 150,-1000,-1000
kLowerLeftCornerBallDestDelta  dw 250,-1000,1000, -350, 1000,-150, 1000,1000,
                                  0, 1000,-1000, 1000,-1000,   0,-1000,-1000
```

`kPenaltyBallDestDelta` is the cleanest illustration: the four cardinal entries are
full ±1000 reach, but the diagonals are halved to ±500. A penalty taker gets less
angular range on the diagonals than on the straight shots.

**`kLowerRightCornerBallDestDelta` is disassembled as `db` with byte-sized garbage**
(`6, -1, 24, -4, -24, 3, ...`) where every sibling table is `dw`. That is an IDA
misparse, not a different data layout — the byte pairs `24, -4` and `-24, 3`
reassemble to `-1000` and `1000` in little-endian words. Do not read those numbers
as written.

---

## 8. There is no wall

**`grep -i wall` over the entire disassembly and the entire port source returns
zero matches.** SWOS has no wall-assembly routine, no wall entity, no
distance-from-ball enforcement, and no free-kick-specific defensive formation code.

Defenders during a free kick are positioned by the same zonal off-ball path as
always ([AI.md](AI.md) §3) — the free-kick game state changes the tactics lookup
input and the players arrange themselves accordingly, but nothing walks them into
a line. What looks like a wall on screen is emergent from the tactics grid.

This is worth stating loudly because it is the single most likely thing for a
reimplementation to over-build. The correct amount of wall code is none.

---

## 9. Constants quick reference

| Constant | Value | Meaning |
|---|---|---|
| `kPitchCenterX`, `kPitchCenterY` | 336, 449 | Centre spot; also the L/R and U/L discriminator |
| Penalty area x | `[193, 478]` | Both ends |
| Upper penalty area | `y <= 216` | |
| Lower penalty area | `y >= 682` | |
| Upper penalty spot | `(336, 187)` | *"upper white spot"* |
| Lower penalty spot | `(336, 711)` | *"lower white spot"* |
| Upper free-kick band | `216 <= y < 331` | |
| Lower free-kick band | `567 < y <= 682` | |
| Free-kick x slices | 153, 261, 309, 362, 410, 518 | Six boundaries → seven zones |
| Throw-in taker offset | `±3` from ball x | Away from centre line, i.e. off the pitch |
| `playerTurnFlags` | `0x7C`/`0xC7` kick-off, `0x38`/`0x83` penalty, `0xFF` free kick | Bit *n* = octant *n*, 0 = N |
| `cameraDirection` | 0 = north, 4 = south | |
| `breakCameraMode` | `-1` on every restart | |
| Restart states | 21 | Position encoded in the enum |

---

## 10. What this resolves, and what still needs measurement

**Confirmed as structure:**

- Every restart is a `gameState` plus the same four writes (spot, camera, turn
  flags, team). ✓
- `playerTurnFlags` bit order settled: bit *n* = octant *n*, 0 = N, clockwise. ✓
- Penalty/free-kick/nothing is decided by pure rectangular geometry on the **fouled
  player's** position. ✓
- Penalty spots are hardcoded, not derived from the foul. ✓
- Seven free-kick zones by x, mirrored by offending team; two y-bands only. ✓
- Throw-in taker is placed 3 units outside the touchline; ball is hidden during the
  throw. ✓
- Restart aiming works by a per-state table of eight directional deltas added to the
  normal kick. ✓
- Corners: two states, four aiming tables, near/far recovered from the spot. ✓
- **No wall code exists.** ✓

**Open (measurement targets, [LEGACY.md](LEGACY.md) §15):**

- **Taker selection.** Who is chosen to take a throw-in, corner, free kick or
  penalty is *not* in any routine read here. This is the largest remaining gap in
  this document and it needs its own pass through
  [updatePlayers.cpp](../reference/swos-port/src/game/updatePlayers/updatePlayers.cpp)'s
  restart paths.
- **Non-taker positioning.** §8 asserts defenders come from the normal zonal path;
  that is an inference from the absence of wall code, not a positive trace. Confirm
  by finding what, if anything, reads `gameState` inside the off-ball destination
  logic ([AI.md](AI.md) §3).
- **Penalty run-up and keeper dive.** `NextPenalty`
  ([swos.asm:104450](../reference/swos-port/swos/swos.asm#L104450)) and
  `kGoalkeeperDiveDeltas` ([amigaMode.cpp:9-14](../reference/swos-port/src/game/amigaMode.cpp#L9))
  were not traced. The dive delta tables differ between Amiga and DOS builds.
- **`ST_PLAYERS_TO_INITIAL_POSITIONS`** — how players walk to their kick-off
  positions, and how long it takes.
- **Goal kicks and keeper distribution.** `ST_KEEPER_HOLDS_BALL` is referenced by
  [BALL.md](BALL.md) §2 step 8 but the release, the throw/kick choice and the
  distribution target are unread.
- The `ST_FOUL` restart (§4, "otherwise") — what actually happens in that state.
- ~~Whether the six throw-in states' `FORWARD`/`CENTER`/`BACK` component affects
  anything beyond animation.~~ It selects **which third of the pitch** the throw is
  in, by the Y thresholds **342** and **556** — the same two the shot-on-goal test
  uses ([SHOOTING.md](SHOOTING.md) §9). And the six values are *relative to the
  taking team*, not absolute. See §12.
- ~~The remaining `kLowerRightCornerBallDestDelta` values.~~ Recoverable: all four
  corner tables are at asm:36553–36616 in the Amiga listing, correctly parsed as
  words. Transcribe from there rather than reassembling IDA's misparsed bytes.

---

## 11. Guidance for the reimplementation

- **Encode restarts as one state value with a small data table beside it**, rather
  than 21 enum members. SWOS put position in the enum because it was cheap in 1994;
  we can carry `(kind, spot, side)` as a struct and collapse seven free-kick states
  into one. Keep the *behaviour*, drop the encoding.
- **Keep the four-writes idiom.** Spot, camera, turn flags, team — written together,
  atomically, by one function per restart kind. It is the discipline that keeps
  restarts from drifting out of sync, and [SIMULATION.md](SIMULATION.md) §13 already
  recommends it.
- **Use a turn-direction bitmask.** It is one byte, it makes "taker may not face out
  of play" declarative, and it composes with the normal movement code instead of
  special-casing it.
- **Classify fouls geometrically and coarsely.** Two bands and seven slices is
  enough. A continuous distance/angle model will produce free kicks that feel
  nothing like SWOS's.
- **Bias restart kicks with a per-state delta table** rather than writing bespoke
  corner/throw-in kick routines. One table lookup keeps every restart on the same
  code path as an open-play kick ([SHOOTING.md](SHOOTING.md)).
- **Do not build a wall.** Let the off-ball tactics produce the shape. If it looks
  wrong, fix the tactics input for the free-kick state, not by adding a wall system.
- **Resolve taker selection before building any of this.** It is the one piece with
  no reference behaviour recorded, and guessing it will be visible immediately.
  Partial answer in §12: for a *free kick* the Amiga scans the fouled side's
  outfielders for whoever is nearest the opponent's goal.
- **Make the classification tree total.** Every ball leaving the pitch must produce
  exactly one restart; a fall-through is a hang. Assert on it.
- **Account for the celebration `Rand` calls** (§12) in the RNG sequence even if we
  render celebrations differently. Skipping them desynchronises every roll after the
  first goal.

---

## 12. Amiga cross-check

Traced independently through the Amiga original — [amiga/SETPIECES.md](amiga/SETPIECES.md).

### Confirmed, exactly

- **The state enum**, value for value, including the values §1 was missing (0, 1, 2,
  13, 21–30, 31). Both readings independently recover the same numbering.
- **The turn-mask bit order**: bit *n* = octant *n*, 0 = N, clockwise. The Amiga
  reads the corner masks as bit patterns and gets geometrically sensible arcs, which
  only works with §2's bit order.
- **The CPU turn restriction.** [MOVEMENT.md](MOVEMENT.md) §5's `AND 0b10111011` is
  the Amiga's `AND $BB` at asm:41546 — same constant, and the Amiga explains it:
  when the restart belongs to a CPU side, octants 2 and 6 are cleared so the CPU
  never takes a restart straight along the horizontal axis.
- **Penalty-area bounds** X 193 … 478, Y ≤ 216 / ≥ 682 — identical to §4.
- **Kick-off at (336, 449)**.
- **The aiming-table biases.** Every entry the Amiga describes matches §7's literals:
  throw-ins put ±250 on octants 0 and 4 and leave 2 and 6 alone; the penalty table
  halves all four diagonals to ±500; the upper-left corner table replaces octant 2's
  `(1000, 0)` with `(1000, 150)` and octant 3's `(1000, 1000)` with `(1000, 300)`.
  Two independent transcriptions of the same data agreeing entry by entry is about
  as good as this gets.
- **The design principle**: a set piece is not a different mechanic, it is the same
  kick through a different aiming table. Both documents reach it independently.

### The placement coordinates §2 does not have

`GameSetup` writes the same four globals this document identifies, and the Amiga
records the actual spots:

| Restart | X | Y | Camera dir | Turn mask |
|---|---|---|---|---|
| Kick-off | 336 | 449 | — | — |
| Goal kick, top goal | 396 or 276 | 154 | 4 | $7C |
| Goal kick, bottom goal | 396 or 276 | 744 | 0 | $C7 |
| Corner, top-left | 86 | 134 | 2 | $1C |
| Corner, top-right | 585 | 134 | 6 | $70 |
| Corner, bottom-left | 86 | 764 | 2 | $07 |
| Corner, bottom-right | 585 | 764 | 6 | $C1 |
| Throw-in, right touchline | 590 | ball Y | 6 | $F1 |
| Throw-in, left touchline | 81 | ball Y | 2 | $1F |

The four corner masks and the two throw-in masks are entirely new here — §2 only
has the kick-off, penalty and free-kick values. Read as bit patterns they are
exactly what you would draw by hand: each corner permits the three octants pointing
into the pitch from that flag, each throw-in the five pointing inward from that
touchline.

Note the Amiga attributes $7C / $C7 to **goal kicks** where §3 attributes them to
**kick-off**. Both are plausible — it is one mask value used in more than one
place — but if only one is right, the Amiga's is read directly off the goal-kick
placement site.

### Goal kicks and throw-ins use a *relative* encoding

This is the thing most likely to be got wrong. Values 1–2 and 15–20 describe the
restart **from the taking team's point of view**, so the same value maps to mirrored
absolute geometry for the two sides.

| Goal | Ball X ≥ 336 | Ball X < 336 |
|---|---|---|
| Top | 1 | 2 |
| Bottom | 2 | 1 |

| Touchline | Taken by | Y < 342 | 342 … 555 | Y ≥ 556 |
|---|---|---|---|---|
| Right (X 590) | left team | 15 | 16 | 17 |
| Right (X 590) | right team | 20 | 19 | 18 |
| Left (X 81) | left team | 18 | 19 | 20 |
| Left (X 81) | right team | 17 | 16 | 15 |

The named endpoints — `ST_THROW_IN_FORWARD_RIGHT` = 15 and `ST_THROW_IN_BACK_LEFT`
= 20 — confirm the reading: the value encodes *forward/back* and *left/right*
relative to the thrower. §5's `LEFT`/`RIGHT` selecting the aiming table by testing
`foulXCoordinate > 336` is therefore recovering absolute geometry from the spot,
exactly as §6 does for corners.

### New, and not covered above

- **There is no offside rule.** The Amiga found no test resembling one anywhere in
  `GameSetup` or the contest code. This matches community understanding, but it is
  worth recording as a confirmed *absence* rather than an assumption — it belongs
  next to §8's "there is no wall" as the second thing a reimplementation is likely
  to over-build.
- **The post-hit whistle suppression.** Before the goal-line branch, a ball with
  speed ≥ $300 inside X 290 … 381 with Z < 25 triggers woodwork audio *and clears
  the referee-whistle flag* — a shot that comes back off the frame and out does not
  get whistled as an ordinary out-of-play.
- **Celebration length is decided in the simulation and consumes RNG.** After a
  goal: 300 frames if the score is now level, 200 for a goal that takes a side from
  level to a one-goal lead or from a one-goal deficit to level, 100 otherwise —
  over a base of `Rand()/2 + 100` with a further `Rand()` added. **Two `Rand` calls
  per goal.** An equaliser gets three times the celebration of a fourth goal in a
  rout, which is a nice piece of design, but the reason it matters to us is the RNG
  sequence ([AI.md](AI.md) §6).
- **The free-kick taker is picked by position**, not by role: the fouled side's
  outfielders are scanned for the one nearest the opponent's goal (asm:39248–39292).
- **A partial answer flowing the other way.** The Amiga could not trace how the
  seven free-kick states are chosen and guessed they were one per octant of the
  foul's facing. §4 has the actual rule — two Y bands and six X slices, mirrored by
  the offending team — and is the better reading. Feed it back.
