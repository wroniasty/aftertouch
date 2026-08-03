# MOVEMENT.md

How anything on the pitch moves: the angle model and its two lookup tables, the
conversion from a scalar speed to a per-axis velocity, where a player's speed comes
from, and what reduces it.

The per-frame pipeline this sits inside is [TIMING.md](TIMING.md) §1. What a player
does *with* possession is [CONTEST.md](CONTEST.md); where an off-ball player is told
to go is [AI.md](AI.md) §2.

> **Provenance.** `CalculateDeltaXAndY` (asm:20661), `UpdatePlayerSpeed` (asm:35391)
> and the movement sections of `UpdatePlayersAndBall` are read directly. Both trig
> tables and both player-speed tables are literals in the source. The 1/512 speed
> unit is derived, not stated — the derivation is in [STATE.md](STATE.md) §1 and is
> repeated in §2 below because everything else depends on it.

---

## 0. One-paragraph version

Nothing in SWOS carries a velocity vector as authoritative state. A body carries a
scalar `speed` and an aim point `(destX, destY)`, and `CalculateDeltaXAndY` derives
the vector fresh every frame: it reduces the offset to the aim point until both
components fit in five bits, looks the ratio up in a 32 × 32 arctangent table to get
a heading in 256ths of a turn, then multiplies a Q7 sine and cosine by the speed to
produce two 16.16 increments. The heading is also collapsed to an octant for
animation and for every gameplay test that cares about direction. A player's speed
comes from an eight-entry table indexed by the Speed attribute — 928 to 1250 during
play, a much flatter 1136 to 1248 while play is stopped — and is then reduced by
injury, by carrying the ball, by a global handicap flag, and by the deceleration
ramps that run during restarts. Turn restrictions are applied not to the movement
but to the *input*, by masking which octants are acceptable before the destination
is set.

---

## 1. Angles and the two tables

### The representation

Headings are integers in **256ths of a turn** (`fullDirection`, 0–255). The octant
(`direction`, 0–7) is derived with a rounding offset (asm:21668):

```
direction = ((fullDirection + 16) & 255) >> 5
```

The `+16` is half an octant, so each octant is centred on its cardinal rather than
starting at it. The octant → screen-vector mapping is fixed by
`defaultPlayerDestinations` and is tabulated in [STATE.md](STATE.md) §1.

### `CalculateDeltaXAndY` — asm:20661

Inputs: aim point in `d1`/`d2`, current position in `d3`/`d4`, speed in `d0`.
Outputs: `deltaX` and `deltaY` as 16.16 longwords, and the heading, or −1 if the
body is not moving.

1. Take the offset to the aim point and record the sign of each axis, then take
   absolute values.
2. **Halve both components together** until each is below 32 (asm:20679–20694). This
   preserves the ratio while shrinking it into the table's domain. It is a
   right-shift loop, so it also quantises the ratio — two nearby aim points can
   produce the same heading, and at long range they usually do.
3. Index `kAngleCoeficients[dy × 32 + dx]` (asm:20774) for the first-quadrant angle.
   The table is 1 024 words holding `atan2(dy, dx)` in 128ths of a turn; entry
   `[0][0]` is −1 and is the "not moving" sentinel.
4. Fold the sign bits back to select the quadrant (asm:20706–20730).
5. Look up `kSineCosineTable` (asm:20761), a 256-entry Q15 sine covering a full
   circle, twice — once at the heading and once 64 entries on for the cosine.
6. Shift both from Q15 to Q7 (`asr #8`) and multiply by the speed. The 32-bit
   products *are* the 16.16 deltas.

Step 6 is where the unit falls out: a Q7 sine peaks at 127, so a full-speed axis
increment is `speed × 127 / 65536` pixels — **speed 512 ≈ 1 px/frame**.

Step 2 is the one with a behavioural consequence. The halving loop means the
heading is computed from a *reduced* ratio, so accuracy degrades with distance.
Because aim points are routinely set 1 000 pixels away
([STATE.md](STATE.md) §1), most movement is resolved at the coarsest quantisation —
which is exactly why eight-way movement feels crisp rather than analogue even though
the underlying heading has 256 steps.

---

## 2. Speed as a scalar

Every body's `speed` is a plain 16-bit integer in units of ~1/512 px/frame. It is
modified by addition and subtraction throughout — friction subtracts, shot bonuses
add, injuries subtract — and the vector is only ever derived from it. This is why
the shot bonus tables ([KICKING.md](KICKING.md) §3) contain signed values that look
small next to the base kick speed: they are composable offsets, not multipliers.

Reference conversions at 50 Hz:

