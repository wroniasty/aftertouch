# BALL.md

Ball physics in the Amiga original: what one frame does to it, what slows it, what
makes it bounce, what the seven pitch types change, and what happens when it meets
the dead-ball barrier or the goal frame.

This covers the ball **between** events. The kick that launches it is
[KICKING.md](KICKING.md); the curl applied for ten frames afterwards is
[AFTERTOUCH.md](AFTERTOUCH.md); the dribble touch that re-aims it is
[CONTEST.md](CONTEST.md) §2; the classification that takes it out of play is
[SETPIECES.md](SETPIECES.md) §1.

> **Provenance.** `UpdateBall` (asm:21593) is one contiguous routine and is read
> directly. Unlike the DOS-port trace, **every physics constant here is a literal
> value in the source with a descriptive label** — `gravityConstant`,
> `ballGroundConstant`, `ballBounceFactorTable` and the rest sit in the block at
> asm:30582–30621. The values are certain; the *interpretation* of each into
> physical units passes through the fixed-point and frame-rate assumptions set out
> in [STATE.md](STATE.md) §1, and that chain is what to attack if a trace disagrees.

---

## 0. One-paragraph version

The ball is a `Sprite` with 16.16 `x`, `y`, `z`, matching 16.16 deltas, a scalar
`speed` and an aim point `(destX, destY)`. Each frame `UpdateBall` re-derives the
horizontal velocity from `speed` and the heading toward the aim point, **subtracts
a friction constant from `speed`** — 16 on the ground, 10 in the air, with a
pitch-type modifier applied only when nobody is in possession — then integrates all
three axes. Vertical motion is independent: `deltaZ` loses `gravityConstant` every
frame, and when `z` would go negative the ball **bounces**, losing a pitch-dependent
fraction of horizontal speed and a pitch-dependent fraction of vertical speed; below
a fixed rebound threshold it stops bouncing entirely. Collisions are not expressed
as velocity reflection but as **mirroring the aim point about the current
position** — the same destination-driven idiom the whole engine uses — with a speed
divisor that differs by surface: half at the invisible pitch barrier, a quarter off
a post, an eighth off the back of the net. A separate pass runs the ballistic arc
forward to its landing point and publishes it for the AI and the keeper to chase.

---

## 1. The frame, in order

`UpdateBall` runs once per frame, early in the loop — before the players
(`UpdatePlayersAndBall`), so every player reacts to a ball position already
advanced this frame. See [TIMING.md](TIMING.md) §1 for the full ordering.

1. **Animation.** Pick the rolling or static frame table by whether either
   horizontal delta is non-zero; advance the cursor at a rate derived from speed
   (asm:21628: `step = (speed >> 9) + 1`). Faster ball, faster spin.
2. **Heading.** Call `CalculateDeltaXAndY` with the aim point and the current
   speed. It returns `deltaX`, `deltaY` and a 0–255 heading (asm:21665). Store the
   heading; derive the octant.
3. **Friction.** Subtract a constant from `speed`, clamped at zero (§2).
4. **Integrate horizontally.** `x += deltaX`, `y += deltaY` as longword adds.
5. **Integrate vertically.** Apply gravity to `deltaZ`, add to `z`, and handle the
   bounce (§3).
6. **Barrier.** Test the dead-ball rectangle and mirror if breached (§4).
7. **Goal frame.** Test posts, bar and net (§5).
8. **Out of play.** During live play, an X below 81 hands off to `GameSetup`
   (asm:22056).
9. **Shadow and quadrant.** Position the shadow sprite; recompute the ball's
   tactical quadrant for the AI (asm:22125, see [AI.md](AI.md) §2).

Step 2 is the load-bearing one: **the ball chases an aim point every single frame**.
It never travels in a stored direction. Change `destX`/`destY` and the ball turns
immediately, which is how curl, rebounds and dribbling are all implemented with one
mechanism.

---

## 2. Friction

Per frame, before integration (asm:21684–21700):

```
c = ballGroundConstant                       ; 16
if neither team has possession:  c += pitchBallSpeedFactor
if z != 0:                       c = ballAirConstant      ; 10
speed = max(0, speed - c)
```

