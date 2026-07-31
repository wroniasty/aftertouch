# CONTROL.md

How ball control works in SWOS: the ball is a **separate physics entity**, never
glued to a player's feet. "Having the ball" is a matter of **proximity** plus a
per-tick **aim-ahead** that pushes the ball in front of a running dribbler, with
the **Control** attribute deciding how tightly it stays. Traced through the
reference DOS port in [../reference/swos-port/](../reference/swos-port/),
cross-checked against [LEGACY.md](LEGACY.md) §5 (ball physics) and §9 (attributes).
Companion to [SHOOTING.md](SHOOTING.md) (leaving the foot) and
[AFTERTOUCH.md](AFTERTOUCH.md) (in-flight steering).

> **Provenance.** Decompiled 68000/x86; control flow is reliable, numeric
> constants live in the original data segment (raw addresses, values not in
> source) and are trace-fitting targets per [LEGACY.md](LEGACY.md) §15. Read to
> understand the design; write our own code.

---

## 0. One-paragraph version

Every tick, each player's **distance to the ball** is bucketed into named bands
(`≤4`, `4–8`, … `>17`). Those bands gate everything: only a player who is
`plVeryCloseToBall`/`plCloseToBall` may pass or shoot, and only one within the
capture band takes possession. A player *with* the ball doesn't hold it — the
dribble routine sets the **ball's own destination a fixed offset ahead** of the
player in his facing direction (`kDefaultDestinations[dir]`) and trims the ball's
speed by his **Control** rating (`kBallSpeedDeltaWhenControlled[Control]`). The
ball chases that moving aim point at its own speed, so running fast makes it run
ahead — the manual's *"the ball is not attached to the player's feet."* After a
kick, a **lockout timer** (`passKickTimer`) keeps the ball uncontrollable briefly,
which is what lets a struck ball pass through team-mates.

---

## 1. The ball is its own entity

`ballSprite` is a full `Sprite` with position, `speed`, `deltaZ` (height), and a
destination `(destX, destY)` it travels toward — the same aim point that
[SHOOTING.md](SHOOTING.md) launches and [AFTERTOUCH.md](AFTERTOUCH.md) curls. No
player owns it. "Possession" is an emergent state: a player is close, and his
dribble routine keeps re-aiming the ball ahead of himself. This is the concrete
form of [LEGACY.md](LEGACY.md) §5: *the ball has its own 3D velocity and height;
players also have height.*

---

## 2. Proximity model — the capture/act bands

