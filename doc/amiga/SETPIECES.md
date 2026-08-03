# SETPIECES.md

How the ball goes out of play, what restart that produces, where everything is
placed, and how the kick from a restart is aimed differently from a kick in open
play.

The stoppage timing that surrounds all of this is [TIMING.md](TIMING.md) §4; the
foul test that produces a free kick or penalty is [CONTEST.md](CONTEST.md) §4.

> **Provenance.** `GameSetup` (asm:41156) and `GetBallDestCoordinatesTable`
> (asm:39793) are read directly, and the eight aiming-delta tables are literals at
> asm:36496–36616. All restart coordinates are immediate operands. What is
> reconstructed rather than read is the `gameState` enumeration — only six values
> are named in the listing and the rest are recovered from placement sites, listed
> in [STATE.md](STATE.md) §3.

---

## 0. One-paragraph version

`GameSetup` is called from the ball physics the moment the ball leaves the field of
play, and it is one long geometric decision tree. It first tests for a goal — ball
below the crossbar, between the posts, past the goal line — and if so awards it,
sets up a kick-off, and picks a crowd-cheer duration that depends on how dramatic
the score change was. Otherwise it works out which line was crossed and who touched
last, and from that selects one of goal kick, corner or throw-in, writing four things
each time: the ball's placement, a camera direction, a **turn-restriction mask** for
the taker, and the `gameState` value. The restart is then executed by the normal
kicking code, with one substitution: `GetBallDestCoordinatesTable` swaps the
open-play directional offset table for a restart-specific one, so a corner taker
pushing "up" aims somewhere quite different from an open-play player pushing "up".

---

## 1. Classification

`GameSetup` (asm:41156) is entered from `UpdateBall` when the ball crosses a
boundary. Tests run in this order.

### Goal

```
z <= 15  and  302 <= x-1 <= 366  and  y in the goal region
```

(asm:41166–41172). Which goal is decided by comparing `y` against 449, the halfway
line. The scoring side is the one whose goal was *not* crossed, with a check for
own-goals via `lastPlayerPlayed` and `lastKeeperPlayed` (asm:41176–41186).

The goal is then followed by a **crowd-reaction duration** chosen from the score
context (asm:41236–41266):

| Situation | Frames |
|---|---|
| Score is now level | 300 |
| The goal took a side from level to a one-goal lead, or a one-goal deficit to level | 200 |
| Otherwise | 100 |
| Base, before the branch | `Rand()/2 + 100`, then a further `Rand()` added |

So an equaliser gets three times the celebration of a fourth goal in a rout. This is
presentation, but it is decided inside the simulation and it consumes two `Rand`
calls, which matters for the RNG sequence ([AI.md](AI.md) §5).

Kick-off is then placed at **(336, 449)** with `gameState` 0.

### Goal kick, corner, throw-in

If not a goal, the decision is driven by which line was crossed and which side
touched the ball last (asm:41414 onward):

| Line crossed | Last touch | Restart |
|---|---|---|
| Goal line (y < 129 or y > 769) | Attacking side | **Goal kick** |
| Goal line | Defending side | **Corner** |
| Touchline (x < 81 or x > 590) | Either | **Throw-in** |

Before the goal-line branch there is a **post-hit check** (asm:41180 region,
asm:41426): if ball speed is at least $300, the ball is within X 290 … 381, and Z is
under 25, a woodwork sound is triggered and the "referee whistle" flag is cleared —
so a shot that comes back off the frame and out does not get a whistle.

---

## 2. Placement

Every restart writes the same four globals (asm:41573–41583): `foulXCoordinate`,
`foulYCoordinate`, `cameraDirection` and `playerTurnFlags`.

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

### The state values are relative, not absolute

Both goal kicks and throw-ins use a **symmetric encoding**: the `gameState` value
describes the restart from the *taking team's* point of view, so the same value
means opposite absolute geometry for the two sides. This is easy to get wrong.

For goal kicks (asm:41404, asm:41419, asm:41451, asm:41477), the ball is placed at
X 396 or 276 depending on which half of the goal area it went out in, and the state
is:

| Goal | Ball X ≥ 336 | Ball X < 336 |
|---|---|---|
| Top (left team's) | 1 | 2 |
| Bottom (right team's) | 2 | 1 |

For throw-ins the same inversion applies, crossed with which third of the pitch the
ball went out in — Y thresholds **342** and **556** (asm:41474). `a6` at that point
holds the team *receiving* the throw:

| Touchline | Taken by | Y < 342 | 342 … 555 | Y ≥ 556 |
|---|---|---|---|---|
| Right (X 590) | left team | $F | $10 | $11 |
| Right (X 590) | right team | $14 | $13 | $12 |
| Left (X 81) | left team | $12 | $13 | $14 |
| Left (X 81) | right team | $11 | $10 | $F |

The two named endpoints of the range — `ST_THROW_IN_FORWARD_RIGHT` = $F and
`ST_THROW_IN_BACK_LEFT` = $14 (asm:291) — confirm the reading: the value encodes
*forward/back* and *left/right* relative to the thrower, which is exactly the
symmetry the table shows.

342 and 556 are the same two thresholds the shot-on-goal test uses
([KICKING.md](KICKING.md) §2) — the pitch is divided into the same three zones for
both purposes.

### The turn mask

`playerTurnFlags` is an eight-bit mask, one bit per octant, restricting which
directions the taker may face ([MOVEMENT.md](MOVEMENT.md) §4). Reading the corner
values as bit patterns:

| Restart | Mask | Binary | Permitted octants |
|---|---|---|---|
| Corner, top-left | $1C | 00011100 | 2, 3, 4 |
| Corner, top-right | $70 | 01110000 | 4, 5, 6 |
| Corner, bottom-left | $07 | 00000111 | 0, 1, 2 |
| Corner, bottom-right | $C1 | 11000001 | 0, 6, 7 |
| Throw-in, right | $F1 | 11110001 | 0, 4, 5, 6, 7 |
| Throw-in, left | $1F | 00011111 | 0, 1, 2, 3, 4 |

Each corner permits exactly the three octants pointing into the pitch from that
flag; each throw-in permits the five pointing inward from that touchline. The masks
are geometrically exactly what you would draw by hand.

There is one modification: if the restart belongs to a CPU side (`TeamGeneralInfo`
+$04 is zero), the mask is ANDed with $BB (asm:41546), clearing octants 2 and 6 —
the CPU is forbidden from taking a restart straight along the horizontal axis.

### Ball placement

`SetBallPosition` (asm:21138) puts the ball at the coordinates, and — for everything
except a keeper's ball — the stoppage machine calls it during break stage 1
(asm:37605). Keeper's ball (`gameState` 3) skips it because the keeper is already
holding it.

`PrepareForInitialKick` (asm:40989) arranges the kick-off formation.

---

## 3. Restart aiming

This is the mechanism that makes set pieces feel different, and it is a single
substitution.

Every kick aims at `ballPosition + table[direction]`
([KICKING.md](KICKING.md) §1). In open play the table is
`defaultPlayerDestinations` — a clean ±1000 in each octant. During a restart,
`GetBallDestCoordinatesTable` (asm:39793) returns a different table:

| `gameState` | Table | Selector |
|---|---|---|
| $F … $14 | `leftThrowInBallDestDelta` / `rightThrowInBallDestDelta` | `foulXCoordinate` > 336 → right |
| $E, $1F | `penaltyBallDestDelta` | |
| 4, 5 | one of four corner tables | `foulYCoordinate` > 449 → lower; `foulXCoordinate` > 336 → right |
| anything else | `defaultPlayerDestinations` | |

### What the tables actually change

All eight tables are eight (dx, dy) pairs, and most entries are the same ±1000 as
the default. The differences are surgical.

**Throw-ins** (asm:36505, asm:36521). Octant 0 becomes (±250, −1000) and octant 4
becomes (±250, +1000) — the sign chosen so the bias is *into* the pitch. Throwing
"straight up the line" is nudged infield by a quarter. Octant 2 is (1000, 0) and
octant 6 is (−1000, 0) unchanged, so throwing directly infield is unmodified.

**Penalty** (asm:36537). The diagonals are halved: octant 1 becomes (500, −1000),
octant 3 (500, 1000), octant 5 (−500, 1000), octant 7 (−500, −1000). Aiming into a
corner of the goal from the spot is compressed toward the centre, which narrows the
angle you can actually achieve and is what makes SWOS penalties a matter of timing
rather than aim.

**Corners** (asm:36553–36616). Each of the four tables biases two or three octants
toward the near post and the penalty spot. The upper-left table, for instance,
replaces octant 2's (1000, 0) with (1000, 150) and octant 3's (1000, 1000) with
(1000, 300), pulling an outswinging corner down toward the six-yard box; octant 4
becomes (250, 1000). The other three are the corresponding reflections.

The design point: **a set piece is not a different mechanic, it is the same kick
through a different aiming table.** Everything else — launch speed, aftertouch,
physics — is identical. That is worth preserving exactly.

---

## 4. Constants quick reference

| Symbol | Line | Value | Meaning |
|---|---|---|---|
| Goal test, Z | 41166 | ≤ 15 | Under the bar |
| Goal test, X | 41168 | 302 … 366 | Between the posts |
| Halfway line | 41174 | 449 | Selects which goal |
| Kick-off position | 41525 | (336, 449) | |
| Goal kick X | 41404, 41419 | 396 / 276 | |
| Goal kick Y | 41405 | 154 / 744 | |
| Corner X | 41449, 41463 | 585 / 86 | |
| Corner Y | 41450 | 134 / 764 | |
| Throw-in X | 41487, 41497 | 590 / 81 | |
| Throw-in thirds | 41474 | 342, 556 | Same as shot-on-goal gates |
| Post-hit speed gate | 41426 | ≥ $300 | |
| Post-hit X band | 41428 | 290 … 381 | |
| Post-hit Z gate | 41432 | < 25 | |
| Celebration, equaliser | 41244 | 300 frames | |
| Celebration, one-goal swing | 41254 | 200 frames | |
| Celebration, otherwise | 41248 | 100 frames | |
| CPU turn-mask restriction | 41546 | AND $BB | Clears octants 2, 6 |
| `defaultPlayerDestinations` | 36496 | ±1000 | Open play |
| Throw-in infield bias | 36505 | ±250 | On octants 0 and 4 |
| Penalty diagonal compression | 36537 | ±500 | On octants 1, 3, 5, 7 |
| Corner tables | 36553–36616 | 150 / 250 / 300 / 350 | Near-post and spot bias |

---

## 5. What this resolves, and what still needs measurement

Confirmed:

- ✓ Out-of-play classification is a geometric decision tree with no randomness.
- ✓ Goal detection geometry, exactly.
- ✓ All restart placement coordinates.
- ✓ The three-way throw-in split, and that it reuses the shot-on-goal Y thresholds.
- ✓ Goal-kick and throw-in `gameState` values are relative to the taking team, so the
  same value means mirrored geometry for the two sides.
- ✓ The turn masks, and that they are exactly the geometrically sensible arcs.
- ✓ CPU sides get an extra restriction clearing the horizontal octants.
- ✓ Set-piece aiming is the open-play kick with a substituted offset table.
- ✓ All eight tables and precisely which octants each modifies.
- ✓ Penalties compress the diagonals by half.
- ✓ Celebration length depends on the significance of the goal, and consumes two
  `Rand` calls.

Open (measurement targets, [../LEGACY.md](../LEGACY.md) §15):

- Free kicks: `gameState` 6 … $C are eight values, presumably one per octant of the
  direction the foul faced, but the placement path was not traced. `GameSetup` does
  not appear to write them — they come from the foul test.
- The seven `gameState` values $15 … $1E covering period transitions.
- Whether `word_10F14A` (tested at asm:41455, asm:41481) is a "swap ends" flag; it
  selects between two placements for the same situation.
- The camera-direction values' meaning; they are recorded but not interpreted here.
- Whether the offside rule exists at all. No test resembling one was found in
  `GameSetup` or in the contest code, which would mean SWOS 96/97 has no offside —
  consistent with community understanding, but worth confirming as an absence rather
  than assuming it.

---

## 6. Guidance for the reimplementation

- **Implement restarts as table substitution, not as separate mechanics.** One kick
  routine, one aiming-table selector. Special-casing corners into their own code path
  is how set pieces end up feeling detached from the rest of the game.
- **Keep the turn masks.** They are cheap, they are geometrically obvious, and they
  are what stops a throw-in being taken backwards over the touchline.
- **Preserve the CPU's extra mask restriction.** It is a small thing that makes CPU
  restarts read as deliberate.
- **Put the classification tree in `at_core` and make it total.** Every ball leaving
  the pitch must produce exactly one restart; a fall-through is a hang. Assert on it.
- **Reuse the 342/556 thresholds as named constants** shared with the shooting code,
  rather than duplicating them. They are the same pitch division and should stay in
  step if either is ever retuned.
- **Account for the celebration `Rand` calls** in the RNG sequence even if we render
  celebrations differently. Skipping them desynchronises everything after the first
  goal.
