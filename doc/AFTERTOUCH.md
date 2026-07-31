# AFTERTOUCH.md

The mechanic the project is named after, traced end to end through the reference
DOS port in [reference/swos-port/](../reference/swos-port/), cross-checked against the
manual and community notes collected in [LEGACY.md](LEGACY.md) §3.

Aftertouch is what turns eight-direction digital input into continuous control
over a ball already in flight: bend it left/right, drive it low, or loft it. This
document explains **exactly what the reference code does each tick**, what state it
reads and writes, and which numbers are known versus still to be measured.

> **Provenance.** The reference tree is a decompiled/ported copy of a copyrighted
> binary; the aftertouch routine survives as translated 68000/x86, not clean C.
> The *structure and control flow* below are read directly from that code and are
> reliable. The *numeric constants* live in the original data segment (raw
> addresses, values not in the source) — treat them as fitting targets for the
> trace harness, exactly as [LEGACY.md](LEGACY.md) §15 prescribes. Read this to
> understand the design; write our own code.

---

## 0. One-paragraph version

When a human-controlled player kicks or passes, the engine opens a **10-tick
window** (`spinTimer` 0→9) on that player's team. Every tick inside the window,
[applyBallAfterTouch](../reference/swos-port/src/game/ball/ball.cpp#L2248) reads the
joystick direction, compares it to the direction the ball was launched, and:
(a) **latches a curl side** (left/right) the first time you push off-axis, then
nudges the ball's **destination point** sideways by
`SpinFactor[dir][side] × Multiplier[spinTimer]` every remaining tick — a curve
that is stronger the earlier you start; and (b) at exactly **tick 4**, sets the
ball's **vertical launch** (`deltaZ`) and speed to low-drive or high-lob depending
on whether you're pushing across or back against the kick. Shots and passes run
the same routine with different factor tables. AI players feed the same fields as
a synthetic joystick — there is no privileged AI path.

---

## 1. Where it lives and when it runs

- Definition: [applyBallAfterTouch()](../reference/swos-port/src/game/ball/ball.cpp#L2248)
  in [ball.cpp](../reference/swos-port/src/game/ball/ball.cpp).
- Called **once per team, every tick**, from the per-team update in
  [updatePlayers.cpp:198](../reference/swos-port/src/game/updatePlayers/updatePlayers.cpp#L198)
  (`l_apply_after_touch_and_set_ball_location_flags`). `A6` = the team's
  `TeamGeneralInfo`; the routine mutates the shared `ballSprite`.

Because it runs inside the per-team simulation update (not the renderer),
aftertouch is part of the deterministic tick — which is what makes SWOS replays
(state, not video) and headless simulation possible ([LEGACY.md](LEGACY.md) §8, §12).

---

## 2. State model

All aftertouch state lives on `TeamGeneralInfo`
([swos.h:338-399](../reference/swos-port/src/swos/swos.h#L338-L399)) plus the global
`ballSprite`:

| Field | Offset | Role |
|---|---|---|
| `spinTimer` | +118 | Window cursor. `-1` = inactive; `0..9` = ticks since kick; hits `10` → reset to `-1`. |
| `leftSpin` | +120 | Latched: curl is bending left. |
| `rightSpin` | +122 | Latched: curl is bending right. |
| `longPass` | +124 | Pass loft flag (pass path only). |
| `longSpinPass` | +126 | Long + spun pass flag. |
| `passInProgress` | +128 | 0 = this launch is a **shot**, 1 = a **pass**. Selects the branch. |
| `currentAllowedDirection` | +44 | Direction the ball was **launched** (the kick octant). `<0` disables spin. |
| `allowedPlDirection` | +56 | Current **joystick** direction of the controlled player (the aftertouch input). Commented `controlledPlDirection` in the asm. |

Ball fields written ([Sprite.h](../reference/swos-port/src/sprites/Sprite.h)):
`ballSprite.destX/destY` (curl — the aim point), `ballSprite.deltaZ` (vertical
launch velocity), `ballSprite.speed`, and `ballSprite.direction` (used to index
the pass table).

**Key implementation insight:** curl is *not* applied to the ball's velocity
directly. It is applied by **shifting the ball's destination coordinates sideways a
little every tick**. The ball always moves from its position toward
`(destX, destY)`; walking that target point laterally across the window bends the
path. Height, by contrast, *is* a direct velocity write (`deltaZ`).

---

## 3. Lifecycle of the window

**Open.** A human kick/pass sets `spinTimer = 0` on the kicking team:

- Shot: [player.cpp:1130](../reference/swos-port/src/game/player.cpp#L1130), then
  `passInProgress = 0` ([:1134](../reference/swos-port/src/game/player.cpp#L1134)).
- Pass: [player.cpp:3076](../reference/swos-port/src/game/player.cpp#L3076) (guarded by
  `playerNumber`, i.e. human-controlled), then `passInProgress = 1`
  ([:3080](../reference/swos-port/src/game/player.cpp#L3080)).

**Advance.** Each tick the routine runs (§4/§5) and ends by incrementing
`spinTimer` ([ball.cpp:2635-2655](../reference/swos-port/src/game/ball/ball.cpp#L2635-L2655)).

**Close.** When `spinTimer` reaches **10**, it is reset to `-1`
([ball.cpp:2657-2659](../reference/swos-port/src/game/ball/ball.cpp#L2657-L2659)) and
aftertouch stops. So the window is **10 ticks** (`spinTimer` 0..9) — at the Amiga
50 Hz tick, ~0.2 s. When `spinTimer == 0` the latched `leftSpin/rightSpin` are
cleared, re-arming for a fresh curve
([ball.cpp:2302-2306](../reference/swos-port/src/game/ball/ball.cpp#L2302-L2306)).

**Aborted.** [resetBothTeamSpinTimers()](../reference/swos-port/src/game/ball/ball.cpp#L4022)
sets both teams' `spinTimer = -1`. It is called on every event that ends the
current ball ownership/flight (tackles, out-of-play, new possession, goal, etc.) —
dozens of call sites across [ball.cpp](../reference/swos-port/src/game/ball/ball.cpp)
and [player.cpp](../reference/swos-port/src/game/player.cpp).

Two guards suppress spin even when the timer is live
([ball.cpp:2261-2290](../reference/swos-port/src/game/ball/ball.cpp#L2261-L2290)):
the opponent keeper is mid-save-comment, or the game is not `ST_GAME_IN_PROGRESS`
(and specifically while `ST_KEEPER_HOLDS_BALL`).

---

## 4. Lateral curl (applied every tick in the window)

This is the bend. Per tick, once a side is latched:

**Latch the side** (first off-axis push only —
[ball.cpp:2308-2364](../reference/swos-port/src/game/ball/ball.cpp#L2308-L2364)):

```
if (leftSpin)  side = LEFT            // already committed this window
elif (rightSpin) side = RIGHT
elif (currentAllowedDirection >= 0) {
    diff = (joystickDir - kickDir) & 7      // 0..7 rotational offset
    if (diff == 0 || diff == 4) → no curl this window   // aligned or opposite
    elif (diff < 4)  { leftSpin = 1;  side = LEFT  }     // diff ∈ {1,2,3}
    else             { rightSpin = 1; side = RIGHT }     // diff ∈ {5,6,7}
}
```

Once latched, the side sticks for the rest of the window even if you re-centre —
you commit to a curl direction by your first nudge.

**Apply the nudge** ([ball.cpp:2379-2440](../reference/swos-port/src/game/ball/ball.cpp#L2379-L2440)):

```
i    = kickDir * 8 + (side == RIGHT ? 4 : 0)   // index into the factor table
m    = kSpinMultiplierFactor[spinTimer]        // per-tick decay weight
ballSprite.destX += kKickSpinFactor[i].x * m
ballSprite.destY += kKickSpinFactor[i].y * m
```

`kKickSpinFactor` holds a `(dx, dy)` lateral vector per **kick direction × side**;
`kSpinMultiplierFactor` scales it by how far into the window we are. Because the
nudge is added *every* tick a side is latched, an early latch accumulates more
ticks *and* hits the higher-weighted early entries — this is the mechanism behind
the manual's *"the sooner you move after the kick, the stronger the effect"*
([LEGACY.md](LEGACY.md) §3).

---

## 5. Vertical launch: low drive vs high lob (applied once, at tick 4)

Height is decided a single time, when `spinTimer == 4`
([ball.cpp:2452-2540](../reference/swos-port/src/game/ball/ball.cpp#L2452-L2540)):

```
diff = (joystickDir - kickDir) & 7
switch (diff) {
    case 2: case 6:  deltaZ = kNormalKickDeltaZ; speed = kNormalKickBallSpeed;  // perpendicular → low drive
    case 3: case 4: case 5:
                     deltaZ = kHighKickDeltaZ;   speed = kHighKickBallSpeed;    // pushing back → high / lob
    default: /* 0,1,7 */  no vertical change                                    // aligned → stays as launched
}
```

Then a **speed trim by facing**
([ball.cpp:2545-2633](../reference/swos-port/src/game/ball/ball.cpp#L2545-L2633)):

| Joystick dir at tick 4 | Speed adjustment |
|---|---|
| 0 or 4 (straight up/down) | `speed -= speed/4` |
| odd (diagonal) | `speed += -speed/4 + speed/8` (net ≈ −1/8) |
| 2 or 6 (straight across) | unchanged |

So `diff == 4` (pushing directly opposite the kick) gives a **pure lob**: no curl
(§4 excludes diff 0/4) but maximum height. Pushing across (diff 2/6) gives a
**low, curled drive**. This is the same up/down-and-sideways model the manual
describes, expressed relative to the launch direction rather than the screen.

---

## 6. Passes vs shots

`passInProgress` selects the branch at the top of the routine
([ball.cpp:2255](../reference/swos-port/src/game/ball/ball.cpp#L2255)). The pass path
(`l_passing_now`, [ball.cpp:2664+](../reference/swos-port/src/game/ball/ball.cpp#L2664))
is structurally identical but differs in three ways:

| | Shot path | Pass path |
|---|---|---|
| Lateral factor table | `kKickSpinFactor` (325806) | `kPassingSpinFactor` (325870) |
| Indexed by | `allowedPlDirection` (player facing) | `ballSprite.direction` (ball's travel dir) — [ball.cpp:2790](../reference/swos-port/src/game/ball/ball.cpp#L2790) |
| Loft control | `deltaZ` swap at tick 4 (§5) | `longPass` / `longSpinPass` flags gate the lob — [ball.cpp:2713-2716,2854](../reference/swos-port/src/game/ball/ball.cpp#L2713-L2716) |

Both paths share `kSpinMultiplierFactor` for the decay curve. This confirms the
manual's note that **aftertouch applies to passes as well as shots**
([LEGACY.md](LEGACY.md) §3).

---

## 7. The data tables

The constants sit contiguously in the original data segment (addresses from the
asm operands; **values are not in the source**):

| Symbol | Address | Size | Meaning |
|---|---|---|---|
| `kHighKickDeltaZ` | 325774 | dword | Upward launch velocity for a lofted kick |
| `kHighKickBallSpeed` | 325778 | word | Ball speed for a lofted kick |
| `kNormalKickDeltaZ` | 325780 | dword | Upward velocity for a low drive |
| `kNormalKickBallSpeed` | 325784 | word | Ball speed for a low drive |
| `kSpinMultiplierFactor` | 325786 | 10 × word | **Decay curve**: per-tick weight, indexed by `spinTimer` 0..9 |
| `kKickSpinFactor` | 325806 | 8 dir × 2 side × (word x, word y) | **Shot curl vectors** |
| `kPassingSpinFactor` | 325870 | 8 dir × 2 side × (word x, word y) | **Pass curl vectors** |

The layout is tight and self-consistent: `325786 + 10×2 = 325806`, and
`325806 + 8×8 = 325870`. Each factor table is 64 bytes = 8 directions, each with a
left `(x,y)` and right `(x,y)` 16-bit vector.

**What this resolves from [LEGACY.md](LEGACY.md) §15's "unknown" list** (as
*structure*; the numbers still want trace confirmation):

- Aftertouch **window length**: 10 ticks. ✓
- **Effect-strength decay** vs elapsed ticks: `kSpinMultiplierFactor[spinTimer]`. ✓
- **Lateral acceleration** for curl: `kKickSpinFactor` / `kPassingSpinFactor`,
  applied as a per-tick destination nudge. ✓
- **Vertical magnitude** for low/high/lob: `kNormal/HighKickDeltaZ` swapped at
  tick 4. ✓
- **Whether it differs between passes and shots**: yes — different tables and a
  different indexing direction. ✓

---

## 8. AI aftertouch — the virtual joystick

There is **no privileged AI trajectory code**. CPU-controlled shots/passes flow
through the same `applyBallAfterTouch` reading the same `allowedPlDirection` /
`currentAllowedDirection` fields. The AI's only extra machinery is choosing *how
hard* to aim: `AI_afterTouchStrength` (weak/medium/strong = 0/1/2) is decided in
[updatePlayers.cpp:18200-18680](../reference/swos-port/src/game/updatePlayers/updatePlayers.cpp#L18200-L18680)
(`l_decide_after_touch_strength`, `l_weak/medium/strong_after_touch`), then
expressed by feeding the corresponding joystick direction into the same fields a
human stick would drive. This is the concrete confirmation of
[LEGACY.md](LEGACY.md) §8's central structural finding: *model every AI player as
emitting the same `(direction, fire_state)` a human joystick would.*

---

## 9. Reconciliation and open questions

**Agrees with the manual** ([LEGACY.md](LEGACY.md) §3): move after the kick to
bend/lob/drive; sooner = stronger; opposite-to-kick = lob; applies to passes.

**Sharper than the manual:** the manual frames height as literal "up = low,
down = high" for the top goal. The code decides height from the *rotational offset*
`diff = (joystick − kick) & 7`, sampled once at tick 4 — which reduces to the
manual's rule when shooting straight at a goal, but generalizes to any launch
direction and any diagonal.

**Still to measure on the reference build** (this is a *port*; verify against
original traces per [LEGACY.md](LEGACY.md) §15's method):

- The actual values in the seven tables in §7 (especially the `kSpinMultiplierFactor`
  decay shape and the per-direction curl magnitudes).
- Whether tick 4 is the exact sampling point on the Amiga build, or a port artifact.
- Interaction of the destination-nudge curl with rolling friction / pitch type
  (a curl on Frozen should travel further before the target is reached).
- Whether `longSpinPass` composes curl + loft additively or caps one of them.

---

## 10. Guidance for the aftertouch reimplementation

- **Put it in `at_core`, inside the deterministic tick**, driven by a per-player
  `(direction, fire_state)` input vector — never from a render-time or
  wall-clock source. Determinism (replays, headless leagues) depends on it.
- **Model curl as a destination-point nudge, not a force**, if you want
  bit-level parity with the reference. A lateral-acceleration model *feels*
  similar but will diverge from reference traces after a few dozen ticks.
- **Keep the two-part split**: continuous lateral curl every tick (side latched
  on first off-axis input, weighted by a decay table) + a one-shot vertical launch
  decision a few ticks in. They are separate systems; don't collapse them.
- **One code path for human and AI.** The AI only chooses an input; the physics is
  shared. Any aftertouch behavior you cannot reproduce by feeding a synthetic
  joystick is a bug ([LEGACY.md](LEGACY.md) §8).
- **Preserve the fine launch direction.** Aftertouch keys off the octant, but the
  ball's launch angle and the curl vectors are finer; keep the 256-step direction
  from [updateSprite.cpp](../reference/swos-port/src/sprites/updateSprite.cpp#L101-L112)
  (see [PLAYER_SPRITES.md](PLAYER_SPRITES.md) §5) available to the ball model.
- **Fit the seven tables from traces**, do not hand-tune by feel — this mechanic
  is precisely the "feels wrong" surface [LEGACY.md](LEGACY.md) §17 warns about.
