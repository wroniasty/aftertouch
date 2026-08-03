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

> **Second oracle.** [amiga/AFTERTOUCH.md](amiga/AFTERTOUCH.md) traces
> `ApplyBallAfterTouch` (asm:40089) in the Amiga original. It calls this "the
> best-documented mechanic in the binary", and it supplies **all seven tables in
> §7 as literal values** — including the complete curl vectors and the decay ramp.
> It also contradicts this document in three places, two of which look like real
> errors here rather than build differences. Everything is in §11; **read that
> before implementing from §4–§6.**

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

**Resolved by the Amiga oracle** (see §11):

- ~~The actual values in the seven tables in §7.~~ All seven recovered as literals.
  The decay ramp is `5, 4, 3, 2, 2, 2, 2, 1, 1, 1` — neither linear nor exponential
  — and the curl magnitudes are only ever 0, 23 or 32.
- ~~Whether tick 4 is the exact sampling point on the Amiga build.~~ It is. The
  Amiga tests `spinTimer == 4` exactly (asm:40154), for kicks only.
- ~~Whether `longSpinPass` composes curl + loft additively.~~ There is **no pass
  loft**. The pass branch changes no `deltaZ` at all; the two flags at +124/+126 are
  a one-shot lockout on a `speed += speed/8` nudge. See §11.

**Still to measure** (verify against original traces per [LEGACY.md](LEGACY.md) §15):

- Interaction of the destination-nudge curl with rolling friction / pitch type
  (a curl on Frozen should travel further before the target is reached). Open on
  both oracles.
- Whether the kick direction (`+56` / Amiga `+$38`) is ever rewritten mid-window. If
  it were, kick curl would compound the way pass curl does. Nothing found suggests
  it is, but the field has several writers.
- Whether the height switch can fire twice if the window is interrupted before tick
  4 and restarted — i.e. whether a quick second touch grants a second lob.
- Whether the asymmetries in the curl table (§11) are deliberate tuning or
  table-entry errors in the original.

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
  We now have candidate values for every one of them (§11), which makes the fitting
  a confirmation exercise rather than a search.
- **Keep the decay ramp as a table.** It is not exponential and not linear; fitting
  a curve to it gets ticks 3–6 wrong, which is exactly where a late reaction lands.
- **Reproduce the per-axis speed correction even though it is a rendering
  artefact.** Vertical shots are the common case in front of goal, and getting them
  33 % too fast is immediately obvious.
- **Trace the whole window** — ten ticks of `spinTimer`, `leftSpin`, `rightSpin`,
  `destX`, `destY`, `speed` and `deltaZ` per kick is a small, complete, checkable
  record, and it belongs in the trace format from the start.

---

## 11. Amiga cross-check

Traced independently through the Amiga original — [amiga/AFTERTOUCH.md](amiga/AFTERTOUCH.md).

### All seven tables, with values

| §7 symbol | Amiga symbol / line | Value |
|---|---|---|
| `kSpinMultiplierFactor` | `spinMultiplierFactor`, asm:30735 | **5, 4, 3, 2, 2, 2, 2, 1, 1, 1** |
| `kKickSpinFactor` | `kickSpinFactor`, asm:30746 | 8 octants × 2 sides × (dx, dy), magnitudes 0 / 23 / 32 |
| `kPassingSpinFactor` | `passingSpinFactor`, asm:30778 | same shape, 0 / 11 / 16 — **half strength** |
| `kHighKickDeltaZ` | `off_10E658`, asm:30740 | **$20000** (2.0 px/frame) |
| `kHighKickBallSpeed` | `word_10E65C`, asm:30741 | **2688** |
| `kNormalKickDeltaZ` | `off_10E65E`, asm:30742 | **$16000** (1.375 px/frame) |
| `kNormalKickBallSpeed` | `word_10E662`, asm:30743 | **2560** |

The curl table in full:

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

23 ≈ 32/√2, so the diagonal cases are the axis-aligned magnitude resolved onto two
axes. The table is hand-tuned and **not perfectly symmetric** — several entries are
(0, 0) where symmetry predicts a value. Reproduce it verbatim rather than deriving
it; whether the gaps are deliberate is listed as open in §9.

**The decay ramp sums to 23, and the first three ticks contribute 12 of that** —
more than half the curl lands in the first 60 ms. Peak per-tick offset is 32 × 5 =
160 destination units on tick 0, which against a launch aim point 1000 units out is
a first-tick heading change of about 9°, falling under 2° by tick 7. That is the
quantitative form of §4's "sooner = stronger", and it is why the mechanic rewards
anticipation rather than reaction.

Total curl across a window: **736** destination units for a kick, **368** for a pass.

### "Low drive" is a misnomer

