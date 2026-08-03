# AFTERTOUCH.md

The ten frames after a kick, during which the joystick still steers the ball. The
mechanic this project is named after, and the one the Amiga source documents most
completely.

The launch itself is [KICKING.md](KICKING.md). What the ball does with the modified
aim point is [BALL.md](BALL.md) §1.

> **Provenance.** `ApplyBallAfterTouch` (asm:40089) is read directly and is
> unambiguous — it is a single routine with two near-identical halves, one for kicks
> and one for passes. Both spin tables, the decay ramp and the height-switch
> constants are literals at asm:30735–30809. This is the best-documented mechanic in
> the binary and there is very little left to guess.

---

## 0. One-paragraph version

Kicking a ball starts a counter, `spinTimer`, at zero. For the next ten frames
`ApplyBallAfterTouch` runs before the ball is integrated and, if the joystick is
held, adds a lateral offset to the ball's aim point — bending the flight. The
direction of the bend is decided **once**, on the first frame the player pushes:
the engine compares the joystick octant against the octant the ball was kicked in,
latches "left" or "right", and every subsequent frame uses that latched side
regardless of what the joystick does afterwards. You cannot S-bend a shot. The
magnitude comes from a per-octant table of (dx, dy) offsets multiplied by a decay
ramp that falls from 5 to 1 across the window, so almost all the curl is applied in
the first three frames. Separately, on **frame 4 exactly**, a one-shot switch reads
the joystick again and changes the ball's height and speed: pulling back from the
kick direction lofts it, pushing across flattens it, pushing forward does nothing.
Passes use the same machinery with a curl table roughly half as strong and a
gentler, un-timed height nudge.

---

## 1. The window

`spinTimer` (`TeamGeneralInfo` +$76) is the whole state machine:

| Value | Meaning |
|---|---|
| −1 | Inactive |
| 0 … 9 | Frame *n* of the aftertouch window |

`ResetLeftAndRightSpinTimers` (asm:40348) sets **both** teams' timers to −1. It is
called on every launch (asm:35052), on every ball–player contact, on post and net
rebounds ([BALL.md](BALL.md) §6), and when possession changes. A kick sets its own
team's timer to 0 immediately afterwards.

The counter is advanced at the *end* of each application (asm:40206), and reaching
10 sets it back to −1. Ten frames at 50 Hz is **200 milliseconds**.

`ApplyBallAfterTouch` is invoked from the player-switch timer block at the top of
`UpdatePlayersAndBall` (asm:42026), so it runs once per team per frame, before any
player is processed and therefore before the ball moves.

### When it does not run

Three early exits (asm:40092–40098):

- The **opponent's** `goalkeeperSavedCommentTimer` is negative — a goal has just
  been scored, so control is surrendered.
- Play is not live *and* `gameState` is 3 (keeper holding).
- `spinTimer` is already −1.

---

## 2. Side latching

The first thing each frame is to establish which way the ball is bending
(asm:40103–40133):

```
if leftSpin or rightSpin already set:  use it
else:
    joy = TeamGeneralInfo.currentDirection            ; -1 if neutral
    if joy < 0:                    no curl this frame
    diff = (kickDirection - joy) & 7                  ; kickDirection = field_38
    if diff == 0 or diff == 4:     no curl this frame
    if diff < 4:   leftSpin  = 1,  side index = 0
    else:          rightSpin = 1,  side index = 4
```

Once `leftSpin` or `rightSpin` is set, the branch at asm:40105 skips the comparison
entirely for the rest of the window. **The side is decided on the first frame the
player pushes and cannot be changed.**

`diff` is the joystick's octant relative to the octant the ball was kicked in.
Pushing straight along the kick (0) or straight against it (4) produces no lateral
component, which is geometrically right. Differences 1–3 curl one way, 5–7 the
other.

Note that `field_38` — the kick direction — is written by whichever routine launched
the ball ([KICKING.md](KICKING.md) §1), so curl is measured against the *kick*, not
against the ball's current heading. A ball already bending continues to be measured
against its original launch axis.

---

## 3. The curl

With the side index (0 or 4) and the kick octant, the offset is a table lookup
(asm:40136–40151):

```
i     = kickDirection × 8 + sideIndex
mag   = spinMultiplierFactor[spinTimer]
destX += kickSpinFactor[i]     × mag
destY += kickSpinFactor[i + 1] × mag
```

