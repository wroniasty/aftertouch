# AI.md

How the eighteen or twenty players nobody is controlling decide where to stand, how
the CPU side decides what to do with the ball, and the random number generator
underneath both.

The destinations produced here are consumed by [MOVEMENT.md](MOVEMENT.md); the
keeper has his own logic in [GOALKEEPER.md](GOALKEEPER.md) and is excluded from the
zonal system.

> **Provenance.** `SetPlayerWithNoBallDestination` (asm:35973), the ball-quadrant
> calculation at the tail of `UpdateBall` (asm:22125), `DoAI` (asm:45296) and `Rand`
> (asm:32600) are read directly. The quadrant limit tables and the cell coordinate
> tables are literals. What is *not* here is the content of the tactics files
> themselves — `g_tacticsTable` points at data loaded from disk, so this document
> describes the format and the addressing but cannot enumerate the tactics.

---

## 0. One-paragraph version

Off-ball positioning is a lookup, not a behaviour. The pitch is divided into a
**5 × 7 grid of thirty-five quadrants**, and the ball's predicted landing point
selects one of them. A tactic is a table of thirty-five bytes per outfield player;
indexing it by the current quadrant yields one byte, whose two nibbles are
coordinates into a 15 × 16 grid of pixel positions. That is the player's
destination. A sub-quadrant offset derived from where within the quadrant the ball
actually is nudges the result, so players drift smoothly rather than snapping
between cells. The right team reads the same tables through a double mirror — the
quadrant index is reflected and the packed byte is subtracted from $EF — so one
tactic file serves both sides. The CPU brain, `DoAI`, is a much simpler thing: once
per frame it synthesises a joystick state for its side, computing the angle to the
opponent's goal and the squared distance to it, then branching through the game
state to decide whether to run, pass, shoot, head or wait. It fakes a joystick; it
does not have privileged access to the engine.

---

## 1. The quadrant grid

Computed at the end of every `UpdateBall` (asm:22125) from `ballNextX`/`ballNextY` —
the **predicted landing point**, not the current position ([BALL.md](BALL.md) §7).
Players therefore move to where the ball is going.

### The bands

`ballXQuadrantLimits` (asm:35896) — four thresholds, five columns:

| Column | X range |
|---|---|
| 0 | < 183 |
| 1 | 183 … 284 |
| 2 | 285 … 386 |
| 3 | 387 … 488 |
| 4 | ≥ 489 |

`ballYQuadrantLimits` (asm:35901) — six thresholds, seven rows:

| Row | Y range |
|---|---|
| 0 | < 220 |
| 1 | 220 … 311 |
| 2 | 312 … 402 |
| 3 | 403 … 494 |
| 4 | 495 … 585 |
| 5 | 586 … 677 |
| 6 | ≥ 678 |

`ballQuadrantIndex = column + 5 × row`, so 0 … 34.

Columns are 102 pixels wide, rows 91–92 tall. The grid covers the playable pitch
with the outer bands absorbing the margins.

### The sub-quadrant nudge

Immediately after picking the band (asm:22143, asm:22203):

```
playerXQuadrantOffset = (ballNextX - columnLowerLimit - 0x33) × 5 / 15
playerYQuadrantOffset = (ballNextY - rowLowerLimit    - 0x2D) × 5 / 15
```

`0x33` is 51 and `0x2D` is 45 — roughly half a column and half a row — so the offset
is signed and centred, running about ±17 pixels either way. It is added to the final
destination, which is what stops eleven players teleporting sideways every time the
ball crosses a band boundary.

---

## 2. From tactic to destination

`SetPlayerWithNoBallDestination` (asm:35973) runs for every outfielder without the
ball.

### Selecting the tactic

```
tactic = g_tacticsTable[TeamGeneralInfo.field_1C]
if gameState is 1, 2 or 3:                       ; goal kick or keeper's ball
    tactic = g_tacticsTable[ tactic[0x171] ]     ; switch to the linked set-piece tactic
grid = tactic + 9
```

Byte $171 of a tactic record is **a pointer to another tactic** to use at restarts.
Tactics carry their own set-piece variant; the engine does not compute one.

The player grid starts at offset 9 into the tactic record, and each outfielder owns
35 consecutive bytes:

```
playerIndex = shirtNumber - 2                    ; 0 … 9, keeper excluded
byte = grid[playerIndex × 35 + quadrantIndex]
```

Thirty-five bytes × ten outfielders = **350 bytes of positional data per tactic**,
plus the header. That is the entire content of a SWOS tactic.

### Unpacking the byte

```
cellX = (byte >> 4) & 15
cellY =  byte       & 15
destX = playerXQuadrantsCoordinates[cellX] + playerXQuadrantOffset
destY = playerYQuadrantCoordinates[cellY]  + playerYQuadrantOffset
```

`playerXQuadrantsCoordinates` (asm:35907) — fifteen columns, 98 to 574 in steps of
34.
`playerYQuadrantCoordinates` (asm:35937) — sixteen rows, 149 to 749 in steps of 40.

So a tactic places each player on a **15 × 16 lattice** covering the pitch, for each
of 35 ball positions. The nibble packing is why the X lattice has fifteen usable
columns rather than sixteen — see the mirror below.

### The right-team mirror

The same tactic data serves both sides through two reflections (asm:36008):

```
quadrantIndex = 34 - quadrantIndex               ; point-reflect the grid
byte          = 0xEF - grid[...]                 ; reflect both nibbles
```

`0xEF` is `14 << 4 | 15`, so the subtraction — which never borrows, because cellX
never exceeds 14 — yields `(14 - cellX, 15 - cellY)`. The X lattice is limited to 15
columns precisely so this arithmetic works without a borrow.

That is an elegant trick and it is worth recognising rather than reimplementing
blindly: it is a 180° rotation of the whole formation, expressed as two subtractions.

### Final adjustments

```
destX -= 4                     ; left team
destX += 4                     ; right team (i.e. -4 then +8)
clamp destX to 81 … 590
clamp destY to 129 … 769
```

The ±4 nudge separates the two formations so opposing players in mirrored positions
do not overlap exactly.

---

## 3. The CPU brain

`DoAI` (asm:45296) runs once per frame per CPU side and does one thing: **write a
synthetic joystick state** into `TeamGeneralInfo`. Every frame it clears
`currentDirection`, `joyIsFiring`, `joyTriggered`, `quickFire` and `normalFire`
(asm:45312–45320) and then decides what to set.

### The geometry it computes first

Before any decision (asm:45333–45360):

```
goalY = 769 for the left team, 129 for the right      ; the goal it attacks
d6    = (ballX - 336)² + (ballY - goalY)²             ; squared distance to that goal
d5    = heading from the ball to the goal             ; via CalculateDeltaXAndY
```

`d5` and `d6` are then in hand for every branch: the angle to goal drives shooting
direction, the distance drives whether to shoot at all.

### The dispatch

`DoAI` is a large branch tree on `gameState`. The structure, in the order it tests:

| Situation | Behaviour |
|---|---|
| Result screens ($19, $1A) in a CPU-vs-CPU match | Press fire after 600 frames |
| Other end-of-period states | Press fire after 350 frames |
| Restart states 1, 2, 3, or throw-ins $F–$14 | No facing required; skip to positioning |
| Foul ($D) or free kick (6–$C) | Aim at the goal: `direction = ((d5 + 16) & 255) >> 5` |
| Penalty ($E) or shootout | Dedicated penalty branch (asm:45700) |
| Goal scored (0) | Celebration branch |
| Live play | The main branch (asm:45694) |

The restart branches are gated on a timer first (asm:45431):

```
if (stoppageTimer & 63) + 100 > stoppageTimerActive:  do nothing yet
```

which makes the CPU wait a variable 100–163 frames before acting on a restart —
deterministic but not obviously periodic, so restarts do not look mechanical.

### In live play

The main branch (asm:45694 onward) works through, roughly:

1. Penalty handling first (asm:45700).
2. `FindClosestPlayerToBallFacing` (asm:46754) — pick which of its own players to
   control.
3. If an opponent is near (asm:45744), consider `AIHeader` (asm:46576) for an aerial
   ball, or a tackle.
4. If our own player is closest to the ball (asm:45987), go for it.
5. If nobody is near (asm:46312), fall back to positional play.
6. `AISetDirectionAllowed` (asm:46524) filters the chosen direction against the turn
   mask before it is written.