Each tick the controlled player's distance to the ball (`Sprite.ballDistance`,
+74) is classified into a set of per-team byte flags on `TeamGeneralInfo`
([swos.h:358-367](../reference/swos-port/src/swos/swos.h#L358-L367)):

| Field | Offset | Meaning |
|---|---|---|
| `plVeryCloseToBall` | +61 | Player is on the ball (act/control range) |
| `plCloseToBall` | +62 | Player is close (can still pass/kick) |
| `plNotFarFromBall` | +63 | Near |
| `ballLessEqual4` | +64 | Ball within ~4 units (kick capture band) |
| `ball4To8` | +65 | Ball 4–8 units away |
| `ball8To12` | +66 | 8–12 |
| `ball12To17` | +67 | 12–17 |
| `ballAbove17` | +68 | > 17 units |
| `prevPlVeryCloseToBall` | +69 | Previous tick's on-ball flag (edge detect) |

These bands are the game's **capture radius**, expressed as thresholds rather than
a single number ([LEGACY.md](LEGACY.md) §5). They gate the on-ball actions
([updatePlayers.cpp:5069-5344](../reference/swos-port/src/game/updatePlayers/updatePlayers.cpp#L5069-L5344)):

- **Pass** (`quickFire`) requires `plVeryCloseToBall || plCloseToBall`.
- **Kick/shot** (`normalFire`) requires the same, and the kick itself keys off
  `ballLessEqual4` ([updatePlayers.cpp:5324](../reference/swos-port/src/game/updatePlayers/updatePlayers.cpp#L5324)).

Distances are in internal pitch units; confirm the coordinate scale before reading
"4" as pixels ([LEGACY.md](LEGACY.md) §15).

---

## 3. Dribbling — aim the ball ahead, tighten it by Control

The dribble routine ([player.cpp:520-640](../reference/swos-port/src/game/player.cpp#L520-L640))
runs for the on-ball player and does two things per tick:

**a. Re-aim the ball ahead of the player.** Using the player's facing direction,
it sets the **ball's** destination to the player's position plus a per-direction
offset from `kDefaultDestinations` (523294, indexed `dir × 4` → `(dx, dy)`):

```
ballSprite.destX = playerX + kDefaultDestinations[dir].x
ballSprite.destY = playerY + kDefaultDestinations[dir].y
```

([player.cpp:546-584](../reference/swos-port/src/game/player.cpp#L546-L584); a ±1
positional nudge orients the ball first,
[:520-544](../reference/swos-port/src/game/player.cpp#L520-L544).) The ball is
pushed *in front of* the runner — the **dribble kick distance** of
[LEGACY.md](LEGACY.md) §5.

**b. Trim ball speed by Control.** On alternate ticks (`test currentTick, 2`) it
looks up `kBallSpeedDeltaWhenControlled[ballControl]` (523904, indexed by the
player's **Control** attribute, `PlayerGameHeader.ballControl` +73) and folds it
into the ball's speed ([player.cpp:594-616](../reference/swos-port/src/game/player.cpp#L588-L616)):

```
if (currentTick & 2)
    delta = kBallSpeedDeltaWhenControlled[ballControl];
ballSprite.speed = adjust(ballSprite.speed, delta);   // higher Control → tighter
```

Because the ball moves toward an aim point *ahead* of the player at its own speed,
a faster run lets the ball drift further out before you catch it again. **Control
is the mechanical meaning of "the ball is not glued to his feet"**
([LEGACY.md](LEGACY.md) §5): it damps how far the ball gets away each cycle.

---

## 4. Losing and regaining control

`TeamGeneralInfo` carries the control-state fields
([swos.h:386-387](../reference/swos-port/src/swos/swos.h#L386-L387)):

| Field | Offset | Meaning |
|---|---|---|
| `ballCanBeControlled` | +110 | Whether the ball is currently capturable |
| `ballControllingPlayerDirection` | +112 | Facing dir of the controlling player |
| `passKickTimer` | +102 | Post-kick lockout countdown |

On a pass/kick the dispatch sets `passKickTimer = 25` and `ballCanBeControlled = 0`
([updatePlayers.cpp:5230-5231](../reference/swos-port/src/game/updatePlayers/updatePlayers.cpp#L5230-L5231)).
That lockout is why a struck ball briefly **passes through team-mates** instead of
being instantly re-collected — the manual's *"keep holding fire after the kick and
the ball passes through your own players"* ([LEGACY.md](LEGACY.md) §3) and the
reason the kicker can't recapture on the same tick. When the timer expires the
capture bands (§2) resume governing possession.

Winning the ball from an opponent (running tackle, slide) resolves through the
tackle system — outcome influenced by the tackler's **Tackling** vs the carrier's
**Control** ([LEGACY.md](LEGACY.md) §3, §15) — and, like every state change that
ends a possession, calls [resetBothTeamSpinTimers()](../reference/swos-port/src/game/ball/ball.cpp#L4022)
to cancel any live aftertouch. The tackle resolution itself is an
[LEGACY.md](LEGACY.md) §15 measurement target and is not detailed here.

---

## 5. Data tables

Original data segment (addresses from asm; **values not in source**):

| Symbol | Address | Indexed by | Meaning |
|---|---|---|---|
| `kDefaultDestinations` | 523294 | direction (0–7) | `(dx, dy)` offset placing the ball ahead while dribbling |
| `kBallSpeedDeltaWhenControlled` | 523904 | **Control** (`ballControl` +73) | Per-tick ball-speed trim while dribbling |

Related launch/attribute tables are in [SHOOTING.md](SHOOTING.md) §6.
`PlayerGameHeader.ballControl` is at +73 (Control); the same struct holds
`shooting`/Velocity +70 and `finishing` +75.

---

## 6. What this resolves, and what still needs measurement

**Confirmed as structure** ([LEGACY.md](LEGACY.md) §5):

- The ball is an independent entity; possession is proximity + aim-ahead, not
  attachment. ✓
- **Capture radius** exists as distance bands (`≤4 … >17`), gating pass/kick/
  control. ✓
- **Dribble kick distance**: `kDefaultDestinations[dir]` placed ahead of the
  runner. ✓
- **Control modifies the dribble** via `kBallSpeedDeltaWhenControlled[Control]`,
  applied every other tick. ✓
- A post-kick lockout (`passKickTimer`, `ballCanBeControlled`) implements
  pass-through-team-mates. ✓

**Open (measurement targets, [LEGACY.md](LEGACY.md) §15):**

- The pitch coordinate scale, so the bands and offsets read in real units.
- The values in `kDefaultDestinations` and `kBallSpeedDeltaWhenControlled`, and
  whether the Control curve is linear.
- Exact `ballDistance` metric (Euclidean vs Chebyshev/Manhattan) and the band
  boundaries.
- The `passKickTimer = 25` duration on the reference build, and how it interacts
  with the aftertouch window.
- Running-tackle / slide contest resolution (inputs, weights, randomness).

---

## 7. Guidance for the reimplementation

- **Make the ball a first-class entity** with its own position/velocity/height and
  a destination it moves toward. Possession is derived, never a parent pointer.
- **Model dribbling as re-aiming the ball ahead each tick** and damping its speed
  by Control — not as pinning it to a foot offset. This is what reproduces the
  loose-at-speed feel.
- **Bucket ball distance into bands** and drive all on-ball permissions from them;
  keep the band thresholds as data you can fit.
- **Keep the post-kick lockout.** Without `passKickTimer`, struck balls get
  instantly recaptured and pass-through-team-mates disappears.
- **One deterministic tick.** Distance classification, dribble aim, and control
  trim all live in `at_core`, driven by per-player input — required for replays
  and headless leagues ([LEGACY.md](LEGACY.md) §8, §12).
- **Fit the tables from traces; tune nothing by feel** ([LEGACY.md](LEGACY.md) §17).
