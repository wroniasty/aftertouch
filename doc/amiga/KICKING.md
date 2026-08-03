# KICKING.md

Every way the ball leaves a player: the kick, the pass, and the two kinds of
header. What launch speed and rise each gets, and how player attributes modify them.

The ten frames *after* the launch — the curl and the lob switch — are
[AFTERTOUCH.md](AFTERTOUCH.md). What happens to the ball in flight is
[BALL.md](BALL.md). The dribble touch, which is not a kick, is
[CONTEST.md](CONTEST.md) §2.

> **Provenance.** `PlayerKickingBall` (asm:35033), `DoPass` (asm:34859),
> `PlayerDoingHeader` (asm:38018), `DoStaticHeader` (asm:38285) and `PlayerHeading`
> (asm:39688) are read directly. Every bonus table is a literal. The *attribute
> mapping* — which byte is Velocity and which is Finishing — is derived in
> [PLAYERS.md](PLAYERS.md) §1 and contradicts one of IDA's labels; see
> [STATE.md](STATE.md) §5.

---

## 0. One-paragraph version

A kick is three writes. The ball's aim point is set to **its own current position
plus a large offset in the kicker's facing octant**, its `speed` is set to a flat
`ballKickingSpeed` of 2208, and its `deltaZ` is set to a flat `ballKickingDeltaZ` of
$14000. Nothing about the player affects those three defaults. What *is* affected is
a bonus added afterwards, and only if the kick qualifies as a shot on goal: the
engine tests whether the kicker is in the attacking half and facing one of the three
octants toward the opponent's goal, and if so adds an eight-entry attribute bonus —
**Finishing** if the kick came from close range in front of goal, **Velocity**
otherwise. A pass is the same launch with a different aim point: `DoPass` scans the
kicker's own team for the nearest player within an eight-of-256 arc of his facing,
and aims at him. Headers split in two: a static header uses the same
destination idiom at a fixed low speed, while a jumping header sets a 50-frame
`PL_HEADING` state and adds a **Heading** bonus, with a lateral offset table letting
the player steer the ball off the axis he is facing.

---

## 1. The destination idiom

Every launch in this document does the same three things (asm:35041–35051):

```
table = GetBallDestCoordinatesTable()            ; usually defaultPlayerDestinations
ball.destX = ball.x + table[direction].dx
ball.destY = ball.y + table[direction].dy
ball.speed  = ballKickingSpeed                   ; 2208
ball.deltaZ = ballKickingDeltaZ                  ; $14000
ResetLeftAndRightSpinTimers()
```

The offsets are ±1 000 pixels ([STATE.md](STATE.md) §1) — far beyond the pitch. The
aim point is not a target, it is a heading expressed as a point, and
`CalculateDeltaXAndY` turns it back into a heading next frame. `GetBallDestCoordinatesTable`
swaps in a different offset table during set pieces, which is how a corner is aimed
differently from an open-play kick; see [SETPIECES.md](SETPIECES.md) §3.

The launch is deliberately **flat and attribute-independent**: 2208 speed
(4.31 px/frame, 216 px/s) and $14000 rise (1.25 px/frame, 62.5 px/s) for everyone.
Skill enters only through the bonus in §3 and through the aftertouch the player then
applies. A weak player and a strong player hit a clearance identically.

---

## 2. Shot-on-goal classification

`PlayerKickingBall` decides whether a kick counts as an attempt (asm:35059–35116).
It is pure geometry — no attribute, no RNG.

### Step one: is the kicker attacking?

| Side | Requires ball Y | Requires facing octant |
|---|---|---|
| Right team (attacking the **top** goal) | ≤ 342 | 0, 1 or 7 |
| Left team (attacking the **bottom** goal) | ≥ 556 | 3, 4 or 5 |

Both thresholds sit either side of the pitch centre line at 449, about 107 pixels
into the attacking half. Fail either test and the kick gets no bonus at all — a
sideways pass out of defence is never treated as a shot.

### Step two: which bonus?

Given a qualifying kick:

| Ball X | Ball Y | Classification |
|---|---|---|
| outside 241 … 431 | any | **Long shot** → Velocity bonus |
| 241 … 431 | < 204 or ≥ 694 | **Finishing shot** → Finishing bonus |
| 241 … 431 | 204 … 693 | **Long shot** → Velocity bonus |

The X band 241–431 is 190 pixels wide, centred on the goal at 336 — roughly the
width of the penalty area. The Y thresholds 204 and 694 sit 75 and 76 pixels from
the goal lines at 129 and 769.