7. A spin branch (asm:46450) applies aftertouch — **on kicks, the CPU uses the same
   aftertouch mechanic as the player**, through the same synthetic joystick.

That last point is the design principle worth carrying forward: the CPU has no
special-cased shooting or curling. It presses the same buttons. Anything a CPU
player does is by construction something a human could do.

**With two deliberate exceptions**, both in the pass path and both found in
`DoPass`:

- **CPU passes get no aftertouch window.** `PlayerKickingBall` opens the window
  unconditionally, but `DoPass` opens it only when `playerNumber != 0`, i.e. for a
  human side (asm:35011). The CPU can curl a shot; it cannot curl a pass.
- **CPU passes get an accuracy roll that human passes do not.** Driven by the passer's
  Passing attribute against the frame counter (asm:34877).

Both are documented in [PASSING.md](PASSING.md) §4. They are the only places found so
far where the two input paths are not interchangeable, and they run in the CPU's
*disfavour* — which is consistent with the principle rather than a violation of it.
`sub_118290` (asm:44631) is a third CPU-only routine, but it only synthesises a fire
press, so it stays within the joystick contract.

`AI_maxStoppageTime` (asm:32206), `AI_counter` and `AI_beginPlayTimer` are the
brain's own timers, decremented at the top of every call.

---

## 4. Random-direction suppression

At asm:45575 there is a branch labelled `random_direction_disallowed`, reached from
the main dispatch. The AI's default when it has nothing better to do is to take a
`Rand`-derived direction, and this branch is the veto — it exists so a CPU player
does not jitter when idle. The precise conditions were not fully traced and this is
a measurement target.

---

## 5. The random number generator

`Rand` (asm:32600) is **not** an LCG and not a shift register. It is a table walk:

```
i = counter1                       ; byte at 10F43E
if i == 0:
    counter2 += 1
    key = table[counter2]          ; refresh the XOR key on wrap
d0  = table[i] XOR key
counter1 += 1                      ; wraps at 256
return d0
```

`table` is a fixed 256-byte block at asm:32590 (`unk_10F2F9`). Three bytes of state:
a position counter, a key counter, and the current key.

Properties that matter:

- **The period is 65 536 calls** — 256 positions × 256 keys — and it is completely
  deterministic with no seeding. Every match that consumes the same number of `Rand`
  calls in the same order sees the same sequence.
- It returns a **byte**, and callers mask it: `& 31` for the tackle contest
  ([CONTEST.md](CONTEST.md) §3), `& $18` for the keeper's penalty reach, `& 3` for
  cards, `& 1` for coin flips.
- It is used far less than one might expect. The two most consequential rolls in the
  game — the goal/save resolution and the goalmouth rebound scatter — **do not call
  it**, reading the frame counter instead ([GOALKEEPER.md](GOALKEEPER.md) §4,
  [BALL.md](BALL.md) §6).

Confirmed consumers: `InitGame` (pitch and weather selection, ×4), `maingame`
(kick-off coin flip), `sub_109FF2` (crowd, ×3), `GameSetup` (restart camera and
crowd, ×3), `sub_111388` (cards), `ShouldGoalkeeperDive` (penalty reach), `DoAI`
(idle direction), `CalculateIfPlayerWinsBall` (tackle contest).

**The RNG call order is part of the simulation state.** Because there is no seeding
and the state is three bytes, our engine can reproduce it exactly — but only if we
consume it in exactly the same places. Adding one speculative `Rand` call for a
cosmetic effect desynchronises every subsequent gameplay roll.

---

## 6. Constants quick reference