Three things worth noticing.

**Air friction is lower than ground friction**, 10 against 16, so a lofted ball
carries further than a rolled one at the same launch speed. That is the whole
reason chipping is useful.

**The height test is on the integer part of `z`.** Friction flips to the air value
the instant the ball leaves the turf ([STATE.md](STATE.md) §5).

**The pitch modifier only applies to a free ball.** While a player is in possession
the surface makes no difference — the dribble touch re-imposes speed anyway (see
[CONTEST.md](CONTEST.md) §2), so the original simply skips it.

With `ballKickingSpeed` 2208 and ground friction 16, a kicked ball rolling on a
normal pitch stops after 138 frames — **2.76 seconds** at 50 Hz.

---

## 3. Gravity and the bounce

Gravity is a single longword subtraction (asm:21728):

```
deltaZ -= gravityConstant                    ; 4608 in 16.16 = 0.0703 px/frame²
deltaZ |= 1                                  ; force non-zero
z += deltaZ
```

The `|= 1` on the fraction is a guard against `deltaZ` reaching exactly zero and
the ball hanging; it costs nothing physically.

At 50 Hz, 0.0703 px/frame² is **176 px/s²**. Against a pitch 641 px long that is a
deliberately floaty gravity — a ball launched at the standard `ballKickingDeltaZ`
of $14000 (1.25 px/frame, 62.5 px/s) reaches apex in about 18 frames and hangs for
36, which is the characteristic SWOS chip.

When `z` goes negative, the bounce runs (asm:21733–21755):

```
speed -= (speed * ballSpeedBounceFactor) >> 8      ; horizontal loss
z      = 0
deltaZ = -deltaZ
deltaZ -= (deltaZ >> 8) * ballBounceFactor         ; vertical loss
deltaZ |= 1
if deltaZ <= $A000:  deltaZ = 0                    ; stop bouncing
```

Both factors are `/256` fractions selected per pitch type (§5). The cut-off,
$A000 = **0.625 px/frame**, is a fixed constant and not pitch-dependent: below that
rebound velocity the ball is declared settled and rolls. This is what stops the
infinite geometric bounce sequence a naive model produces.

The horizontal loss is applied to the scalar `speed`, so a bounce slows the ball
along its heading without changing its direction. Bounces do not deflect.

---

## 4. The dead-ball barrier

An invisible rectangle well outside the touchlines keeps the ball on screen
(asm:21785–21805):

| Axis | Range | On breach |
|---|---|---|
| X | 53 … 618 | mirror `destX` about `x`, `speed >>= 1`, restore position |
| Y | 100 … 799 | mirror `destY` about `y`, `speed >>= 1`, restore position |

"Mirror the aim point" is `dest = pos - (dest - pos)` — helpers `sub_109A54` and
`sub_109A66` (asm:22228, asm:22240). Because the heading is re-derived from the aim
point next frame, this produces a clean specular reflection without ever touching
the velocity.

"Restore position" means writing back the pre-integration `x`, `y`, `z` saved at
the top of the frame, so the ball never visibly penetrates the barrier.

Note this barrier is *outside* the pitch: X 53 and 618 sit well beyond the playable
81 … 591. The barrier exists to contain a ball that has already gone out and is
waiting for the restart to be set up — it is not what puts the ball out. That is
`GameSetup`, driven separately from the Y comparisons in the goal-frame block and
from the X < 81 test at asm:22056.

---

## 5. Pitch types

`InitGame` (asm:23742–23748) reads a pitch-type index — 0 to 6 — and pulls one
entry from each of three parallel tables:

| Index | `pitchBallSpeedInfluence` | `ballSpeedBounceFactorTable` | `ballBounceFactorTable` |
|---|---|---|---|
| 0 | −2 | 24 | 88 |
| 1 | +2 | 80 | 112 |
| 2 | +3 | 80 | 104 |
| 3 | 0 | 72 | 104 |
| 4 | 0 | 64 | 96 |
| 5 | −1 | 40 | 88 |
| 6 | −1 | 32 | 80 |