§5 calls the `diff ∈ {2, 6}` branch a **low drive**. Against the launch default of
`ballKickingDeltaZ` = $14000, the drive value $16000 is **higher**, not lower — it
is a *raised* drive, and the lob at $20000 is 60 % above the default. Both branches
also *increase* speed, from 2208 to 2560 and 2688 respectively. That reads as
counter-intuitive until you remember air friction is lower than ground friction
([BALL.md](BALL.md) §3): a lofted ball needs the extra pace to arrive at the same
time. **Nothing in aftertouch makes a ball fly lower than it left the foot.**

The Amiga also covers a case §5 omits: with **no joystick input at all** at tick 4,
the drive branch fires — $16000 / 2560, not "no change". Only pushing *on* with the
kick (diff 0, 1, 7) leaves the launch values alone.

### Three disagreements

1. **`+44` and `+56` look swapped in §2, and [MOVEMENT.md](MOVEMENT.md) agrees with
   the Amiga, not with this table.** §2 reads `currentAllowedDirection` (+44) as
   "direction the ball was launched" and `allowedPlDirection` (+56) as "current
   joystick direction". The Amiga has `currentDirection` at **+$2C = 44** —
   *"joystick octant this frame, −1 = neutral"* — and `field_38` at **+$38 = 56** —
   *"direction the ball was kicked in, the axis curl is measured against"*
   ([amiga/STATE.md](amiga/STATE.md) §4). [MOVEMENT.md](MOVEMENT.md) §3.1
   independently describes `currentAllowedDirection` as the per-tick input reading,
   which matches the Amiga. Two of our three readings say +44 is live input.
   **Treat §2's two rows as swapped pending a trace**; the code in §4 is written in
   terms of `joystickDir` and `kickDir`, so the logic survives the correction
   unchanged, but any implementation that copies the offsets will bend shots the
   wrong way.

2. **The latch subtraction runs the other way.** §4 computes
   `diff = (joystickDir − kickDir) & 7`; the Amiga computes
   `diff = (kickDirection − joy) & 7` (asm:40103–40133), with the same
   `diff < 4 → left` / `else → right` split. Since `(a − b) & 7` and `(b − a) & 7`
   are reflections about 0 and 4 — the two values that produce no curl — the guard
   behaves identically but **every non-zero case latches the opposite side**. One of
   the two transcriptions has the operands round the wrong way. This is a one-line
   error with a total effect on feel, and it is worth resolving before anything else
   in this document.

3. **Passes cannot be lofted.** §6 says `longPass` / `longSpinPass` "gate the lob"
   on the pass path. On the Amiga the pass branch (asm:40216) touches `deltaZ`
   **not at all**: both the "pulling back" and "across" cases do the same thing,
   `speed += speed/8`, and the flag pair at +$7C exists only to make that nudge
   one-shot. A pass can be hurried, never lofted. The Amiga also notes the pass
   height switch is *not* gated on tick 4 — it fires on the first tick the condition
   is met. §6's other two rows (a separate, weaker table; indexed by the ball's
   current travel direction rather than the launch direction) are both confirmed.

### Confirmed exactly

- The window is **10 ticks**, counted 0–9, `−1` inactive, reset at 10. ✓
- The side is latched on the first off-axis push and **cannot be reversed** within
  the window. ✓ The Amiga is emphatic that this is the single most important
  behavioural property: reading the stick fresh each tick "lets players wiggle the
  ball into the goal and destroys the skill ceiling".
- Neutral stick and the two collinear octants (diff 0 and 4) produce no curl. ✓
- Curl is applied to the **aim point**, before the ball integrates, never to
  velocity. ✓
- The height switch fires at **tick 4 exactly**, for kicks only. ✓
- The three early-exit guards match §3 exactly: opponent's keeper save-latch
  negative, play not live while the keeper holds, timer already −1. ✓
- The struct offsets in §2 match byte for byte — `spinTimer` 118, `leftSpin` 120,
  `rightSpin` 122, the `longPass` pair 124/126, `passInProgress` 128. The Amiga
  reads +124 as one longword used as two independent word flags, which is exactly
  §2's `longPass` / `longSpinPass` pair. ✓
- The speed trim magnitudes match: ×3/4 on the vertical axis, ×7/8 on diagonals,
  none on the horizontal. But see the note below on what indexes them.
- Aftertouch **does not survive a rebound or a contact** — post, net and possession
  changes all call the reset. ✓ (§3)
- The CPU applies aftertouch through the same code path, as a synthetic joystick. ✓
  (§8)

### One more thing to check in the port

§5's speed trim is keyed on the **joystick direction at tick 4**. The Amiga applies
the same three multipliers keyed on the **kick octant** (asm:40170–40194) and
explains them as a screen-space correction — the pitch is drawn with vertical
foreshortening, so a ball travelling up-screen covers more apparent ground per pixel
and is damped hardest. That explanation only makes sense keyed on the direction of
travel, i.e. the kick axis. Another swapped operand, most likely, and the same class
of error as (1) and (2) above.
