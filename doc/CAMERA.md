# CAMERA.md

How the view follows the game: the five camera modes and their priority, the
lead-ahead that makes the camera anticipate rather than chase, the two-stage
clipping that keeps it on the pitch, and the special case that lets it slide off to
the bench. Traced through the reference DOS port in
[../reference/swos-port/](../reference/swos-port/).

Where the camera is *used* — subtracting it from sprite positions to get screen
coordinates — is [PLAYER_SPRITES.md](PLAYER_SPRITES.md) §10. The `cameraDirection`
written by each restart is [SETPIECES.md](SETPIECES.md) §2. This document is how
the camera position itself is produced, every tick.

> **Provenance.** [camera.cpp](../reference/swos-port/src/game/camera.cpp) is one of
> the few subsystems the porters rewrote as ordinary modern C++ rather than leaving
> as register-level decompilation. Every constant is a named `constexpr` with a real
> value, and the control flow is directly readable. This is therefore the
> **highest-confidence document in the set** — but for exactly that reason, remember
> the numbers are the *porters'* transcription of the original, and the port also
> introduces its own concepts (zoom, resolution independence) that the original
> lacked. Read to understand the design; write our own code.

---

## 0. One-paragraph version

[moveCamera()](../reference/swos-port/src/game/camera.cpp#L97) picks one of five
modes by priority — booking, penalty shootout, substitution, leaving-bench, bench,
otherwise standard — and each returns a `CameraParams` of *destination*, *x-limit*
and *lead velocity*. Standard mode then sub-switches on `gameState` into
"watch the players walk off", "show the result", or the normal
**follow-the-ball**. The destination is the target minus half the screen, **plus an
accumulated lead offset** that builds at ±2 per tick toward ±40 in whatever
direction the ball is travelling — so the camera sits ahead of play rather than on
top of it. The camera then eases toward that destination by **one sixteenth of the
remaining distance per tick**, capped at 5 units of movement, and is finally
clamped to the pitch. Clipping happens **twice against different bounds**: the
destination is clipped by a mode-dependent side margin, and the resulting position
is clipped again by the hard pitch limits.

---

## 1. State

| Variable | Meaning |
|---|---|
| `m_cameraX`, `m_cameraY` | `FixedPoint` camera position — the only real state |
| `swos.cameraXVelocity`, `cameraYVelocity` | Accumulated **lead offset** (§4) — despite the name, not a speed |
| `m_leavingBenchMode` | Latch, set externally, cleared when the bench is off screen |
| `swos.cameraDirection` | Octant written by each restart ([SETPIECES.md](SETPIECES.md) §2) |

`CameraParams` — what every mode returns
([camera.cpp:46-56](../reference/swos-port/src/game/camera.cpp#L46-L56)):

```
xDest, yDest      // where to look, in pitch coordinates
xLimit            // side margin: how close to the touchline the view may get
xVelocity, yVelocity   // lead offset to add
```

---

## 2. Mode priority

[moveCamera()](../reference/swos-port/src/game/camera.cpp#L97). First match wins:

| # | Condition | Mode | Destination |
|---|---|---|---|
| — | `showFansCounter` | **frozen** — returns immediately, no update at all | |
| 1 | `cardHandingInProgress()` | `bookingPlayerMode` | the booked player |
| 2 | `playingPenalties` | `penaltyShootoutMode` | fixed `(336, 107)` |
| 3 | `g_waitForPlayerToGoInTimer` | `benchMode(substituting)` | bench, limit 51 |
| 4 | `m_leavingBenchMode` | `leavingBenchMode` | fixed x 211 |
| 5 | `inBench()` | `benchMode(false)` | bench, limit from §6 |
| 6 | — | `standardMode` | §3 |

Two things worth noting. **`showFansCounter` freezes the camera entirely** — not a
mode, an early return, so the lead offsets also stop accumulating. And the
**penalty shootout camera is a fixed point**, `(336, 107)`: it does not follow the
ball, the taker or the keeper. It stares at the upper goal for the whole shootout.

---

## 3. Standard mode

[standardMode()](../reference/swos-port/src/game/camera.cpp#L160) resolves the
lead direction, the side limit, and then sub-switches on `gameState`:

```
if (gameStatePl == kInProgress)  direction = ball.deltaX, ball.deltaY
else                             direction = getGameStoppedCameraDirections()   // §5

limit = 63                                          // kPitchSideCameraLimitDuringGame
if (stopped && (corner || throw-in))  limit = 37    // kPitchSideCameraLimitDuringBreak
```

| `gameState` | Destination |
|---|---|
| `kStartingGame`, `kCameraGoingToShowers`, `kGoingToHalftime`, `kPlayersGoingToShower` | `(590, 449)` — off the pitch edge, watching players leave |
| `kResultAfterTheGame` | `(336, 129)` — centre x, top goal line |
| `kGameEnded` (with `penaltiesState < 0`), and default | `(336, 449)` — centre spot |
| `kFirstHalfEnded` | falls through to follow-the-ball |
| otherwise | **`followTheBall`** — `(ball.x, ball.y)` plus lead |

The **corner and throw-in exception is the interesting one**: the side limit drops
from 63 to 37, letting the camera get closer to the touchline so the set piece is
actually visible. Everywhere else the game keeps a 63-unit margin, which is why
SWOS never shows you the very edge of the pitch during open play.

---

## 4. The lead offset

[getStandardModeCameraVelocity()](../reference/swos-port/src/game/camera.cpp#L351):

```
if (xDirection < 0 && xVelocity != -40)  xVelocity -= 2
else if (xDirection > 0 && xVelocity != 40)  xVelocity += 2
// same for y
```

And in [updateCameraCoordinates()](../reference/swos-port/src/game/camera.cpp#L219):

```
xDest = params.xDest - kVgaWidth/2  + params.xVelocity
yDest = params.yDest - kVgaHeight/2 + params.yVelocity
```

**`cameraXVelocity` is added to the destination, not to the position.** It is a
displacement, not a speed — the name is misleading. It ramps by 2 per tick to a
maximum of 40, in whichever direction the ball is moving, and it decays only by
ramping the other way when the ball reverses.

This is the whole reason SWOS's camera feels right. A camera locked to the ball
puts the ball in the centre and shows you equal amounts of where you came from and
where you are going. This one **drifts up to 40 units ahead of the ball and stays
there**, so a player running at goal sees the goal. The ramp is slow enough (20
ticks to full lead) that direction changes don't snap.

Note the equality tests (`!= -40`, `!= 40`) rather than clamps. Since the increment
is exactly 2 and the limit is exactly 40, the value always lands on 40 precisely —
but it is a fragile idiom that would break if either constant changed to make them
non-divisible.

The lead is only produced by `followTheBall`; every other mode returns zero
velocities, which **resets the accumulator** because `updateCameraCoordinates`
writes `params.xVelocity` straight back into `swos.cameraXVelocity`.

---

## 5. Lead direction while play is stopped

[getGameStoppedCameraDirections()](../reference/swos-port/src/game/camera.cpp#L326).
A stopped ball has no `deltaX`, so the direction comes from a player instead:

```
if (lastTeamPlayedBeforeBreak && its controlledPlayer)
     direction = controlledPlayer.direction
else direction = swos.cameraDirection          // written by the restart

if (valid) (xDir, yDir) = kNextCameraDirections[direction]
```

```
kNextCameraDirections[16] = { 0,-1,  1,-1,  1,0,  1,1,  0,1,  -1,1,  -1,0,  -1,-1 }
```

An octant → unit-vector table, confirming once more the **0 = N, clockwise**
convention shared by [SETPIECES.md](SETPIECES.md) §2,
[PLAYER_SPRITES.md](PLAYER_SPRITES.md) §5 and [BALL.md](BALL.md) §2.

So during a stoppage the camera leads in the direction **the taker is facing**,
falling back to the restart's `cameraDirection` when there is no controlled player.
This is why the view swings toward goal as you turn a free-kick taker.

---

## 6. Bench mode and the slide

[getBenchCameraXLimit()](../reference/swos-port/src/game/camera.cpp#L303). Normally
the side limit is 37 during a break. But:

```
cameraAtBenchLevel = cameraY ∈ [339, 359]

if (cameraAtBenchLevel && both goal sprites absent-or-offscreen)
    limit = g_substituteInProgress ? 51 : 0
```

Dropping the limit to **0** removes the side margin entirely and lets the camera
travel all the way to `x = 0`, off the side of the pitch, to show the dugout. The
source comment says it directly: *"once the camera reaches this area it's allowed
to slide all the way left"*.

The gate is a narrow 20-unit y-band around the halfway line plus a test that
neither goal is visible. **The porters flagged this as not fully understood** —
their comment reads *"These sprite conditions are weird, I don't fully understand
them so leaving them in verbatim. Image index should always be set, so it's
something 'if both goals are not visible'. But at this y range goals shouldn't be
visible anyway — is it a tautology?"* They kept it verbatim and added an `assert`
asserting the tautology. Treat it as unresolved.

`m_leavingBenchMode` is a latch cleared by
[updateCameraLeaving()](../reference/swos-port/src/game/camera.cpp#L209) once
`cameraX < 35`, i.e. once the bench is no longer on screen.

---

## 7. Movement and the two clippings

[updateCameraCoordinates()](../reference/swos-port/src/game/camera.cpp#L219), in
order:

```
1.  store lead velocities back into swos.cameraXVelocity / cameraYVelocity
2.  xDest = target - screen/2 + lead
3.  clipCameraDestination(xDest, yDest, xLimit)      // FIRST clip
4.  delta = (dest - camera) / 16
5.  clipCameraMovement(delta)                        // |delta| <= 5
6.  camera += delta
7.  constrainCameraToPitch(camera)                   // SECOND clip
```

**Step 4 is an exponential ease** — one sixteenth of the remaining distance per
tick, so the camera decelerates as it arrives and never snaps. **Step 5 caps it at
5 units per tick**, which only binds when the destination jumps a long way (a
restart at the far end, a mode change). Together they give a fast-but-smooth
response with a hard speed ceiling.

The two clips use **different bounds**, which is easy to miss:

| | x | y |
|---|---|---|
| `clipCameraDestination` (mode-dependent) | `[xLimit, 352 − xLimit]` | `[16, 664]`, or `[80, 616]` training |
| `constrainCameraToPitch` (absolute) | `[0, 352]` | `[16, 680]` |

The destination clip is symmetric about the pitch and narrows with `xLimit`; the
position clip is the hard boundary and is **16 units taller at the bottom**
(680 vs 664). So the camera can end up in a position its destination was never
allowed to be — briefly, while easing.

---

## 8. Initial position

[setCameraToInitialPosition()](../reference/swos-port/src/game/camera.cpp#L121):

```
if (g_trainingGame)  (168, 313)
else                 (176, rand() & 1 ? 664 : 16)
```

**The kick-off camera starts at a randomly chosen end of the pitch** and eases to
the centre as play begins. One RNG draw, purely cosmetic, but it consumes from the
match RNG stream ([AI.md](AI.md) §6) — so it must be reproduced in the same order
or every subsequent random draw in the match desynchronises.

---

## 9. Constants quick reference

| Constant | Value | Meaning |
|---|---|---|
| `kCenterX` | 176 | Kick-off camera x |
| `kTopStartLocationY` / `kBottomStartLocationY` | 16 / 664 | Randomly chosen start end |
| `kTrainingGameStartX/Y` | 168, 313 | Training start |
| `kPenaltyShootoutCameraX/Y` | 336, 107 | Fixed shootout view |
| `kLeavingBenchCameraDestX` | 211 | |
| `kBenchSlideAreaStartY` / `EndY` | 339 / 359 | Band where the side limit may drop to 0 |
| `kPlayersOutsidePitchX` | 590 | "Watch them walk off" x |
| `kTopGoalLine` | 129 | Result-at-top y |
| `kPitchMaxX` | 352 | |
| `kPitchMinY` / `kPitchMaxY` | 16 / 664 | Destination y bounds |
| `kTrainingPitchMinY` / `MaxY` | 80 / 616 | |
| `kCameraMinX` / `MaxX` | 0 / 352 | Hard position bounds |
| `kCameraMinY` / `MaxY` | 16 / 680 | Hard position bounds — note 680, not 664 |
| `kPitchSideCameraLimitDuringGame` | 63 | Normal side margin |
| `kPitchSideCameraLimitDuringBreak` | 37 | Corners, throw-ins, booking, leaving bench |
| `kSubstituteCameraLimit` | 51 | |
| `kCameraLeavingBenchXLimit` | 35 | Bench considered off screen |
| Ease factor | `/16` | Fraction of remaining distance per tick |
| `kMaxCameraMovement` | 5 | Per-tick movement cap |
| `kVelocityIncrement` | 2 | Lead ramp per tick |
| `kMaxVelocity` | 40 | Maximum lead offset |

---

## 10. What this resolves, and what still needs measurement

**Confirmed as structure:**

- Five modes plus a freeze, resolved by strict priority in one function. ✓
- `cameraXVelocity` is an accumulated **lead displacement**, not a speed — added to
  the destination, ramping ±2/tick to ±40. ✓
- Lead direction is the ball's delta in play, and the taker's facing (or the
  restart's `cameraDirection`) when stopped. ✓
- Movement is a `/16` exponential ease capped at 5 units per tick. ✓
- Two clipping stages against **different** bounds. ✓
- Side margin narrows from 63 to 37 for corners and throw-ins specifically. ✓
- Penalty shootout uses a fixed camera position. ✓
- Kick-off camera starts at a random end — one RNG draw. ✓
- Bench slide unlocks the left margin inside a 20-unit y-band. ✓

**Open (measurement targets, [LEGACY.md](LEGACY.md) §15):**

- **`kVgaWidth` / `kVgaHeight`** are not resolved here; the centring offset in §7
  depends on them, and the port has a resolution-independent layer the original did
  not ([PITCH.md](PITCH.md)).
- **Zoom interaction.** The port adds a zoom system
  ([PITCH.md](PITCH.md) §5) that the original lacked. Whether camera limits are
  applied before or after zoom scaling — and what that does to the effective side
  margin — is not covered by this file.
- The goal-sprite condition in §6 is unexplained, and the porters said so.
- Whether the original ramped the lead offset at the same rate, or whether 2/40 are
  the porters' fitted values. This is the highest-value thing to confirm against a
  trace: the lead offset is what the camera *feels* like.
- `breakCameraMode` is written by every restart ([SETPIECES.md](SETPIECES.md) §2)
  but is not read anywhere in this file. Who consumes it?
- What sets `showFansCounter`, and for how long the camera stays frozen.
- The `kFirstHalfEnded` case falls through to follow-the-ball with no comment —
  intentional or a missing `return`?

---

## 11. Guidance for the reimplementation

- **Build the lead offset before anything else.** Destination = target − half-screen
  + accumulated lead, ramping toward the direction of travel. It is twenty lines and
  it is the difference between a camera that feels like SWOS and one that does not.
  Everything else in this document is refinement.
- **Keep the ease-then-cap ordering.** `/16` toward the destination, then clamp the
  delta. Reversing them (clamping the destination distance, then easing) produces a
  visibly different, laggier response.
- **Model modes as functions returning a params struct**, exactly as the port does.
  It keeps the priority list readable in one place and makes adding a mode a
  two-line change. This is one place where copying the port's *structure* — not the
  original's — is the right call.
- **Keep both clipping stages separate.** They are not redundant: one shapes where
  the camera wants to be, the other is a hard boundary. Merging them changes
  behaviour at the pitch edges.
- **Reproduce the random start end**, and draw it from the match RNG stream in the
  same order, or replays will diverge ([SIMULATION.md](SIMULATION.md) §9).
- **Do not put the camera inside the deterministic tick's state.** It reads
  simulation state and affects nothing; keeping it outside `at_core` means camera
  changes can never break replay determinism. The one exception is the RNG draw in
  §8 — do that in core, pass the result out.
- **Treat the side margins as tuning knobs, not constants.** 63/37/51 are pitch- and
  resolution-specific; parameterise them from the start rather than discovering they
  need to be later.
