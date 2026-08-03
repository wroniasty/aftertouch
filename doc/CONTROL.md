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

> **Second oracle.** [amiga/CONTEST.md](amiga/CONTEST.md) traces the same subsystem
> through the Amiga original (`CalculateIfPlayerWinsBall` asm:35144). It supplies
> **both data tables §5 lists as address-only**, gives the exact height-band
> boundaries, and adds a whole mechanic this document does not mention — the
> touch-count that ends a dribble. See §8. Most of §6's open list is now answered.

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

**Possession while charging a shot is ordinary possession.** Holding fire does
not freeze the dribble: our first implementation suppressed the aim-ahead for
the length of the hold so a charged shot could not be kicked off the player's
own foot, and the result was that the ball stayed put while the carrier ran on,
left the close band, and the queued strike arrived as a slide tackle
([B6a](implementation/B6a-kick-fidelity.md) §2 S4). The strike tick
short-circuits the dribble on its own, which is the whole guard that is needed.

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

**Resolved since writing** (see §8, and [MOVEMENT.md](MOVEMENT.md) §6):

- ~~The values in `kDefaultDestinations` and `kBallSpeedDeltaWhenControlled`.~~
  `±1000` per octant, and `130, 116, 102, 88, 74, 60, 46, 32` by Control — linear,
  step −14, and **inverted**: a low-Control player pushes the ball *further*.
- ~~Exact `ballDistance` metric and the band boundaries.~~ Squared Euclidean, never
  square-rooted ([MOVEMENT.md](MOVEMENT.md) §6). The `≤4 … >17` bands are the
  **ball's height** `z`, not distance, with pixel-exact boundaries at 4, 8, 12, 17.
- ~~Running-tackle / slide contest resolution.~~ Documented in
  [TACKLING.md](TACKLING.md) and confirmed with its full table in
  [amiga/CONTEST.md](amiga/CONTEST.md) §3.

**Open (measurement targets, [LEGACY.md](LEGACY.md) §15):**

- The pitch coordinate scale, so the bands and offsets read in real units. Narrowed:
  one `speed` unit is ~1/512 px/frame at 50 Hz ([MOVEMENT.md](MOVEMENT.md) §13).
- The `passKickTimer = 25` duration on the reference build, and how it interacts
  with the aftertouch window. The Amiga uses the same field as a *frames-since-kick*
  counter and gates shot resolution on it reaching **22**
  ([amiga/GOALKEEPER.md](amiga/GOALKEEPER.md) §4) — near enough to 25 to be worth
  checking whether they are the same constant read two ways.
- The three **planar** proximity thresholds. [MOVEMENT.md](MOVEMENT.md) §6 gives
  `≤ 32 / 72 / 2450` squared; the Amiga could not isolate them, so this is
  single-sourced and worth a trace.
- What marks a slide as a *deflecting* tackle rather than a possession attempt
  (`Sprite` +$6A = −1 on the Amiga). It changes both the contest and the recovery
  cost, and it is player-facing.

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
- **Implement the touch counter** (§8). Without it, Control only affects how far the
  ball drifts, and a Control-0 player dribbles indefinitely. The counter is what
  makes Control the most consequential attribute in the game.
- **Preserve the inverted sign** of the touch-impulse table. It reads as a bug in
  review and is not one.

---

## 8. Amiga cross-check

Traced independently through the Amiga original — [amiga/CONTEST.md](amiga/CONTEST.md).

### The two tables §5 could only name

```
ball.destX = player.x + defaultPlayerDestinations[dir].dx     ; ±1000
ball.destY = player.y + defaultPlayerDestinations[dir].dy
impulse    = (currentTick bit 1) ? ballSpeedDeltaWhenControlled[Control] : 0
ball.speed = player.speed + impulse
```

| Ball Control | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
|---|---|---|---|---|---|---|---|---|
| `ballSpeedDeltaWhenControlled` (asm:34783) | 130 | 116 | 102 | 88 | 74 | 60 | 46 | 32 |

Two corrections to §3's reading:

- The impulse is **added to the player's speed**, not folded into the ball's
  existing speed. Each touch *re-imposes* a speed rather than trimming one.
- The table is **inverted**. §3 says "higher Control → tighter", which is the right
  outcome, but the mechanism is that a *low*-Control player gets the *larger*
  impulse and so knocks the ball further ahead of himself. Read the table as "how
  badly you push it away", not "how well you keep it".

§3's `currentTick & 2` is confirmed as `btst #1, currentTick+1` (asm:35286) — the
impulse fires on **two frames in four**. On the other two the ball simply inherits
the player's speed and decelerates under friction, which is the visible bobble of a
SWOS dribble.

There is one term §3 does not have: a **direction-change bonus** of **+256**
(asm:35296) when the ball's fine heading has drifted more than a quarter-turn from
the player's octant — the extra push needed to bring the ball round on a turn.

### The mechanic this document is missing: losing the ball by touch count

Every touch taken with the player's octant differing from the stored kick direction
increments a counter (`unkBallTimer`, asm:35305). When it passes a Control-derived
threshold, possession is interrupted for 8 frames:

| Ball Control | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
|---|---|---|---|---|---|---|---|---|
| Touches allowed (asm:34791) | 4 | 5 | 6 | 8 | 11 | 14 | 17 | 21 |

The spacing accelerates sharply at the top — Control 7 changes direction more than
five times as often as Control 0 before losing the ball. §4 describes losing control
only through a tackle; this is the *other* way, it needs no opponent, and it is the
single clearest attribute effect in the game. It belongs in §4.

### The height bands, exactly

§2's `≤4 … >17` are bands of the ball's `z`, confirming
[MOVEMENT.md](MOVEMENT.md) §6's correction, with boundaries that are pixel-exact:
`≤ 4`, `5–8`, `9–12`, `13–17`, `> 17`. The top band is a **hard veto**: a ball
higher than 17 cannot be played by an outfielder at all (asm:42497). The crossbar
sits at 15–19 ([BALL.md](BALL.md) §12), so the playable ceiling is set just under
the bar — which is a deliberate piece of design, not a coincidence.

The Amiga also has a companion routine, `UpdateBallVariables` (asm:38711), that
publishes the ball position under three filters — `ballDefensive*` (keeper),
`ballNotHigh*` and `strikeDest*` — chosen by height band and by whether the ball is
rising or falling. **These, not the raw position, are what the AI chases.** That is
very likely the same thing as the unexplained `ballNextYGroundY`/`ballNextY` split
in [BALL.md](BALL.md) §10.

### Agreements worth recording

- Possession is proximity + re-aim, never attachment. ✓ (stated identically on both
  sides, and the Amiga document is emphatic that a parented ball "plays completely
  differently and cannot be tuned back")
- Exactly one proximity band is set per player per frame, by a descending comparison
  chain. ✓
- The bands gate *everything* downstream — no contest routine recomputes them. ✓
- Aftertouch is cancelled on every ball–player contact and every possession change.
  ✓ ([AFTERTOUCH.md](AFTERTOUCH.md) §3)