### `kickSpinFactor` — asm:30746

Thirty-two words: eight kick octants × two sides × (dx, dy). Laid out readably:

| Kick octant | Left side (dx, dy) | Right side (dx, dy) |
|---|---|---|
| 0 (up) | (−32, 0) | (0, 0) |
| 1 (up-right) | (+32, 0) | (0, −23) |
| 2 (right) | (+23, 0) | (0, 0) |
| 3 (down-right) | (0, −32) | (0, +32) |
| 4 (down) | (+23, 0) | (0, 0) |
| 5 (down-left) | (+23, +32) | (0, −32) |
| 6 (left) | (0, 0) | (+23, −23) |
| 7 (up-left) | (0, 0) | (+32, 0) |

The magnitudes are only ever 0, 23 or 32 — and 23 ≈ 32/√2, so the diagonal cases are
the axis-aligned magnitude resolved onto two axes. The table is a hand-tuned
perpendicular offset, not a computed one, and it is not perfectly symmetric: several
entries are (0, 0) where symmetry would predict a value. Reproduce it verbatim
rather than deriving it.

### `spinMultiplierFactor` — asm:30735

The decay ramp, indexed by `spinTimer`:

| Frame | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 |
|---|---|---|---|---|---|---|---|---|---|---|
| Multiplier | 5 | 4 | 3 | 2 | 2 | 2 | 2 | 1 | 1 | 1 |

Total across the window: 23. The first three frames contribute 12 of that — **more
than half the curl is applied in the first 60 milliseconds.** A player who reacts
late gets a fraction of the effect, which is exactly why aftertouch in SWOS rewards
anticipation rather than reaction.

Peak per-frame offset is 32 × 5 = 160 aim-point pixels on frame 0. Against a launch
aim point 1 000 pixels out, that is a first-frame heading change of about 9°,
falling to under 2° by frame 7.

---

## 4. The height switch

Independent of the curl, and the reason aftertouch is more than a curl mechanic.

### For kicks — frame 4 only

At `spinTimer == 4` exactly (asm:40154), the joystick is read again and `diff` is
recomputed against the kick direction:

| `diff` | `deltaZ` set to | `speed` set to | Effect |
|---|---|---|---|
| 3, 4, 5 (pulling back) | **$20000** (2.0 px/frame) | 2688 | **Lob** |
| 2, 6 (across) | $16000 (1.375) | 2560 | Raised drive |
| no joystick | $16000 (1.375) | 2560 | Raised drive |
| 0, 1, 7 (pushing on) | unchanged | unchanged | Stays flat |

Against the launch defaults of $14000 and 2208, pulling back raises the ball 60 %
higher *and* speeds it up by 22 %. Both branches also increase speed, which is
counter-intuitive until you remember air friction is lower than ground friction
([BALL.md](BALL.md) §2) — a lofted ball needs the extra pace to arrive at the same
time.

Then a speed correction by kick axis (asm:40170–40194):

| Kick octant | Correction |
|---|---|
| 0 or 4 (vertical) | `speed × 3/4` |
| 1, 3, 5, 7 (diagonal) | `speed × 7/8` |
| 2 or 6 (horizontal) | none |

Vertical kicks — the ones aimed straight at goal — are damped hardest. This is a
screen-space correction: the pitch is drawn with a vertical foreshortening, so a
ball travelling up-screen covers more apparent ground per pixel and needs slowing to
look right. It is a rendering concern that lives in the simulation, and it must be
reproduced or vertical shots will feel wrong.

### For passes — any frame, once

When `passInProgress` is set the second half of the routine runs (asm:40216). The
height switch is not gated on frame 4; it fires on the first frame the condition is
met and is then locked out by the `longPass` flag pair at +$7C (asm:40241).

Both the "pulling back" and "across" branches do the same thing: `speed += speed/8`.
There is no `deltaZ` change at all. A pass cannot be lofted by aftertouch — it can
only be hurried.

### Pass curl

`passingSpinFactor` (asm:30778) mirrors `kickSpinFactor` exactly in shape, with every
magnitude reduced from 32 to 16 and from 23 to 11 — **half strength**. The same decay
ramp applies, so total pass curl is 16 × 23 = 368 aim-point pixels against 736 for a
kick.

