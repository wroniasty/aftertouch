# SHOOTING.md

How a kick becomes a moving ball in SWOS: the pass-vs-shot decision, launch speed
(power), the vertical launch height *before* any aftertouch, and the
attribute-driven bonus for shots on goal. Traced through the reference DOS port in
[../reference/swos-port/](../reference/swos-port/), cross-checked against
[LEGACY.md](LEGACY.md) §3 (controls) and §9 (attributes). Aftertouch — the
post-kick bending/lofting — is a separate document, [AFTERTOUCH.md](AFTERTOUCH.md);
this one is everything up to the instant the ball leaves the foot.

> **Provenance.** The kick routines survive as decompiled 68000/x86; the control
> flow below is read straight from that code and is reliable. Numeric constants
> live in the original data segment (raw addresses; values not in the source) —
> treat them as fitting targets for the trace harness per [LEGACY.md](LEGACY.md)
> §15. Read to understand the design; write our own code.

---

## 0. One-paragraph version

A **tap** of fire is a **pass** (`quickFire`); a **hold** is a **kick/shot**
(`normalFire`) — the manual's one-button model. When the controlled player is
close enough to the ball, [playerKickingBall()](../reference/swos-port/src/game/player.cpp#L750)
fires: it aims the ball a fixed offset in the player's facing octant, sets a
**constant base launch speed** (`kBallKickingSpeed`) and a **constant base upward
velocity** (`kBallKickingDeltaZ`), and re-arms the aftertouch window. Then a
**shot-on-goal classifier** checks the ball's pitch position: inside the box it
adds a **Finishing**-scaled speed bonus, outside it adds a **Velocity**-scaled
one — exactly the manual's split. Height then follows a simple ballistic arc under
gravity, unless aftertouch overrides it a few ticks later.

---

## 1. The input model: tap = pass, hold = kick

One button, eight directions ([LEGACY.md](LEGACY.md) §3). The engine reduces the
fire button to three per-team flags on `TeamGeneralInfo`
([swos.h:350-353](../reference/swos-port/src/swos/swos.h#L350-L353)):

| Flag | Offset | Meaning |
|---|---|---|
| `quickFire` | +48 | A quick **tap** was registered → **pass** |
| `normalFire` | +49 | A sustained **hold** was registered → **kick / shot** |
| `firePressed` | +50 | Fire is physically down this frame |

The on-ball dispatch ([updatePlayers.cpp:5067-5344](../reference/swos-port/src/game/updatePlayers/updatePlayers.cpp#L5067-L5344)):

```
if (quickFire && haveDirection && (plVeryCloseToBall || plCloseToBall))
        → passing path            // see CONTROL.md for proximity bands
elif (normalFire && haveDirection && (plVeryCloseToBall || plCloseToBall))
        → playerKickingBall()     // the shot/kick, §2
```

Both require the player to actually be near the ball; possession is not "glued"
(see [CONTROL.md](CONTROL.md)). The **hold-duration threshold** that separates a
tap from a hold is applied upstream in the input classifier that sets these flags;
the community estimate is **~4 frames** ([LEGACY.md](LEGACY.md) §8) and is a
value to confirm by trace, not a constant visible here. AI players set the very
same `quickFire`/`normalFire` flags
([updatePlayers.cpp:18125,18229](../reference/swos-port/src/game/updatePlayers/updatePlayers.cpp#L18125))
— the virtual-joystick model, no privileged shot path
([LEGACY.md](LEGACY.md) §8).

---

## 2. The base kick — [playerKickingBall()](../reference/swos-port/src/game/player.cpp#L750)

Inputs: `A1` = player sprite, `A6` = team. Steps, in order
([player.cpp:750-805](../reference/swos-port/src/game/player.cpp#L750-L805)):

1. **Record kick direction.** The player's facing octant (`Sprite.direction`,
   0–7) is copied to `TeamGeneralInfo.controlledPlDirection` (+56) — the reference
   that aftertouch later measures against.

2. **Aim the ball.** [getBallDestCoordinatesTable()](../reference/swos-port/src/game/ball/ball.cpp#L4051)
   returns a per-**game-state** table of `(dx, dy)` offsets, one per direction
   (even entries = Δx, odd = Δy). The ball's aim point is set from its current
   position plus the entry for the kick direction:
   `ballSprite.destX = ballX + table[dir].x`, `destY = ballY + table[dir].y`
   ([player.cpp:762-799](../reference/swos-port/src/game/player.cpp#L762-L799)).
   Different tables exist for open play vs each set-piece halt (throw-ins, etc.).

3. **Set launch speed (power).** `ballSprite.speed = kBallKickingSpeed` — a
   **fixed constant** (325772), *not* scaled by hold duration
   ([player.cpp:800-802](../reference/swos-port/src/game/player.cpp#L800-L802)).

4. **Set launch height (vertical, no aftertouch).**
   `ballSprite.deltaZ = kBallKickingDeltaZ` — a **fixed** upward velocity (325768,
   a dword) ([player.cpp:803-804](../reference/swos-port/src/game/player.cpp#L803-L804)).
   This is the ball's default rise; gravity (in the ball update) pulls it back
   down into a low arc. **This is the vertical trajectory when the player applies
   no aftertouch.** §4 covers how aftertouch can replace it.

5. **Re-arm spin.** [resetBothTeamSpinTimers()](../reference/swos-port/src/game/ball/ball.cpp#L4022)
   clears both teams, then the aftertouch window is opened (`spinTimer = 0`) at
   [player.cpp:1130](../reference/swos-port/src/game/player.cpp#L1130) — see
   [AFTERTOUCH.md](AFTERTOUCH.md) §3.

> **On "longer hold = harder kick"** ([LEGACY.md](LEGACY.md) §3, manual): the base
> kick speed in this port is a constant. Hold duration is what this code uses to
> choose **pass vs kick** (§1), not to scale base power. Whether the Amiga build
> additionally scaled launch speed or `deltaZ` with hold frames is **not settled
> by this reference** and is a measurement target — do not assume a charge curve
> without a trace.

---

## 3. Shot-on-goal bonus — Velocity outside, Finishing inside

After the base kick, a classifier decides whether the kick is a **shot on goal**
and, if so, adds an **attribute-scaled** speed bonus
([player.cpp:846-1073](../reference/swos-port/src/game/player.cpp#L846-L1073)).

**Is it a shot on goal?** A set of integer comparisons on the ball's pitch
`(x, y)` against goal-mouth thresholds (e.g. `x` vs 241/431, `y` vs 204/342/556/694)
combined with the kick direction pointing goalward
([player.cpp:858-977](../reference/swos-port/src/game/player.cpp#L858-L977)).
Mirrored for the top vs bottom team. Off-target kicks skip the bonus entirely.

**Which bonus?** By whether the ball is inside the penalty area:

| Zone | Bonus added to `ballSprite.speed` | Table | Indexed by |
|---|---|---|---|
| **Finishing** (inside the area) | `kBallSpeedFinishing[Finishing]` | 523888 | `PlayerGameHeader.finishing` (+75) |
| **Long shot** (outside the area) | `kBallSpeedKicking[Velocity]` | 523840 | `PlayerGameHeader.shooting` (+70) |

([player.cpp:1018-1046](../reference/swos-port/src/game/player.cpp#L1018-L1046) finishing;
[player.cpp:1049-1073](../reference/swos-port/src/game/player.cpp#L1049-L1073) long shot.)

This is the mechanical confirmation of [LEGACY.md](LEGACY.md) §9: **Velocity =
shot power from outside the box; Finishing = shots from inside**. Both are looked
up by the attribute value (a per-point table, so the modifier need not be linear)
and **added** to the constant base speed. So final shot power =
`kBallKickingSpeed + attributeTable[attr]`, with the table chosen by zone.

---

## 4. Vertical launch without aftertouch — and how aftertouch overrides it

Straight off the foot, the ball rises with `deltaZ = kBallKickingDeltaZ` (§2.4)
and descends under gravity: a shallow, mostly-grounded arc. That default holds
**unless** the player pushes the stick during the aftertouch window. At exactly
`spinTimer == 4`, [applyBallAfterTouch()](../reference/swos-port/src/game/ball/ball.cpp#L2452-L2540)
**overwrites** `deltaZ` and `speed` with the low-drive pair
(`kNormalKickDeltaZ` / `kNormalKickBallSpeed`) or the high-lob pair
(`kHighKickDeltaZ` / `kHighKickBallSpeed`) depending on the stick offset from the
kick direction. Full mechanism in [AFTERTOUCH.md](AFTERTOUCH.md) §5. So:

- **No aftertouch** → `kBallKickingDeltaZ`, a fixed low trajectory.
- **Aftertouch across the kick line** → low driven shot (`kNormalKickDeltaZ`).
- **Aftertouch back against the kick** → lofted shot/lob (`kHighKickDeltaZ`).

---

## 5. Passes

The pass path (`quickFire`) is the sibling of the kick: it launches the ball
toward the nearest team-mate in the facing cone ([LEGACY.md](LEGACY.md) §8),
adds `kBallSpeedPassingIncrease` (523806,
[player.cpp:3003](../reference/swos-port/src/game/player.cpp#L3003)) to speed, and
opens the same aftertouch window with `passInProgress = 1` so passes can be curled
and lofted too ([AFTERTOUCH.md](AFTERTOUCH.md) §6). Set-piece launches reuse
`getBallDestCoordinatesTable` with the state-specific offset table.

---

## 6. Data tables

Constants in the original data segment (addresses from asm operands; **values not
in source**). The launch constants sit just below the aftertouch block documented
in [AFTERTOUCH.md](AFTERTOUCH.md) §7:

| Symbol | Address | Size | Meaning |
|---|---|---|---|
| `kBallKickingDeltaZ` | 325768 | dword | Base upward launch velocity (no aftertouch) |
| `kBallKickingSpeed` | 325772 | word | Base kick launch speed |
| `kBallSpeedKicking` | 523840 | word[] | Long-shot speed bonus, indexed by **Velocity** |
| `kBallSpeedFinishing` | 523888 | word[] | In-box speed bonus, indexed by **Finishing** |
| `kBallSpeedPassingIncrease` | 523806 | word[] | Pass speed increase |
| `kBallSpeedDeltaWhenControlled` | 523904 | word[] | Dribble speed delta by **Control** (see [CONTROL.md](CONTROL.md)) |

`PlayerGameHeader` attribute byte offsets used here: `shooting`/Velocity +70,
`ballControl`/Control +73, `finishing` +75.

---

## 7. What this resolves, and what still needs measurement

**Confirmed as structure** (numbers still want traces):

- Tap = pass, hold = kick, via `quickFire`/`normalFire`. ✓
- Base launch speed and height are **constants**, not hold-scaled, in this port. ✓
- Shot power = base + attribute bonus; **Velocity outside**, **Finishing inside**
  the area ([LEGACY.md](LEGACY.md) §9). ✓
- Vertical launch without aftertouch = `kBallKickingDeltaZ` + gravity. ✓

**Open (measurement targets, [LEGACY.md](LEGACY.md) §15):**

- The tap/hold frame threshold (community ~4) and whether the human classifier
  scales anything with hold length.
- Whether the Amiga build scaled base power/height by hold duration at all.
- The actual values in every table above, and the **shape** of the attribute
  curves (linear vs not).
- The exact goal-zone thresholds vs pitch coordinate scale (they are in internal
  units; confirm the scale first — [LEGACY.md](LEGACY.md) §15 "pitch coordinate
  scale").
- The `getBallDestCoordinatesTable` offsets per game state.

---

## 8. Guidance for the reimplementation

- **Model the button as tap/hold → pass/kick**, both gated on ball proximity, both
  emitted identically by human and AI (one input path).
- **Keep base launch as a constant + attribute-table bonus**, zone-selected, if you
  want reference parity. Resist a continuous power bar until a trace demands it.
- **Split power from height.** Speed and `deltaZ` are independent writes; the base
  kick sets both, and aftertouch may later overwrite them at a fixed tick.
- **Zone the pitch for Velocity vs Finishing** from the start — the split is
  load-bearing for how attributes pay off ([LEGACY.md](LEGACY.md) §6 notes long
  shots matter more on Frozen).
- **Do the whole kick inside the deterministic `at_core` tick**, driven by the
  per-player `(direction, fire_state)` input, so replays and headless sim stay
  bit-exact.
- **Fit the tables from traces, tune nothing by feel** ([LEGACY.md](LEGACY.md) §17).