Reading down the columns tells you what the seven surfaces are without needing the
artwork. Index 0 loses the *least* horizontal speed on a bounce (24/256 ≈ 9 %) and
has *negative* rolling friction modifier — a fast, true surface. Index 1 and 2 lose
80/256 ≈ 31 % on a bounce and add 2–3 to rolling friction — heavy, slow, dead
surfaces. Indices 5 and 6 are fast again but with the lowest bounce retention of
all.

Mapped onto SWOS's seven pitch conditions in menu order (normal, soft, muddy,
frozen, wet, dry, hard), index 0 reads as the reference surface and 1–2 as the
mud. The mapping is not proven from this binary — the index comes in from a global
set outside the match module — and is flagged `[UNKNOWN]` accordingly.

The three constants are latched into `pitchBallSpeedFactor`, `ballSpeedBounceFactor`
and `ballBounceFactor` at kick-off and never change during a match.

---

## 6. The goal frame

The most intricate geometry in the routine (asm:21796–21880). It only runs when the
ball is beyond a goal line — Y below 129 or above 769 — and is skipped entirely for
the whole playable interior.

Geometry, in pixels:

| Feature | Extent |
|---|---|
| Frame outer (posts included) | X 296 … 372 |
| Goal mouth (between posts) | X 302 … 366 |
| Crossbar underside | Z 15 |
| Crossbar top | Z 19 |
| Bottom goal line | Y 770 |
| Bottom net back | Y 785 |
| Top goal line | Y 128 |
| Top net back | Y 112 |

Resolution, in the order the code tests:

| Condition | Outcome |
|---|---|
| Z > 19 | Over the bar — no interaction |
| X outside 296 … 372 | Wide — no interaction |
| Z > 15, inside the frame | **Crossbar** |
| X outside 302 … 366 | **Post** |
| Past the net back | **Net** |
| Otherwise | Goal — ball continues into the net |

And the three rebound treatments:

| Hit | Speed after | Aim point | Extra |
|---|---|---|---|
| **Post** | `speed >> 2` | mirror `destX` | spin timers reset |
| **Net** | `speed >> 3` | mirror `destY` | spin timers reset |
| **Crossbar**, `z` > 15 | 512 (fixed) | `destY` pushed 1000 away from goal | `deltaZ = 1` |
| **Crossbar**, `z` ≤ 15 | 0 | position restored | `deltaZ = 1` |

The crossbar is the interesting one: it does not reflect. A ball hitting the bar
above 15 has its speed *set* to 512 — roughly a quarter of a full kick — and is
aimed 1000 pixels back out of the goal, which is the flat, deadened bounce-out
everyone remembers. Below that height it is simply stopped dead on top of the bar
and dropped.

Resetting the spin timers on post and net contacts means **aftertouch does not
survive a rebound**. See [AFTERTOUCH.md](AFTERTOUCH.md) §3.

### The goalmouth scatter

While play is live there is a second, tighter goal-area block (asm:21840–21900)
that adds a deterministic lateral jitter to the rebound:

```
d4 = ((stoppageTimer & 31) << 4) - 256          ; range -256 … +240, step 16
```

added to `destX` or `destY` depending on which face was struck. The value is
derived from the frame counter, not from `Rand` — it looks random but is fully
determined by when the contact happened. This is the scramble that makes goalmouth
rebounds unpredictable without introducing an RNG dependency into the physics.

---

## 7. The landing predictor

`CalculateNextBallPosition` (asm:35665) and `CalculateBallNextXYPositions`
(asm:39041) run the arc forward to publish `ballNextX` / `ballNextY` — where the
ball will come down. Everything that chases the ball chases *this*, not the current
position: the zonal grid quadrant is computed from it (asm:22126), and the keeper
compares his time-to-reach against it ([GOALKEEPER.md](GOALKEEPER.md) §3).

`CalculateBallNextXYPositions` steps the ballistic solution by repeatedly
subtracting `z` while accumulating (asm:39065), i.e. it simulates the descent rather
than solving it — so the prediction has exactly the same rounding as the real
integration and cannot drift from it. Reimplementations that solve the quadratic
analytically will disagree with the original by a pixel or two at long range, which
matters because it changes which player is nearest.

