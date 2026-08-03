# GOALKEEPER.md

The goalkeeper: where he stands, when he dives, and the two-stage resolution that
turns a shot into a goal or a save.

The shot that arrives is [KICKING.md](KICKING.md); the ball physics after a parry are
[BALL.md](BALL.md); the keeper's immunity to fouls is [CONTEST.md](CONTEST.md) §4.

> **Provenance.** `ShouldGoalkeeperDive` (asm:38560), `GetFramesKeeperNeedsToReachBall`
> (asm:38672), the goalkeeper branch of `SetPlayerWithNoBallDestination` (asm:36060)
> and the shot-resolution block at asm:42489–42700 are read directly. All constants
> are literals. The goal/save chance table is the closest thing in this binary to a
> result simulator and is treated at length in §4.

---

## 0. One-paragraph version

The keeper's resting position is a linear interpolation: his X is the ball's X mapped
from the pitch's 510-pixel width onto a 103-pixel arc across his goal, and his Y is
the ball's Y mapped from the 641-pixel pitch length onto a 27-pixel band off his
line. He never leaves that band unless a specific state pulls him out. When a shot
comes in, two things happen in sequence and they are independent. First, a **goal or
save roll**: the striker's Finishing minus the keeper's goalie skill, offset by seven,
indexes a sixteen-entry chance table, compared against four bits of the frame
counter — not `Rand`. If the shot is beaten, it is a goal outright. If not, the
keeper is committed to a save and `ShouldGoalkeeperDive` decides whether he can
physically reach it: it computes, in frames, how long the ball needs to arrive and
how long the keeper needs to get across, and dives only if he is quicker. Penalties
bypass the reachability test with two fixed reach distances chosen by a coin flip.

---

## 1. Positioning

The goalkeeper branch of `SetPlayerWithNoBallDestination` (asm:36060) is four lines of
arithmetic and no logic:

```
destX = 285 + (ballX - 81)  × 103 / 510
destY = base + (ballY - 129) × span / 641
```

where the Y parameters differ by end:

| Keeper | `base` | Upper limit | `span` |
|---|---|---|---|
| Top (left team) | 135 | 161 | 27 |
| Bottom (right team) | 737 | 763 | 27 |

So the keeper tracks the ball across a **103-pixel horizontal arc** (285 … 387,
centred on the goal at 336) and a **27-pixel depth band**. The goal mouth is 302 …
366, 64 pixels wide, so he ranges about 20 pixels past each post.

The depth band is the interesting one. It is driven by the *ball's* Y, so as an
attack advances the keeper advances with it — 27 pixels off his line at the far end,
back on it when the ball is at his feet. That is the whole of SWOS's keeper
positioning: no angle narrowing, no sweeping, no decision.

Speeds: `goalkeeperGameSpeed` 1024 during play, `goalkeeperSpeedWhenGameStopped` 1024
as well (asm:30705, asm:30578) — 2 px/frame, 100 px/s, comfortably below the fastest
outfielder.

`DoGoalKeeperSprites` (asm:22234) selects the dive sprites and is presentation only.

---

## 2. Getting to the shot

`GetFramesKeeperNeedsToReachBall` (asm:38672) is a small integer division routine
whose contract is worth stating precisely, because everything in §3 depends on it.

Given a distance in `d4` and a per-frame rate in `d0`, it returns in `d7` the number
of frames to cover the distance, computed by **normalising the rate to at least
$10000 by doubling, counting the doublings, then repeatedly subtracting**. It is a
shift-and-subtract divide with the quotient accumulated in the shifted units. A rate
of zero returns zero frames, which the callers treat as "never" rather than
"instantly" — every call site tests for zero and bails out (asm:38621, asm:38636,
asm:38650).

That inversion is easy to get wrong in a port: **a zero return means unreachable.**

---

## 3. The dive decision

`ShouldGoalkeeperDive` returns 1 to attempt a save, 0 to stand.

### Step one: is the ball even in front of him?

```
d0 = ballY - keeperY,  negated for the right team
if d0 >= 0:                    ball is behind → go to the distance test
if d0 < -10:                   too far in front → no dive
otherwise:                     dive
```

The −10 window means a ball within ten pixels in front of the keeper always triggers
a dive attempt regardless of anything else — the reflex block.

### Step two: penalties

If a penalty or a shootout is running (asm:38593), the reachability test is skipped
entirely. Instead:

```
d1 = keeperPenaltySaveDistanceFar          ; 20
if (Rand() & $18) == 0:  d1 = keeperPenaltySaveDistanceNear   ; 12
dive if distance <= d1
```

`Rand() & $18` isolates bits 3–4, so it is zero one time in four. **Three penalties
in four use the 20-pixel reach; one in four uses 12.** That is the only randomness in
the keeper's positional logic, and it is what makes penalties feel like a lottery
without making them one.

### Step three: normal shots — the three-way race

For an open-play shot (asm:38609 onward):

1. Reject immediately if the ball is more than `keeperSaveDistance` = **24** pixels
   past the keeper.
2. Compute `d1` = frames for the ball to arrive vertically, from |ballY − keeperY|
   and the ball's `deltaY`.