| Symbol | Line | Value | Meaning |
|---|---|---|---|
| `ballXQuadrantLimits` | 35896 | 183, 285, 387, 489 | 5 columns |
| `ballYQuadrantLimits` | 35901 | 220, 312, 403, 495, 586, 678 | 7 rows |
| Sub-quadrant X centre | 22147 | 0x33 (51) | |
| Sub-quadrant Y centre | 22207 | 0x2D (45) | |
| Sub-quadrant scale | 22149 | × 5 / 15 | ≈ ±17 px |
| `playerXQuadrantsCoordinates` | 35907 | 98 … 574 step 34 | 15 columns |
| `playerYQuadrantCoordinates` | 35937 | 149 … 749 step 40 | 16 rows |
| Tactic grid offset | 35993 | +9 | Start of positional data |
| Bytes per player | 36000 | 35 | One per quadrant |
| Set-piece tactic link | 35988 | tactic + $171 | |
| Right-team quadrant mirror | 36010 | 34 − index | |
| Right-team cell mirror | 36013 | $EF − byte | |
| Formation separation | 36026 | ∓4 px | |
| Destination clamp | 36030 | X 81…590, Y 129…769 | |
| CPU restart delay | 45431 | `(stoppageTimer & 63) + 100` | 100–163 frames |
| CPU result-screen wait | 45383 | 600 frames | |
| CPU other-state wait | 45392 | 350 frames | |
| `Rand` table | 32590 | 256 bytes | Period 65 536 |

---

## 7. What this resolves, and what still needs measurement

Confirmed:

- ✓ Off-ball positioning is a pure table lookup with no behaviour, no steering and
  no collision avoidance.
- ✓ The grid is 5 × 7 = 35, keyed on the ball's **predicted landing point**.
- ✓ A tactic is 35 bytes per outfielder, nibble-packed into a 15 × 16 lattice.
- ✓ Exact quadrant boundaries and lattice coordinates.
- ✓ The sub-quadrant nudge and its scale.
- ✓ The right team is served by a 180° double mirror, and the 15-column X lattice
  exists to make that mirror borrow-free.
- ✓ Tactics carry a linked set-piece tactic at byte $171.
- ✓ The CPU drives a synthetic joystick and has no privileged mechanics.
- ✓ The CPU applies aftertouch to *kicks* through the same code path as a human, but
  gets no aftertouch on passes and an accuracy penalty on them instead.
- ✓ `sub_111B98` (asm:36982) is the human-side player-selection routine: it computes
  every player's `ballDistance` and picks the nearest eligible one, excluding the
  pass receiver and the player who just kicked. See [PASSING.md](PASSING.md) §6.
- ✓ `Rand` is a 256-byte table walk with an XOR key, period 65 536, unseeded.
- ✓ The two biggest rolls in the game bypass `Rand` entirely.

Open (measurement targets, [../LEGACY.md](../LEGACY.md) §15):

- **The tactics data itself.** `g_tacticsTable` points at disk-loaded records. The
  format is now known well enough to parse them; the files are not in this
  repository.
- The tactic record header, bytes 0–8, and everything between the grid and $171.
- The conditions under which `random_direction_disallowed` (asm:45575) fires.
- The full contents of the `DoAI` live-play branch. It is ~1 400 lines and this
  document maps its structure rather than its detail; a dedicated pass is warranted
  before we build a CPU opponent.
- Whether `field_1C` (tactic index) matches the on-disk tactic ordering.
- The 256-byte `Rand` table's contents were not transcribed; they are at asm:32590
  and should be extracted verbatim when we implement it.

---

## 8. Guidance for the reimplementation

- **Build the zonal grid before building any AI behaviour.** It is thirty lines of
  code and it produces recognisably SWOS-shaped team movement on its own. Steering
  behaviours, flocking and pathfinding are all wrong answers here.
- **Keep the grid keyed on the predicted landing point.** Keying it on the ball's
  current position makes the team lag behind every long pass.
- **Implement the mirror as the original does**, rather than storing two tactics.
  Getting `$EF −` right first time is easier than reconciling two copies later, and
  it documents itself.
- **Give the CPU a synthetic joystick and nothing else.** This is a hard
  architectural rule worth enforcing with a type: the AI writes an input struct, the
  same struct a human writes, and the simulation cannot tell them apart. It makes
  every AI behaviour automatically reachable by a human and it makes replays
  uniform.
- **Extract the `Rand` table verbatim and preserve the call order.** Treat RNG
  consumption as part of the trace. If a cosmetic system needs randomness, give it
  its own generator.
- **Do not add `Rand` where the original reads the clock.** Deterministic
  pseudo-randomness from the frame counter is a feature: it makes outcomes
  reproducible without threading a generator through the physics.