---

## 8. Constants quick reference

All values read directly from the listing. See [SOURCE-MAP.md](SOURCE-MAP.md) §2 for
the full blocks.

| Symbol | Line | Value | Meaning |
|---|---|---|---|
| `gravityConstant` | 30615 | 4608 (16.16) | 0.0703 px/frame², 176 px/s² |
| `ballGroundConstant` | 30582 | 16 | Rolling friction per frame |
| `ballAirConstant` | 30583 | 10 | Air friction per frame |
| `pitchBallSpeedInfluence` | 30608 | −2 … +3 | Pitch modifier on rolling friction |
| `ballSpeedBounceFactorTable` | 30594 | 24 … 80 | Horizontal loss on bounce, /256 |
| `ballBounceFactorTable` | 30601 | 80 … 112 | Vertical loss on bounce, /256 |
| Bounce cut-off | 21754 | $A000 | 0.625 px/frame; below this, stop bouncing |
| Barrier X | 21786 | 53 … 618 | Speed halved on breach |
| Barrier Y | 21800 | 100 … 799 | Speed halved on breach |
| Post rebound | 21873 | `speed >> 2` | |
| Net rebound | 21868 | `speed >> 3` | |
| Crossbar rebound speed | 21751 | 512 | Fixed, not proportional |
| Goalmouth scatter | 21841 | ±256 from `stoppageTimer` | Deterministic, not RNG |

---

## 9. What this resolves, and what still needs measurement

Confirmed as structure and value:

- ✓ Friction is a subtraction from a scalar, not a multiplication — so it is linear,
  and a slow ball stops abruptly rather than asymptotically.
- ✓ Air friction < ground friction.
- ✓ Gravity is a single constant with no drag term.
- ✓ Bounce loses horizontal and vertical energy through two independent
  pitch-dependent factors.
- ✓ A fixed rebound cut-off terminates the bounce sequence.
- ✓ All collisions are aim-point mirrors, never velocity reflections.
- ✓ Post, net and barrier have three different speed divisors.
- ✓ The crossbar sets rather than scales speed.
- ✓ The goalmouth scatter is frame-counter-derived and therefore deterministic.
- ✓ Seven pitch types, three constants each, values known.

Open (measurement targets, [../LEGACY.md](../LEGACY.md) §15):

- Which pitch index is which named surface. The index arrives from outside this
  binary.
- Whether the ball has any spin state that survives a frame beyond the aftertouch
  window. Nothing in `UpdateBall` suggests so, but `Sprite` $54–$5E are unread here.
- The exact behaviour of `sub_107260` (asm:21759), called on a high-energy bounce.
  It sits outside the physics block and is probably the bounce sound trigger, but
  that is inferred, not read.
- Whether the goalmouth scatter applies to the keeper's parry as well as to frame
  rebounds.

---

## 10. Guidance for the reimplementation

- **Implement friction as subtraction.** It is tempting to write `speed *= 0.99`;
  do not. The linear model gives SWOS its distinctive "ball dies suddenly at the end
  of a roll" feel, and every constant in the tables is calibrated against it.
- **Keep the aim-point mirror.** Reflections through `dest` cost one subtraction and
  automatically compose with curl, which is still adjusting `dest` on the same
  frames. A velocity-reflection model has to special-case that interaction.
- **Simulate the landing prediction, do not solve it.** Reuse the integrator. This
  is cheap and it guarantees the predictor and the physics cannot diverge — which
  is a correctness property, not an optimisation.
- **Make the pitch constants a three-field struct set once per match**, exactly as
  `InitGame` does. Do not thread the pitch type through the physics.
- **Preserve the frame-counter scatter as-is.** It is deterministic and belongs
  inside the `at_core` tick. Replacing it with a call into our RNG would change the
  RNG consumption sequence and break trace comparison for everything downstream.
- **Test the bounce cut-off first.** It is a single constant that controls whether
  a dropping ball settles in half a second or jitters for three, and it is the
  fastest way to tell whether our fixed-point matches.