3. Compute `d2` = frames for the keeper to cover the horizontal gap, from
   |`ballDefensiveX` − keeperX| and the keeper's `deltaX`. **If `d2` ≤ `d1`, no
   dive** — he can simply walk there and does not need to.
4. Compute `d3` the same way but using a rate drawn from a reaction table:

```
i    = sField_18.index_dseg_11061B[(stoppageTimer & $3C) >> 1]
rate = dseg_11061B[i]                      ; one of $30000 … $68000
d3   = frames at that rate
if d3 == 0 or d3 < d1:  no dive
```

`dseg_11061B` (asm:30539) holds eight rates from $30000 to $68000 in steps of
$8000 — 3.0 to 6.5 px/frame. Which one is used is selected by a **16-entry index
table inside the per-side `sField_18` block**, itself indexed by six bits of the
frame counter. So the keeper's effective dive speed cycles deterministically through
a per-team profile as the match runs.

That is almost certainly the difficulty knob: `sField_18` is per-side, the index
table is 16 entries, and the rates it selects are the only thing standing between a
keeper reaching a shot and not. It is flagged `[UNKNOWN]` because nothing in this
binary writes the index table.

Note the ordering: the keeper dives only when he is **slower than the ball**
(`d3 ≥ d1`) but **not so slow that he cannot get there** (`d3 != 0`), and only when
walking would not have sufficed (`d2 > d1`). A dive is the last resort, not the
first.

---

## 4. Goal or save

This runs *before* the dive decision, in the shot-resolution block at asm:42569.

### The gates

A shot only reaches resolution if all of these hold (asm:42571–42587):

| Gate | Condition |
|---|---|
| Height | ball `z` ≤ 16 |
| Save latch | `goalkeeperSavedCommentTimer` ≤ 0 |
| Proximity | `ballDistance` ≤ 128 (≈ 11 px) |
| Height band | `ballAbove17` clear |
| Intent | the attacking side's fire button is down |
| Recency | if the attacker is not in possession, `passKickTimer` ≥ 22 |

The last one is a 22-frame arming delay: a ball that has only just been kicked cannot
already be resolving at the keeper. It stops a shot taken inside the six-yard box
resolving on the frame it is struck.

### The roll

```
d1 = striker.Finishing            ; 0 if no identified striker
d1 -= keeper.goalieSkill
d1 += 7                            ; shift into 0 … 14
d0 = (stoppageTimer >> 1) & 15
if d0 < goalScoredChances[d1]:  GOAL
else:                           SAVE
```

`goalScoredChances` (asm:34815) is `1, 2, 3, … 15, 0` — sixteen entries, the first
fifteen simply counting up.

So with the +7 offset, the probability of a goal is:

| Finishing − GoalieSkill | −7 | −4 | −1 | 0 | +1 | +4 | +7 |
|---|---|---|---|---|---|---|---|
| Index | 0 | 3 | 6 | 7 | 8 | 11 | 14 |
| Chance of 16 | 1 | 4 | 7 | 8 | 9 | 12 | 15 |
| Probability | 6.25 % | 25 % | 43.75 % | **50 %** | 56.25 % | 75 % | 93.75 % |

**An evenly matched striker and keeper is exactly 50/50**, the same design signature
as the tackle contest ([CONTEST.md](CONTEST.md) §3) — but here the curve is far
steeper: each attribute point is worth 6.25 points of probability against the
tackle's 3.125, and the range runs almost the full 0–100 rather than 50–72.

The sixteenth entry is 0, unreachable through the +7 offset with 0–7 attributes
(maximum index is 14). It is a guard.

### The source of randomness is the clock

`d0` comes from `stoppageTimer >> 1`, **not from `Rand`**. Like the goalmouth scatter
([BALL.md](BALL.md) §6), this is deterministic pseudo-randomness derived from when
the event occurred. Two identical shots at the same instant of the same match give
the same result; the variation comes from the impossibility of arriving on exactly
the same frame twice.

This matters for us: it means the shot outcome is a pure function of match state and
consumes no RNG, so it can be traced and replayed without threading a generator
through it.

### On the outcome

- **Goal** (asm:42613): `goalkeeperSavedCommentTimer` is set to −5, the keeper is
  turned toward the ball, the ball's speed is captured and **clamped to 1536**, the
  aftertouch window is killed, and `GoalkeeperJumping` runs the despairing dive.
- **Save**: `goalkeeperSavedCommentTimer` is set to +5 and control falls through to
  `ShouldGoalkeeperDive`.

The +5 / −5 latch on one field is how the rest of the engine knows what just
happened; `ApplyBallAfterTouch` reads it to surrender control after a goal
([AFTERTOUCH.md](AFTERTOUCH.md) §1).

---

## 5. Catching, claiming and distributing

Three routines handle the ball once the keeper has it.

- **`GoalkeeperCaughtTheBall`** (asm:39290) — transition into the catch.
- **`GoalkeeperClaimedTheBall`** (asm:40627) — sets `PL_GOALIE_CLAIMED` and stops
  play with `gameState` 3, the one restart that does not wait for the ball to come
  to rest ([TIMING.md](TIMING.md) §4).