| Raw speed | px/frame | px/s | Crosses the 641 px pitch in |
|---|---|---|---|
| 512 | 1.0 | 50 | 12.8 s |
| 928 (Speed 0) | 1.81 | 91 | 7.1 s |
| 1250 (Speed 7) | 2.44 | 122 | 5.3 s |
| 2048 (jump header) | 4.0 | 200 | — |
| 2208 (kick) | 4.31 | 216 | 3.0 s |

The spread between the slowest and fastest outfielder is **35 %**. That is a
deliberately narrow band: SWOS does not let pace alone win a game.

---

## 3. Where a player's speed comes from

`UpdatePlayerSpeed` (asm:35391) runs once per player per frame, and only for players
in `PL_NORMAL` — any other state keeps whatever speed its own handler set.

### Base

Two tables, selected by whether play is live (asm:35407):

| Speed attribute | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
|---|---|---|---|---|---|---|---|---|
| `playerSpeedsGameInProgress` | 928 | 974 | 1020 | 1066 | 1112 | 1158 | 1204 | 1250 |
| `playerSpeedsGameStopped` | 1136 | 1152 | 1168 | 1184 | 1200 | 1216 | 1232 | 1248 |

The stopped-play table is nearly flat — a 10 % spread against 35 % — and sits at the
*top* of the live range. Everyone jogs back into position at roughly the same brisk
pace. This is a presentation decision that happens to live in the simulation.

There is one exception before the table is even consulted: a **goalkeeper who is not
the controlled player** is skipped entirely during live play (asm:35395), keeping
whatever speed the keeper logic assigned.

### Reductions, in application order

| Condition | Effect | Line |
|---|---|---|
| `runSlower` global set | `speed -= speed/4 + speed/8` (× 5/8) | 35414 |
| Injured, human side | `speed += injuriesSpeedPenalty[level]` | 35440 |
| Is the controlled player **and** has the ball | `speed -= speed/8` (× 7/8) | 35452 |
| Restart states $17/$18 | `speed = speed × min(100, 4×stoppageTimerTotal) / 100` | 35509 |
| Restart states $1D/$1E | `speed -= 32 × stoppageTimerTotal`, clamped at 0 | 35521 |

`injuriesSpeedPenalty` (asm:35536) is indexed by the top three bits of
`PlayerGame.injuriesBitfield`:

| Level | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
|---|---|---|---|---|---|---|---|---|
| Penalty | 0 | −96 | −128 | −160 | −192 | −224 | −256 | −288 |

At level 7 a Speed-7 player drops from 1250 to 962 — below an uninjured Speed-0
player. Injuries are severe, and they only apply on a human-controlled side
(asm:35434 gates on `playerNumber`/`plCoachNum`), which is a genuine asymmetry.

**Carrying the ball costs 12.5 %.** That single line is what makes a dribbler
catchable, and it applies only to the player actually under control — an AI
teammate in possession does not pay it.

### Pass receivers run to meet the ball

If this player is the intended receiver of a long or curled pass, and the ball is
moving in roughly the same direction he faces, his speed is *overridden* outright
(asm:35462–35482):

| Heading difference (of 256) | Speed set to |
|---|---|
| within ±5 | 256 |
| ±6 to ±7 | 512 |
| beyond ±7 | no override |

Both are far below normal running speed, so the receiver **slows down** to let the
ball arrive. The tighter the alignment, the slower he goes. This is the mechanic
that makes SWOS's long passes look intentional.

### Animation cadence

The last thing `UpdatePlayerSpeed` does is set the frame-cycle reload (asm:35526):

```
field_18 = max(0, $500 - speed) >> 7 + 6
```

Faster player, smaller reload, quicker leg cycle. Speed and animation rate are
coupled in the simulation, not in the renderer.

---

## 4. Turn restrictions

Direction changes are constrained by masking the *input*, before any destination is
set. `TeamGeneralInfo.allowedDirections` ($2A) holds a bitmask of acceptable
octants; the loop at `find_acceptable_turn_flags_loop` (asm:43359) walks outward
from the requested octant until it finds one the mask permits, and uses that.

The mask is loaded per restart by `GameSetup`, which writes a `playerTurnFlags` byte
alongside every restart placement — see [SETPIECES.md](SETPIECES.md) §2 for the
values. During open play the mask is permissive; during a throw-in or a corner it
restricts the taker to a realistic arc.

Because the restriction is applied to input rather than to motion, a player never
turns *through* a forbidden direction — he simply cannot select it. There is no
turn-rate limit anywhere in the engine: an unrestricted player reverses in one
frame.

---

## 5. Pitch clamps and boundaries

Two different clamps, applied in different places.

**Destination clamp**, applied by `SetPlayerWithNoBallDestination` (asm:36030) to
AI-positioned players only:

| Axis | Range |
|---|---|
| X | 81 … 590 |
| Y | 129 … 769 |

**Position tests**, applied to the controlled player (asm:43750–43772): the four
comparisons against the same bounds gate whether the destination update happens at
all, so a player pushed against the touchline stops rather than sliding along it.

A sliding tackle has its own set (asm:44042–44080): a tackle that ends outside
X 73 … 598 or Y 121 … 777 has its speed cut by an eighth per frame, and one that
ends inside the goal-mouth strip X 275 … 396 while off the pitch is cut to a
sixteenth. That is the "slide into the net and stop dead" behaviour.

---

## 6. Constants quick reference

| Symbol | Line | Value | Meaning |
|---|---|---|---|
| `kSineCosineTable` | 20761 | 256 × Q15 | Full-circle sine |
| `kAngleCoeficients` | 20774 | 32 × 32 | `atan2` in 128ths of a turn; `[0][0]` = −1 |
| Octant rounding offset | 21668 | +16 | Half an octant |
| Speed unit | derived | ~1/512 px/frame | 512 ≈ 1 px/frame |
| `playerSpeedsGameInProgress` | 34726 | 928 … 1250 | By Speed 0–7 |
| `playerSpeedsGameStopped` | 34734 | 1136 … 1248 | By Speed 0–7 |
| `injuriesSpeedPenalty` | 35536 | 0 … −288 | By injury level 0–7 |
| `substitutedPlSpeed` | 30574 | 1536 | Player leaving/entering the pitch |
| `goalkeeperSpeedWhenGameStopped` | 30578 | 1024 | |
| `goalkeeperGameSpeed` | 30705 | $400 (1024) | |
| `playerTacklingSpeed` | 30706 | $700 (1792) | Slide launch speed |
| `jumpHeaderSpeed` | 30707 | 2048 | Jumping header launch |
| `playerGroundConstant` | 30584 | 96 | Player friction, per frame |
| Ball-carrier penalty | 35452 | × 7/8 | |
| `runSlower` handicap | 35414 | × 5/8 | |
| Receiver speed, aligned | 35478 | 256 | |
| Receiver speed, near-aligned | 35473 | 512 | |

Player friction at 96 per frame is **six times** the ball's ground friction of 16 —
a sliding player at `playerTacklingSpeed` 1792 stops in 19 frames, well under half a
second.

---

## 7. What this resolves, and what still needs measurement

Confirmed:

- ✓ Heading is 256-step; octant is derived with a half-octant rounding offset.
- ✓ Both trig tables and their exact contents.
- ✓ The halving reduction, and therefore that heading precision degrades with
  distance to the aim point.
- ✓ Speed unit derivation, and hence real px/s figures for every table.
- ✓ Two distinct player speed tables, live and stopped, with the stopped table
  deliberately flat.
- ✓ Ball-carrier penalty is exactly 12.5 %.
- ✓ Injury penalties, their scale, and that they apply only to human sides.
- ✓ Pass receivers are *slowed* to 256 or 512 to meet the ball.
- ✓ Animation cadence is derived from speed inside the simulation.
- ✓ Turn restriction is input masking with no turn-rate limit.

Open (measurement targets, [../LEGACY.md](../LEGACY.md) §15):

- What sets `runSlower`. It reads as a difficulty or handicap flag but is written
  outside the match module.
- Whether the injury asymmetry (human sides only) is intentional or a bug. It is
  unambiguous in the code; whether it was *meant* is not answerable from here.
- The full `allowedDirections` mask semantics during open play — `GameSetup` writes
  the restart masks but the open-play value's origin is not traced.
- Whether `sField_18.speed_unk` (used for the loose-ball chase, asm:42527) varies by
  difficulty.

---

## 8. Guidance for the reimplementation

- **Derive the velocity vector every frame from speed and aim point.** Do not cache
  it. Curl, rebounds and dribble touches all work by moving the aim point and rely
  on the derivation happening afterwards.
- **Reproduce the halving reduction rather than calling `atan2`.** An exact
  arctangent gives a *different heading* for the same inputs, and headings feed
  octant tests that gate shooting, heading and passing. This is not a precision
  nicety; it changes outcomes.
- **Keep speed as an integer scalar with additive modifiers.** The whole tuning
  system is built on composable offsets.
- **Implement turn restriction as input masking**, walking outward from the
  requested octant. A turn-rate model looks more physical and feels wrong.
- **Do not smooth the animation cadence.** It is computed from speed in the
  simulation, and highlight playback will drift from live play if the renderer owns
  it instead.
- **Keep the two speed tables separate.** The temptation is to scale one from the
  other; the flatness of the stopped table is a design choice worth preserving
  verbatim.