One difference in the lookup: passes index the table by the **ball's current
`direction`** (asm:40224) rather than by the stored kick direction. A pass that has
already begun to bend is measured against where it is going now, so pass curl
compounds slightly where kick curl does not.

---

## 5. Constants quick reference

| Symbol | Line | Value | Meaning |
|---|---|---|---|
| Window length | 40207 | 10 frames | 200 ms at 50 Hz |
| `spinMultiplierFactor` | 30735 | 5,4,3,2,2,2,2,1,1,1 | Decay ramp, sums to 23 |
| `kickSpinFactor` | 30746 | 0 / ±23 / ±32 | 8 octants × 2 sides × (dx, dy) |
| `passingSpinFactor` | 30778 | 0 / ±11 / ±16 | Half strength |
| Lob `deltaZ` | 30740 (`off_10E658`) | $20000 | 2.0 px/frame |
| Lob speed | 30741 (`word_10E65C`) | 2688 | |
| Drive `deltaZ` | 30742 (`off_10E65E`) | $16000 | 1.375 px/frame |
| Drive speed | 30743 (`word_10E662`) | 2560 | |
| Height switch frame | 40154 | `spinTimer == 4` | Kicks only |
| Vertical axis damping | 40197 | × 3/4 | Octants 0, 4 |
| Diagonal axis damping | 40182 | × 7/8 | Octants 1, 3, 5, 7 |
| Pass speed nudge | 40260 | +1/8 | One-shot |

---

## 6. What this resolves, and what still needs measurement

Confirmed:

- ✓ The window is exactly ten frames, counted 0–9, with −1 as the inactive sentinel.
- ✓ Curl side is latched on first push and cannot be reversed.
- ✓ Neutral joystick and the two collinear octants produce no curl.
- ✓ The full curl table, both variants, verbatim.
- ✓ The decay ramp, and hence that over half the curl lands in three frames.
- ✓ Curl is applied to the aim point, not to velocity.
- ✓ The height switch fires on frame 4 only, for kicks.
- ✓ Pulling back lofts; pushing across raises slightly; pushing forward does nothing.
- ✓ Both height branches also raise speed.
- ✓ The per-axis speed correction, including that vertical kicks are damped most.
- ✓ Passes get no height change at all, only a one-shot speed nudge.
- ✓ Kick curl indexes on the launch direction; pass curl indexes on current heading.
- ✓ Aftertouch does not survive a rebound or a contact.

Open (measurement targets, [../LEGACY.md](../LEGACY.md) §15):

- Whether `field_38` is re-written mid-window by anything. If it were, kick curl
  would compound like pass curl does; nothing found suggests it is, but the routine
  has several writers.
- Whether the height switch can fire if the window is interrupted before frame 4 and
  restarted — i.e. whether a quick second touch grants a second lob.
- Whether the two `longPass` word flags at +$7C and +$7E are genuinely independent
  or whether one is dead. Both are written; only the pair being non-zero is tested.
- Whether the asymmetries in `kickSpinFactor` (the (0, 0) entries where symmetry
  would predict a value) are intentional tuning or table-entry errors in the
  original.

---

## 7. Guidance for the reimplementation

- **This is the mechanic to get exactly right.** It is the project's name and it is
  the thing players feel. Every constant here is known; there is no excuse for
  approximating it.
- **Latch the side.** The single most important behavioural property is that you
  commit on the first push. A model that reads the joystick fresh each frame lets
  players wiggle the ball into the goal and destroys the skill ceiling.
- **Apply curl to the aim point before the ball integrates.** Ordering matters: the
  offset must be visible to the heading derivation on the same frame.
- **Keep the decay ramp as a table.** It is not exponential and not linear; fitting a
  curve to it will get frames 3–6 wrong, which is where a late reaction lands.
- **Reproduce the per-axis speed correction even though it is a rendering artefact.**
  Vertical shots are the common case in front of goal; getting them 33 % too fast is
  immediately obvious.
- **Treat the reset points as part of the mechanic.** Post rebounds, contacts and
  possession changes all kill the window, and that is what stops aftertouch from
  being usable on a ball that is no longer yours.
- **Trace the whole window.** Ten frames of `spinTimer`, `leftSpin`, `rightSpin`,
  `destX`, `destY`, `speed` and `deltaZ` per kick is a small, complete, checkable
  record — this belongs in the trace format from the start.