- **`UpdateBallWithControllingGoalkeeper`** (asm:39965) — holds the ball at the
  keeper's hands each frame using the ±1 offset table `unk_113864` (asm:39905), the
  same octant-offset idiom used everywhere else but at unit scale.

The keeper's approach behaviour while the ball is loose in his area is in the
`player_goalkeeper` branch (asm:42218–42480): he is drawn to the ball inside the
penalty area, refuses to come for a ball that is a shot on goal or very close
(asm:42376), and centres himself on the ball's X when it is stationary in the
six-yard area (asm:42412).

`GoalkeeperJumping` (asm:39480) drives the dive itself, using a small set of speeds
at asm:30512–30520 ($200, $300, $400, $500, $600, $800) selected by dive type.

---

## 6. Constants quick reference

| Symbol | Line | Value | Meaning |
|---|---|---|---|
| Keeper X arc | 36061 | 285 … 387 | 103 px, centred on goal |
| Keeper Y band, top | 36074 | 135 … 161 | 27 px |
| Keeper Y band, bottom | 36077 | 737 … 763 | 27 px |
| Pitch width divisor | 36067 | 510 | |
| Pitch length divisor | 36089 | 641 | |
| `goalkeeperGameSpeed` | 30705 | 1024 | 2 px/frame |
| `goalkeeperSpeedWhenGameStopped` | 30578 | 1024 | |
| Reflex window | 38571 | −10 px | Always dive inside this |
| `keeperSaveDistance` | 34835 | 24 | Normal shot reach |
| `keeperPenaltySaveDistanceFar` | 30556 | 20 | 3 penalties in 4 |
| `keeperPenaltySaveDistanceNear` | 30557 | 12 | 1 penalty in 4 |
| Penalty reach selector | 38598 | `Rand() & $18` | Zero 1 time in 4 |
| `dseg_11061B` dive rates | 30539 | $30000 … $68000 | 3.0 … 6.5 px/frame |
| Rate index source | 38634 | `stoppageTimer & $3C` | Deterministic cycle |
| Shot height gate | 42571 | z ≤ 16 | |
| Shot proximity gate | 42577 | 128 squared ≈ 11 px | |
| Shot arming delay | 42587 | 22 frames | |
| `goalScoredChances` | 34815 | 1 … 15, 0 | Goal odds of 16 |
| Chance roll source | 42607 | `stoppageTimer >> 1` | Not `Rand` |
| Goal ball speed clamp | 42630 | 1536 | |

---

## 7. What this resolves, and what still needs measurement

Confirmed:

- ✓ Keeper position is a pure two-axis linear interpolation from ball position, with
  exact ranges.
- ✓ No angle-narrowing, no sweeping, no decision-making in positioning.
- ✓ The dive test is a three-way frames-to-arrive race, and a dive only happens when
  walking will not do.
- ✓ A zero return from the frames routine means unreachable, not instant.
- ✓ Penalties use two fixed reaches on a 3:1 split.
- ✓ Goal-or-save is a Finishing-minus-GoalieSkill lookup, exactly 50/50 when level,
  6.25 points per attribute point, spanning 6.25 % to 93.75 %.
- ✓ The roll consumes no RNG; it reads the frame counter.
- ✓ The 22-frame arming delay on shots.
- ✓ Goals clamp ball speed to 1536.

Open (measurement targets, [../LEGACY.md](../LEGACY.md) §15):

- **The `sField_18` dive-rate index table.** Sixteen entries, per side, selecting
  between eight dive speeds. This is very likely the difficulty setting and it is the
  single highest-value unknown in this document. Nothing in the match module writes
  it.
- Whether `lastHeadingPlayer` is always the right striker for the Finishing lookup.
  If it is stale, the goal roll silently uses Finishing 0.
- The dive-speed selection inside `GoalkeeperJumping` — six speeds are present, the
  selector was not traced.
- Whether the keeper's refusal to come for a ball (asm:42376) has an attribute term.

---

## 8. Guidance for the reimplementation

- **Implement positioning as the interpolation, verbatim.** It is four lines and it
  is correct. Any "smarter" keeper positioning changes the feel of every attack.
- **Keep goal-resolution and dive-decision separate and in that order.** They are
  independent, and merging them — deciding the save from the physics — produces a
  keeper that is either unbeatable or useless. The original decides the *outcome*
  first and animates it second.
- **Drive both the goal roll and the penalty reach from deterministic sources**
  where the original does. The goal roll must not consume RNG; the penalty reach
  must. Getting this backwards will desynchronise every trace downstream.
- **Make the even contest exactly 50 %.** As with tackles, this is a design
  statement.
- **Preserve the "zero means unreachable" contract** in whatever replaces
  `GetFramesKeeperNeedsToReachBall`, and assert on it. It is the kind of inverted
  sentinel that survives a port as a subtle bug for months.
- **Expose the dive-rate profile as our difficulty knob.** Even without knowing the
  original's values, the structure — a per-side table selecting keeper reach speed —
  is the right shape for the setting, and building it in now costs nothing.
