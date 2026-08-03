# BALL.md

The ball as a physical object: how one tick moves it, what slows it, what makes it
bounce, what happens when it meets a boundary or the goal frame, and how the engine
predicts where it will land. Traced through the reference DOS port in
[../reference/swos-port/](../reference/swos-port/).

This document covers the ball **between** events. The kick that launches it is
[SHOOTING.md](SHOOTING.md); the curl applied for a few ticks afterwards is
[AFTERTOUCH.md](AFTERTOUCH.md); the dribble that keeps re-aiming it is
[CONTROL.md](CONTROL.md); the out-of-play classification that ends its journey is
[SIMULATION.md](SIMULATION.md) §5. Everything here is what happens in between,
every tick, regardless of who last touched it.

> **Provenance.** `updateBall` and `calculateNextBallPosition` survive as
> register-level decompilation; the control flow below is read straight from it and
> is reliable. Unusually for this project, **the physics constants are present as
> real values in the port's source, not as data-segment addresses** — they are set
> in [amigaMode.cpp](../reference/swos-port/src/game/amigaMode.cpp) and
> [game.cpp](../reference/swos-port/src/game/game.cpp#L1388-L1401). They are still
> worth confirming against traces, but they are considerably better than guesses.
> Read to understand the design; write our own code.

> **Second oracle.** [amiga/BALL.md](amiga/BALL.md) covers the same subsystem traced
> through the Amiga original's `UpdateBall` (asm:21593). Every constant in §9 below
> is independently confirmed there as a *named literal* in the Amiga data segment,
> and the Amiga reading also settles four of the open questions in §10. Where the
> two disagree — the crossbar, the goalmouth region, the barrier's activation gate —
> §12 records it.

---

## 0. One-paragraph version

The ball is a `Sprite` with 16.16 fixed-point `x`, `y`, `z` and matching
`deltaX/Y/Z`, plus a scalar `speed` and a destination `(destX, destY)`. Each tick
[updateBall()](../reference/swos-port/src/game/ball/ball.cpp#L13) recomputes
`deltaX`/`deltaY` from speed and heading, **subtracts a friction constant from
`speed`** — a different constant on the ground than in the air, and the ground one
is modified by pitch type — then integrates position. Vertical motion is separate:
`z += deltaZ` and, when `z` would go negative, the ball **bounces**: horizontal
speed is cut by a pitch-dependent factor, `deltaZ` is negated and reduced by
another pitch-dependent factor, and if the result falls below a fixed threshold the
ball is declared to have stopped bouncing. Reflections off the invisible dead-ball
barrier and off the goal frame are expressed not as velocity negation but as
**mirroring the destination point about the current position** — the same
destination-driven idiom the whole engine uses. Separately,
[calculateNextBallPosition()](../reference/swos-port/src/game/ball/ball.cpp#L4206)
runs the ballistic arc forward to landing and publishes `ballNextX/ballNextY`, the
predicted landing spot that the AI and keeper chase.

---

## 1. State

The ball is [`ballSprite`](../reference/swos-port/src/sprites/Sprite.h), a full
`Sprite`. The fields that matter here:

| Field | Offset | Type | Meaning |
|---|---|---|---|
| `x`, `y`, `z` | +30, +34, +38 | `FixedPoint` (16.16) | Position. `z` is height. |
| `deltaX`, `deltaY`, `deltaZ` | +46, +50, +54 | `FixedPoint` | Per-tick position increment. |
| `speed` | +44 | `int16` | Scalar speed. Drives `deltaX/Y` via `CalculateDeltaXAndY`. |
| `direction` | +42 | `int16` | Octant, 0–7. |
| `fullDirection` | +82 | `uint16` | Fine heading, 0–255. |
| `destX`, `destY` | +58, +60 | `int16` | Aim point, in whole pitch units. |
| `frameIndicesTable` | +18 | ptr | `ballStaticFrameIndices` or `ballMovingFrameIndices`. |
| `imageIndex` | +70 | `int16` | `-1` hides the ball. |

**The integer part is the pitch coordinate.** Comparisons throughout `updateBall`
read `word ptr [esi+(Sprite.x+2)]` — the high word of the 16.16 fixed point — and
compare it against whole numbers like 53, 618, 129. So the barrier and goal-frame
constants in this document are in **whole pitch units**, and the fractional part
never participates in a boundary test.

Two globals are the output of the landing predictor:

| Global | Meaning |
|---|---|
| `ballNextX`, `ballNextY` | Predicted landing point, whole pitch units |
| `ballNextYGroundY` | Related y-prediction consumed heavily by [updatePlayers.cpp](../reference/swos-port/src/game/updatePlayers/updatePlayers.cpp#L1241) |

---

## 2. The per-tick pipeline

[updateBall()](../reference/swos-port/src/game/ball/ball.cpp#L13), in order:

1. **Visibility.** If `hideBall`, set `imageIndex = -1` on ball and shadow and skip
   straight to the delta calculation ([:16-27](../reference/swos-port/src/game/ball/ball.cpp#L16-L27)).
2. **Animation select.** If `deltaX` and `deltaY` are both zero use
   `ballStaticFrameIndices`, otherwise `ballMovingFrameIndices`, resetting
   `frameIndex` on a change ([:29-81](../reference/swos-port/src/game/ball/ball.cpp#L29-L81)).
3. **Roll animation rate is speed-derived**
   ([:83-137](../reference/swos-port/src/game/ball/ball.cpp#L83-L137)):
   `cycleFramesTimer -= (speed >> 9) + 1`, and when it goes negative advance
   `frameIndex` and reload from `frameDelay`. The ball visibly rolls faster when
   struck harder, with no separate animation state.
4. **Heading → deltas.** `CalculateDeltaXAndY` converts `speed` and heading into
   `deltaX`/`deltaY` ([:202-211](../reference/swos-port/src/game/ball/ball.cpp#L202-L211)).
5. **Octant from fine heading** ([:220-234](../reference/swos-port/src/game/ball/ball.cpp#L220-L234)):
   ```
   fullDirection = D0                       // 0..255
   direction     = ((D0 + 16) & 0xFF) >> 5  // 0..7
   ```
   32 units per octant, `+16` biasing to nearest rather than truncating. Same
   octant convention as [PLAYER_SPRITES.md](PLAYER_SPRITES.md) §5.
6. **Friction** — §3.
7. **Integrate x and y**: `x += deltaX`, `y += deltaY`
   ([:299-324](../reference/swos-port/src/game/ball/ball.cpp#L299-L324)).
   The pre-integration `x, y, z` are saved in `D5, D6, D7` and are restored
   wholesale by every collision path below. **Collision in this engine is
   "reject the move", not "resolve the overlap".**
8. **Keeper holding** — if `gameState == ST_KEEPER_HOLDS_BALL` the ball is pinned
   to the keeper ([:325-475](../reference/swos-port/src/game/ball/ball.cpp#L325-L475)).
9. **Integrate z, and bounce** — §4.
10. **Dead-ball barrier** — §5.
11. **Goal frame** — §6.
12. **Shadow** ([:1688](../reference/swos-port/src/game/ball/ball.cpp#L1688)).
13. **Ball quadrant** ([:1823](../reference/swos-port/src/game/ball/ball.cpp#L1823),
    [:1964](../reference/swos-port/src/game/ball/ball.cpp#L1964)) — a coarse
    position classification the AI reads.

---

## 3. Friction

[ball.cpp:240-297](../reference/swos-port/src/game/ball/ball.cpp#L240-L297). One
subtraction from the scalar `speed`, clamped at zero:

```
decel = kBallGroundConstant
if (!topTeam.playerHasBall && !bottomTeam.playerHasBall)
    decel += pitchBallSpeedFactor          // pitch type, only for a loose ball
if (z.whole() != 0)
    decel  = kBallAirConstant              // REPLACES, does not add

speed -= decel
if (speed < 0) speed = 0
```

Three things worth stating plainly, because they are all non-obvious:

- **Airborne friction replaces ground friction rather than adding to it**, and the
  pitch factor is discarded with it. A ball in flight does not care what the pitch
  is made of. Only rolling does.
- **The pitch factor only applies to a loose ball.** If either team has
  `playerHasBall` set, the pitch type stops mattering. Dribbling is unaffected by
  the surface; loose balls are not.
- **`pitchBallSpeedFactor` is negative for fast pitches.** On Frozen it is `-3`
  (DOS), so the subtraction is `13 - 3 = 10`: the ball keeps its speed longer. This
  is the mechanism behind the community claim that long shots matter more on Frozen
  ([LEGACY.md](LEGACY.md) §6).

---

## 4. Height and the bounce

[ball.cpp:476-567](../reference/swos-port/src/game/ball/ball.cpp#L476-L567).

```
z += deltaZ
if (z >= 0) goto assign_delta_z         // still airborne, nothing to do

// --- ground contact ---
speed -= (speed * ballSpeedBounceFactor) >> 8    // horizontal loss
z.whole() = 0                                     // clamp to ground
d = -deltaZ                                       // reflect
d -= (d >> 8) * ballBounceFactor                  // vertical restitution loss
d |= 1                                            // force non-zero

if (d > 0xA000)  { play bounce sample; deltaZ = d }
else             { deltaZ = 0 }                   // ball has settled
```

**Two independent restitution factors**, both indexed by pitch type:
`ballSpeedBounceFactor` costs the ball *horizontal* speed on contact, and
`ballBounceFactor` costs it *vertical* rebound. A pitch can be sticky in one axis
and lively in the other, and the tables in §8 show that Muddy and Wet are exactly
that — the highest horizontal penalty (80) paired with the highest vertical
retention values.

**`0xA000` = 40960 is the settle threshold.** Below that rebound velocity the ball
is simply declared dead vertically. There is no asymptotic bounce sequence; the
engine cuts it off. The bounce sound is gated on the same test, so a ball that
settles does so silently.

**`d |= 1`** forces the rebound odd, which keeps it non-zero after the restitution
subtraction. A tiny detail with a real consequence: a ball landing with almost no
vertical velocity still gets a `deltaZ` of at least 1 rather than exactly 0, and
therefore still counts as "in the air" for the `z.whole() != 0` friction test on
some subsequent tick. Worth reproducing rather than tidying up.

---

## 5. The dead-ball barrier

[ball.cpp:572-708](../reference/swos-port/src/game/ball/ball.cpp#L572-L708).
**Active only when `gameStatePl != ST_GAME_IN_PROGRESS`** — during throw-ins,
corners, free kicks and every other stoppage, an invisible box fences the ball in:

| Axis | Allowed whole-unit range |
|---|---|
| x | `[53, 618]` |
| y | `[100, 799]` |

On violation ([:611](../reference/swos-port/src/game/ball/ball.cpp#L611),
[:674](../reference/swos-port/src/game/ball/ball.cpp#L674)):

```
destX = 2*x - destX      // mirror the destination about current position
speed >>= 1              // halve
x, y, z = D5, D6, D7     // restore the whole pre-integration position
```

**Reflection is expressed in destination space, not velocity space.** The engine
never negates `deltaX`. It mirrors the *aim point* about the ball's current
position and lets the next tick's `CalculateDeltaXAndY` re-derive the deltas. This
is the same idiom as `reverseDestXDirection` (§7) and it is the single most
important structural thing to copy: **the ball is always travelling toward a
destination, and every "physics" event in SWOS is a rewrite of that destination.**

During normal play this barrier is off, and the ball leaving the pitch is handled
by [checkIfBallOutOfPlay](../reference/swos-port/src/game/ball/ball.cpp#L3007)
instead ([SIMULATION.md](SIMULATION.md) §5).

---

## 6. The goal frame

[ball.cpp:709-1690](../reference/swos-port/src/game/ball/ball.cpp#L709-L1690) —
roughly a fifth of the whole function, and the messiest part of it.

**Geometry**, in whole pitch units (goal x-extent from
[pitchConstants.h](../reference/swos-port/src/game/pitch/pitchConstants.h)):

| Constant | Value | Meaning |
|---|---|---|
| `kGoalLeft` / `kGoalRight` | 303 / 367 | Goalmouth x-extent |
| `kGoalAttemptLeft` / `kGoalAttemptRight` | 240 / 431 | Wider "attempt on goal" band |
| `kPitchCenterX` / `kPitchCenterY` | 336 / 449 | Centre spot |
| — | y < 129 or y > 769 | Enters the goal-or-goal-out region |
| — | 112 < y ≤ 128 | Inside the **upper** goal in y |
| — | 770 ≤ y < 785 | Inside the **lower** goal in y |

Within that region the code distinguishes ball-in-net, top-of-goal, goal-out and
frame contact, and separately handles penalty goals
([:1296](../reference/swos-port/src/game/ball/ball.cpp#L1296)) and own goals
([:1343](../reference/swos-port/src/game/ball/ball.cpp#L1343)).

**Frame contact** resolves one of two ways:

- **Bar** — [`l_reverse_delta_z`](../reference/swos-port/src/game/ball/ball.cpp#L1463):
  `deltaZ = -deltaZ`, restore `z` from `D7`, and nudge the saved `y` by `+0x10000`
  (one whole unit) to push the ball clear.
- **Post** — [`cseg_7C940`](../reference/swos-port/src/game/ball/ball.cpp#L1442):
  `reverseDestYDirection()`, `resetBothTeamSpinTimers()`, then offset `destX`.

Both then converge and apply a **25 % speed penalty**
([:1528-1542](../reference/swos-port/src/game/ball/ball.cpp#L1528-L1542)):
`speed -= speed >> 2`, and restore `x, y` from `D5, D6`.

**Note that hitting the frame kills aftertouch** — `resetBothTeamSpinTimers()` on
the post path is the same abort described in [AFTERTOUCH.md](AFTERTOUCH.md) §3.

### The commentary coin-flip

[ball.cpp:1484-1523](../reference/swos-port/src/game/ball/ball.cpp#L1484-L1523).
Which line of commentary plays after frame contact is chosen by `Rand() & 1`, not
by which part of the frame was actually hit:

```
if (Rand() & 1) {
    if (goalTypeScored == GT_OWN_GOAL) PlayPostHitComment()
    else                               PlayBarHitComment()
} else {
    PlayPostHitComment()
}
PlayMissGoalSample()
```

So a ball off the crossbar has a 50 % chance of being called a post, and the only
thing that biases it is whether an own goal was in progress. This belongs in
[LEGACY.md](LEGACY.md) §14 ("things the original got wrong") — it is a bug, it is
audible, and it is not worth reproducing.

---

## 7. Destination reversal

[reverseDestXDirection](../reference/swos-port/src/game/ball/ball.cpp#L4509) and
[reverseDestYDirection](../reference/swos-port/src/game/ball/ball.cpp#L4551), each
about twenty instructions:

```
destX = x - (destX - x)   // = 2x - destX
```

Mirror the aim point about the current position on one axis. Called from the goal
frame paths and by deflection handling. The barrier code in §5 open-codes the same
arithmetic rather than calling these — the two implementations agree, but it is
worth knowing there are two.

---

## 8. The landing predictor

[calculateNextBallPosition()](../reference/swos-port/src/game/ball/ball.cpp#L4206).
This is **not** the integrator. It is a forward simulation that runs the ballistic
arc to ground and publishes where the ball will land, for the AI and the keeper to
chase.

```
if (speed == 0) → landing = current position

d1, d2, d3 = deltaX, deltaY, deltaZ
d4         = kGravityConstant
step       = select_band()

loop {
    x += d1;  y += d2
    d3 -= d4                     // gravity
    z  += d3
} until (z < 0)

ballNextX, ballNextY = whole(x), whole(y)
```

**The band selector is a speed optimisation with visible consequences**
([:4230-4276](../reference/swos-port/src/game/ball/ball.cpp#L4230-L4276)):

| Condition | Step | Applied as |
|---|---|---|
| `deltaZ > 0` (ball rising) | ×8 | `d1,d2,d4 <<= 3`, `z >>= 3` |
| `z.whole() > 35` | ×8 | `d1,d2,d4 <<= 3`, `z >>= 3` |
| `30 < z.whole() ≤ 35` | ×4 | `d1,d2,d4 <<= 2`, `z >>= 2` |
| `20 < z.whole() ≤ 30` | ×2 | `d1,d2,d4 <<= 1`, `z >>= 1` |
| `z.whole() ≤ 20` | ×1 | exact |

The higher the ball, the coarser the integration step, so the loop terminates in
few iterations regardless of how high the ball was hit. **A rising ball always uses
the coarsest step**, which means the predicted landing point is at its least
accurate exactly while the ball is climbing — precisely when defenders and the
keeper are committing to a position. Prediction sharpens as the ball falls through
the bands.

This is not a rounding artefact to be cleaned up. It is a **behavioural
characteristic**: chasers commit early on bad information and correct late. Any
reimplementation that computes an exact analytic landing point will produce
defenders who read high balls better than SWOS defenders do, and the game will feel
wrong in a way that is hard to trace back to this function.

---

## 9. Constants quick reference

**Global physics constants**, set by
[setAmigaModeEnabled()](../reference/swos-port/src/game/amigaMode.cpp#L21). These
are the concrete form of the Amiga-vs-DOS divergence
[LEGACY.md](LEGACY.md) §1 makes you choose between:

| Constant | Amiga | DOS | Meaning |
|---|---|---|---|
| `kBallGroundConstant` | 16 | 13 | Rolling friction, per tick |
| `kBallAirConstant` | 10 | 4 | Airborne friction, per tick |
| `kGravityConstant` | 4608 | 3291 | Gravity, 16.16 fixed point |
| `kKeeperSaveDistance` | 24 | 16 | Keeper reach |

Amiga is a **heavier, draggier ball** on every axis — more than double the air
friction and 40 % more gravity — on a 50 Hz tick against the DOS build's slower
rate (`kTargetFpsAmiga` vs `kTargetFpsPC`). The two builds do not merely run at
different speeds; the physics were retuned. This is why the community insists they
do not play the same ([LEGACY.md](LEGACY.md) §1).

**Per-pitch-type tables**, from
[initPitchBallFactors()](../reference/swos-port/src/game/game.cpp#L1388-L1401),
indexed by `PitchTypes` (0 Frozen, 1 Muddy, 2 Wet, 3 Soft, 4 Normal, 5 Dry, 6 Hard):

| Table | Frozen | Muddy | Wet | Soft | Normal | Dry | Hard |
|---|---|---|---|---|---|---|---|
| `pitchBallSpeedFactor` (DOS) | **−3** | **+4** | +1 | 0 | 0 | −1 | −1 |
| `pitchBallSpeedFactor` (Amiga) | −2 | +2 | +3 | 0 | 0 | −1 | −1 |
| `ballSpeedBounceFactor` | 24 | 80 | 80 | 72 | 64 | 40 | 32 |
| `ballBounceFactor` | 88 | 112 | 104 | 104 | 96 | 88 | 80 |

This is the complete answer to [LEGACY.md](LEGACY.md) §15's *"restitution and
rolling friction per pitch type (7 sets)"*. Read it as a surface model: Frozen is
fast underfoot (−3) and barely damps a bounce (24); Muddy is slow (+4) and kills
horizontal speed on contact (80). Note the Amiga and DOS speed-influence tables
**disagree on Wet** — +3 versus +1 — while agreeing everywhere else.

**Other literals:**

| Value | Meaning |
|---|---|
| `0xA000` (40960) | Minimum rebound `deltaZ`; below this the ball settles |
| `[53, 618] × [100, 799]` | Dead-ball invisible barrier, whole units |
| `speed >> 1` | Barrier reflection penalty |
| `speed >> 2` | Goal-frame contact penalty |
| `(speed >> 9) + 1` | Roll-animation advance rate |
| `((full + 16) & 0xFF) >> 5` | Fine heading → octant |
| 20 / 30 / 35 | Landing-predictor z band edges |

---

## 10. What this resolves, and what still needs measurement

**Confirmed as structure:**

- Ball is a `Sprite` with 16.16 position and independent scalar `speed` + heading. ✓
- Friction is a per-tick subtraction from `speed`; **air replaces ground**, and the
  pitch factor applies **only to a loose ball**. ✓
- Bounce = horizontal loss + negated-and-damped `deltaZ`, with a hard settle
  threshold at `0xA000`. ✓
- Two independent per-pitch restitution factors, horizontal and vertical. ✓
- All collisions restore the pre-integration position rather than resolving
  penetration. ✓
- Reflection is implemented as **destination mirroring**, not velocity negation. ✓
- Frame contact costs 25 % speed and (on the post path) kills aftertouch. ✓
- The landing predictor uses height-banded coarse stepping, worst while rising. ✓
- Gravity, friction and keeper reach differ between Amiga and DOS builds — with
  numbers. ✓

**Resolved by the Amiga oracle** (were open here; see §12):

- ~~`CalculateDeltaXAndY` is not read here.~~ Read in full at
  [amiga/MOVEMENT.md](amiga/MOVEMENT.md) §1: a halving reduction into a 32 × 32
  arctangent table, then a Q15 sine table shifted to Q7 and multiplied by `speed`.
  The 32-bit products *are* the 16.16 deltas, which is where the ~1/512 speed unit
  comes from.
- ~~Whether `kBallGroundConstant` etc. match the original binaries.~~ They do. 16,
  10 and 4608 are literals named `ballGroundConstant`, `ballAirConstant` and
  `gravityConstant` in the Amiga data segment (asm:30582, 30583, 30615). The port's
  Amiga column is a faithful transcription, not a fit.
- ~~What sets `fullDirection`.~~ `CalculateDeltaXAndY` returns it; resolution is 256
  steps but *effective* resolution degrades with distance to the aim point because
  of the halving reduction — see §12.
- ~~Ball-quadrant classification: what the bands are.~~ A 5 × 7 grid keyed on the
  **predicted landing point**, boundaries in [amiga/AI.md](amiga/AI.md) §1, consumed
  by off-ball positioning ([AI.md](AI.md) §3).
- ~~Tick rate.~~ 50 Hz on the Amiga, derived and corroborated in
  [amiga/TIMING.md](amiga/TIMING.md) §2 from the four match-length settings. Every
  per-second figure in §12 uses it. The DOS port's rate is still unmeasured.

**Open (measurement targets, [LEGACY.md](LEGACY.md) §15):**

- The goal-frame region ([:709-1690](../reference/swos-port/src/game/ball/ball.cpp#L709-L1690))
  is summarised, not fully traced — the net, top-of-goal and goal-out subdivisions
  need their own pass before set-piece work. The Amiga geometry in §12 is a good
  template for what that pass should recover.
- Deflection off players: where an intercepted ball's new direction comes from
  (`[LEGACY.md](LEGACY.md)` §15 "deflection rules on intercepted balls" is still open).
  Partially answered for the *tackle* case in [amiga/CONTEST.md](amiga/CONTEST.md) §3.
- `ballNextYGroundY` versus `ballNextY`: two different predictions, and
  [updatePlayers.cpp](../reference/swos-port/src/game/updatePlayers/updatePlayers.cpp#L1241)
  reads the former in at least five places. The distinction is unexplained. The
  Amiga has an analogous split in `UpdateBallVariables` (asm:38711), which publishes
  the ball position under three filters — `ballDefensive*`, `ballNotHigh*` and
  `strikeDest*` — and that is probably the same idea.
- Which pitch index is which named surface **in the Amiga binary**. The DOS port
  names them (§9); the Amiga index arrives from outside the match module. The tables
  are identical, so the naming almost certainly carries over, but it is inferred.
- Whether the DOS port also derives the goalmouth rebound scatter from the frame
  counter (§12). If it uses `Rand` instead, the two builds consume RNG differently
  and no trace can be compared across them.

---

## 11. Guidance for the reimplementation

- **Make the destination the primitive, not the velocity.** Every collision in
  SWOS rewrites `destX`/`destY` and lets deltas be re-derived next tick. Build the
  ball that way from the start; a velocity-based ball with a destination bolted on
  will diverge in exactly the places that matter.
- **Reject moves, do not resolve them.** Save position at the top of the tick,
  restore it wholesale on any collision. This is cheap, deterministic, and it is
  what the reference does.
- **Keep `speed` scalar and separate from heading.** Friction, bounce, barrier and
  frame penalties are all scalar operations on it. A vector velocity makes all four
  more complicated and none of them clearer.
- **Air friction replaces ground friction. Do not add them.** And do not let pitch
  type affect a ball in flight or a ball under a player's control.
- **Implement the settle threshold.** Without the `0xA000` cutoff the ball
  micro-bounces forever and the animation state flickers.
- **Reproduce the landing predictor's coarse bands, including the rising-ball
  case.** It is tempting to solve the arc analytically. Don't — defender commitment
  behaviour depends on the prediction being wrong in a specific, height-dependent
  way.
- **Put the Amiga/DOS constants behind one switch** and pick Amiga as the default
  ([LEGACY.md](LEGACY.md) §1). The four values in §9 plus the tick rate are the
  whole of the divergence as far as the ball is concerned.
- **Do not reproduce the commentary coin-flip** (§6). It is a bug, and unlike most
  SWOS quirks it carries no gameplay meaning.
- **Fit every constant against traces before trusting it** ([LEGACY.md](LEGACY.md) §17).
  The values in §9 are the best starting point this project has, which is exactly
  why they should be verified rather than assumed. The Amiga confirmation in §12
  raises confidence in the *Amiga* column specifically; the DOS column is still only
  the port's own word.

---

## 12. Amiga cross-check

Traced independently through the Amiga original — [amiga/BALL.md](amiga/BALL.md).

### Confirmed, exactly

Every physics constant in the §9 **Amiga** column appears in the Amiga binary as a
named literal, at the lines given:

| §9 constant | Amiga symbol | Line | Value |
|---|---|---|---|
| `kBallGroundConstant` | `ballGroundConstant` | asm:30582 | 16 |
| `kBallAirConstant` | `ballAirConstant` | asm:30583 | 10 |
| `kGravityConstant` | `gravityConstant` | asm:30615 | 4608 |
| `kKeeperSaveDistance` | `keeperSaveDistance` | asm:34835 | 24 |
| `pitchBallSpeedFactor` (Amiga row) | `pitchBallSpeedInfluence` | asm:30608 | −2, +2, +3, 0, 0, −1, −1 |
| `ballSpeedBounceFactor` | `ballSpeedBounceFactorTable` | asm:30594 | 24, 80, 80, 72, 64, 40, 32 |
| `ballBounceFactor` | `ballBounceFactorTable` | asm:30601 | 88, 112, 104, 104, 96, 88, 80 |
| `0xA000` settle threshold | — | asm:21754 | $A000 |
| Barrier `[53, 618] × [100, 799]` | — | asm:21786, asm:21800 | same |
| `speed >> 1` on barrier breach | — | asm:21786 | same |

All seven entries of all three pitch tables match element-for-element, in the same
order. That is the strongest single agreement between the two oracles: the port's
`setAmigaModeEnabled()` really does restore the original's numbers, and the DOS
column in §9 is the deliberate retune it claims to be.

The structural claims agree too: friction as a subtraction (not a multiplication),
air friction *replacing* ground friction, the pitch factor applying only to a loose
ball, destination-mirroring instead of velocity reflection, position restored rather
than penetration resolved, and a forward-simulated rather than analytically-solved
landing predictor.

### Now with physical units

The Amiga's 50 Hz tick ([amiga/TIMING.md](amiga/TIMING.md) §2) and the ~1/512
px/frame speed unit ([amiga/STATE.md](amiga/STATE.md) §1) turn §9's raw integers
into figures we can sanity-check a trace against:

| Quantity | Raw | Real |
|---|---|---|
| Gravity | 4608 | 0.0703 px/frame², **176 px/s²** |
| Ground friction | 16 | 0.031 px/frame², 1.56 px/s² |
| Kick launch speed | 2208 | 4.31 px/frame, **216 px/s** |
| Kick launch rise | $14000 | 1.25 px/frame, 62.5 px/s |
| Bounce cut-off | $A000 | 0.625 px/frame |

A kicked ball rolling on a normal pitch stops after **138 frames — 2.76 seconds**.
A ball launched at the standard rise reaches apex in ~18 frames and hangs for ~36.
Both are cheap first tests for our integrator.

### Three disagreements to resolve

1. **The crossbar.** §6 here says bar and post converge on `speed -= speed >> 2`
   with `deltaZ = -deltaZ` on the bar path. The Amiga does something different and
   more specific ([amiga/BALL.md](amiga/BALL.md) §6): three *distinct* treatments —
   post `speed >> 2` with `destX` mirrored, net `speed >> 3` with `destY` mirrored,
   and the crossbar not reflecting at all but having `speed` **set** to a flat 512
   with `destY` pushed 1000 px back out of the goal and `deltaZ = 1`. Below z = 15
   the bar simply stops the ball dead. A set-versus-scaled speed is a behavioural
   difference, not a rounding one, and it is the flat deadened bounce-out players
   remember. **Re-read the DOS port's [:1442-1542](../reference/swos-port/src/game/ball/ball.cpp#L1442-L1542)
   against this before implementing either.**

2. **Goalmouth geometry, by one pixel.** §6 gives `kGoalLeft`/`kGoalRight` as
   303/367; the Amiga goal test reads 302 … 366 for the mouth with the posts at
   296 … 372 (asm:21830). The one-unit offset is most likely an inclusive/exclusive
   convention difference in the two transcriptions rather than a real divergence,
   but the *posts* band has no counterpart in this document at all and needs adding
   once the goal-frame pass in §10 is done. The Amiga also gives the crossbar as a
   band, Z 15 … 19, where this document has no crossbar height at all.

3. **When the barrier is live.** §5 states the dead-ball barrier is active only
   when `gameStatePl != ST_GAME_IN_PROGRESS`. The Amiga reading describes the same
   rectangle but does not record that gate — it presents the barrier as a
   containment field for a ball that has already gone out, which is consistent in
   spirit but not evidence for the gate. Treat the DOS reading as authoritative
   here (it is the more specific claim) and flag it for confirmation.

### New from the Amiga, not covered above

- **The goalmouth scatter.** During live play a second, tighter goal-area block
  (asm:21840–21900) adds a lateral jitter to the rebound:
  `d4 = ((stoppageTimer & 31) << 4) − 256`, i.e. −256 … +240 in steps of 16, added
  to `destX` or `destY` by struck face. It is derived from the **frame counter, not
  from `Rand`** — deterministic, and therefore safe to put inside `at_core` without
  perturbing the RNG sequence. This is the mechanism behind unpredictable goalmouth
  scrambles, and nothing in this document mentions it.
- **The post-hit whistle suppression.** Before the goal-line branch, a ball with
  speed ≥ $300 inside X 290 … 381 and Z < 25 triggers woodwork audio *and clears the
  referee-whistle flag* ([amiga/SETPIECES.md](amiga/SETPIECES.md) §1), so a shot
  that rebounds off the frame and out does not get whistled as a normal out-of-play.
- **`deltaZ |= 1` is on the Amiga too** (asm:21728), and the Amiga reading gives the
  reason plainly: it guards against `deltaZ` reaching exactly zero and the ball
  hanging. §4's instinct to reproduce it rather than tidy it is correct.
- **Bounces do not deflect.** The horizontal loss is applied to the scalar `speed`
  only, so a bounce slows the ball along its existing heading and never changes it.
  Worth stating explicitly; it is easy to add a deflection that was never there.
- **`sub_107260` (asm:21759)** is called on a high-energy bounce and is *probably*
  the bounce sound trigger — the same gating this document notes at §4, where a
  settling ball bounces silently.