So: **inside the box and central, Finishing decides; anywhere else, Velocity does.**
That is a clean, legible rule and it is worth stating plainly because it is the
single most consequential attribute mapping in the game.

---

## 3. The two bonus tables

Both are signed offsets added to the flat launch speed of 2208 (asm:35100, asm:35113).

| Attribute value | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
|---|---|---|---|---|---|---|---|---|
| `ballSpeedKicking` (Velocity, long shots) | −384 | −270 | −162 | −54 | +54 | +162 | +270 | +384 |
| `ballSpeedFinishing` (Finishing, close range) | −288 | −160 | −32 | +96 | +224 | +352 | +480 | +608 |

Resulting launch speeds:

| Attribute | Long shot | px/s | Finishing shot | px/s |
|---|---|---|---|---|
| 0 | 1824 | 178 | 1920 | 188 |
| 3 | 2154 | 210 | 2304 | 225 |
| 7 | 2592 | 253 | 2816 | 275 |

Two observations.

**Velocity is symmetric about the base; Finishing is not.** `ballSpeedKicking`
straddles zero evenly, so an average Velocity player kicks at almost exactly the
default. `ballSpeedFinishing` is skewed upward — its centre of mass is around +100 —
so close-range shots are systematically harder than long ones for the same attribute
value, and a Finishing-7 striker hits 275 px/s, the fastest ball in the game.

**The spread is much wider for Finishing**: 896 units across the scale against 768.
Finishing matters more, and it matters exactly where you would want it to.

---

## 4. Passing

`DoPass` (asm:34859) runs when the fire button is tapped rather than held. It differs
from a kick in three respects: the aim point is a teammate, the strength is derived
from the distance to him, and the receiver enters a state in which he cannot be
controlled.

**Passing has its own document — [PASSING.md](PASSING.md).** The short version:

- `GetClosestNonControlledPlayerInDirection` (asm:39859) picks the nearest own player
  within **±22.5° of the passer's facing as measured from the ball**.
- Direction is set by extending the ball→receiver ray until it leaves the pitch, so
  the ball travels *through* the receiver rather than stopping at him.
- **Strength is banded by distance to the receiver** — eight 50-pixel bands from
  $600 to $8AA — plus a **Passing** bonus of up to +384.
- CPU passes get an accuracy roll driven by Passing; human passes are always exact.
- With no target in the cone the pass degrades to a directional clearance at a flat
  $700 (asm:34994).

Pass curl uses the weaker `passingSpinFactor` table — see
[AFTERTOUCH.md](AFTERTOUCH.md) §4 — and only for human sides.

The receiver's behaviour is the other half of the mechanic: he **slows to 256 or 512**
to let the ball arrive ([MOVEMENT.md](MOVEMENT.md) §3), steps sideways into the ball's
path if the pass is off-target, and is excluded from player selection until he has the
ball ([PASSING.md](PASSING.md) §6–§8).

---

## 5. Headers

Two entirely separate paths.

### Static header — `DoStaticHeader`, asm:38285

Feet on the ground, ball at chest height. The player's own aim point is set to his
position plus the directional offset, his speed is set to a flat **$100 (256)** —
very slow, a nudge — and he enters `PL_NORMAL2` for **20 frames** (asm:38302). No
attribute is consulted. This is the defensive nod, not a scoring header.

### Jumping header — `PlayerHeading` then `PlayerDoingHeader`

`PlayerHeading` (asm:39688) starts the jump: `PL_HEADING`, a **50-frame** timer, the
jump animation table, and `jumpHeaderSpeed` 2048 toward the facing octant. The player
is committed for a full second.

`PlayerDoingHeader` (asm:38018) fires on contact and is the more interesting routine:

1. Ball speed is set to the player's speed plus a quarter of it — `speed × 5/4`
   (asm:38027). The header inherits the jumper's momentum.
2. Rise is set to `ballKickingDeltaZ_2` = **$A000**, exactly *half* the kick's
   $14000. Headers stay lower than kicks.
3. **Steering.** The difference between the player's facing octant and the joystick
   octant selects one of several lateral adjustments (asm:38036–38095). Difference 0
   heads straight on; ±1 nudges the aim one octant either way; ±2 and ±3 route
   through helpers `sub_11280A` and `sub_112822` which alter `deltaZ` and cut speed
   before also nudging the octant. With no joystick input at all, the ball goes
   straight on with `sub_11280A` applied.
4. `playerStrongHeaderSpeedIncrease[Heading]` is added (asm:38106).
5. The player's own speed is halved (asm:38111) — he lands and stops.

The Heading bonus:

| Heading | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
|---|---|---|---|---|---|---|---|---|
| Bonus | −336 | −288 | −240 | −192 | −144 | −96 | −48 | 0 |

This table is **entirely non-positive**: Heading 7 gets no bonus, and every lesser
value is a penalty. It is a handicap ramp, not a reward ramp, and it is applied to a
speed that was already derived from the jumper's momentum rather than from a flat
base. A Heading-0 player loses 336 units — with a jump speed of 2048 × 5/4 = 2560,
that is a 13 % cut.

---

## 6. Constants quick reference

| Symbol | Line | Value | Meaning |
|---|---|---|---|
| `ballKickingSpeed` | 30730 | 2208 | Flat kick launch speed |
| `ballKickingDeltaZ` | 30729 | $14000 | Flat kick rise, 1.25 px/frame |
| `ballKickingDeltaZ_2` | 30587 | $A000 | Header rise, half a kick's |
| `ballSpeedKicking` | 34836 | −384 … +384 | Velocity bonus, long shots |
| `ballSpeedFinishing` | 34844 | −288 … +608 | Finishing bonus, close range |
| `playerStrongHeaderSpeedIncrease` | 34852 | −336 … 0 | Heading handicap |
| `jumpHeaderSpeed` | 30707 | 2048 | Jump launch |
| Static header speed | 38300 | $100 (256) | |
| Static header duration | 38303 | 20 frames | |
| Jump header duration | 39694 | 50 frames | |
| Header momentum multiplier | 38027 | × 5/4 | |
| Shot-on-goal Y gate | 35063, 35077 | 342 / 556 | Right / left team |
| Finishing zone X | 35085 | 241 … 431 | |
| Finishing zone Y | 35089 | < 204 or ≥ 694 | |
| Pass target arc | 39883 | ±16 of 256 | ±22.5°, measured from the ball |
| Pass base speed | 34680–34691 | $600 … $8AA | By **distance** to the receiver |
| Pass power bonus | 34701 | 0 … +384 | By Passing; see [PASSING.md](PASSING.md) §3 |
| `defaultPlayerDestinations` | 36496 | ±1000 | Directional offset |

---

## 7. What this resolves, and what still needs measurement

Confirmed:

- ✓ Launch speed and rise are flat constants; attributes enter only as bonuses.
- ✓ The shot-on-goal test is pure geometry, both thresholds known.
- ✓ Velocity governs long shots, Finishing governs close-range shots, and the
  boundary between them is a 190 × 75 pixel box in front of each goal.
- ✓ Both bonus tables, exactly.
- ✓ Finishing has a wider spread and an upward skew relative to Velocity.
- ✓ Pass targeting is a ±22.5° cone over own players, nearest wins — anchored at the
  ball, not the passer ([PASSING.md](PASSING.md) §1).
- ✓ Two distinct header mechanics with different rises, durations and bonuses.
- ✓ The Heading table is a handicap ramp with no positive term.
- ✓ Headers rise exactly half as steeply as kicks.

Open (measurement targets, [../LEGACY.md](../LEGACY.md) §15):

- What `sub_11280A` and `sub_112822` (asm:38230, asm:38248) do to `deltaZ` in detail.
  They are the header's equivalent of the lob/drive switch and deserve their own
  pass.
- Whether the tap/hold threshold that separates `DoPass` from `PlayerKickingBall` is
  a fixed frame count. `quickFire` (+$30) and `normalFire` (+$31) are the two flags;
  the threshold between them was not traced.

**Corrected since the first pass of this document:** the pass launch ramp is banded
by **distance to the receiver**, not by hold duration; and **Passing is read in-match**
— twice, as a power bonus and as the CPU accuracy threshold. Both are worked out in
[PASSING.md](PASSING.md) §3–§4.

---

## 8. Guidance for the reimplementation

- **Model the launch as constant + bonus**, not as a computed function of attributes.
  Every table here is an offset, and the composability is what makes the tuning
  legible.
- **Get the shot-on-goal geometry exactly right before tuning anything else.** It
  decides which of two very differently-shaped bonus tables applies, and a
  five-pixel error in the box boundary changes which attribute matters for a whole
  class of shots.
- **Do not "fix" the Heading table.** A ramp that tops out at zero looks like a bug
  and is not one: headers derive their pace from the jump, and the table exists to
  penalise poor headers rather than reward good ones.
- **Keep the pass cone and the receiver slow-down together.** Either alone feels
  broken; the pair is the mechanic.
- **Resolve the Passing-attribute question early.** If it really is unread in-match,
  our engine should say so explicitly rather than inventing a use for it.
