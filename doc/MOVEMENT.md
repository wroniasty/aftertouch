# MOVEMENT.md

How a player moves in SWOS. Traced through the reference DOS port in
[../reference/swos-port/](../reference/swos-port/), including its annotated
disassembly ([swos/swos.asm](../reference/swos-port/swos/swos.asm)), which turns
out to carry the **actual data-table values** the earlier docs listed as unknown.
Companion to [CONTROL.md](CONTROL.md) (having the ball),
[SHOOTING.md](SHOOTING.md), [AFTERTOUCH.md](AFTERTOUCH.md) and
[PLAYER_SPRITES.md](PLAYER_SPRITES.md) (which frames get drawn while this
happens). Cross-checked against [LEGACY.md](LEGACY.md) §3 (controls), §7 (off-ball
AI), §8 (on-ball AI) and §9 (attributes).

> **Provenance.** Decompiled 68000/x86 plus a commented IDA dump. Control flow is
> reliable; the numeric constants below are read straight out of the data segment
> in `swos.asm` and are therefore *the original values*, not guesses — but they
> are still worth confirming against traces before they are load-bearing
> ([LEGACY.md](LEGACY.md) §15). Read to understand the design; write our own code.

---

## 0. One-paragraph version

There is no velocity vector and no acceleration. Every sprite has a **destination**
`(destX, destY)` and a scalar **`speed`**; each tick the engine converts
*(position → destination, speed)* into a fixed-point `(deltaX, deltaY)` through an
integer arctangent + sine table, then adds those deltas to the position and snaps
on arrival. The human-controlled player's joystick direction is turned into a
destination **1000 units away** in one of eight compass directions
(`kDefaultDestinations`), so "holding right" means "walk toward a point far to the
right"; **releasing the stick sets the destination to the player's own position**,
which is why SWOS stops dead with zero inertia. `speed` is looked up per tick from
the player's **Speed** attribute (`928…1250` in play) and then modified —
−12.5 % while carrying the ball, −injury handicap, ×0.625 after a goal. Off-ball
team-mates are given a destination instead of a direction: the tactics grid maps
the ball's quadrant to a target quadrant per player. The CPU plays by writing the
same `currentAllowedDirection` / fire fields a joystick writes — it is a virtual
joystick, exactly as [LEGACY.md](LEGACY.md) §8 predicted.

---

## 1. The per-tick pipeline

Order inside the main game loop
([swos.asm:@GameLoop](../reference/swos-port/swos/swos.asm#L112476), the loop body
calls in sequence):

```
UpdateAndApplyTeamControls   ; decide: read input / run AI, set direction+destination+speed
UpdateBall                   ; ball physics, ball quadrant
MovePlayers                  ; integrate: step animation, add deltas to x/y
UpdateReferee ... DrawSprites
```

**Decision and integration are separate passes.** As far as movement goes,
`UpdateAndApplyTeamControls` only writes `destX/destY`, `speed`, `direction` and
`deltaX/deltaY`; it never touches `x`/`y`. `MovePlayers` does that, and nothing
else.

### 1.1 One team per tick

`UpdateAndApplyTeamControls` handles **one team per frame**, alternating on a
counter ([gameControls.cpp:57-62](../reference/swos-port/src/controls/gameControls.cpp#L57-L62)):

```cpp
auto team = ++m_teamSwitchCounter & 1 ? &swos.topTeamData : &swos.bottomTeamData;
```

So each team's decision logic (`UpdatePlayers` — input, proximity, tackles,
off-ball destinations) runs at **half the frame rate**, while `MovePlayers`
integrates all 22 sprites **every** frame. Any reimplementation that runs both
teams every tick will feel different, and will halve the input latency asymmetry
the original has between the two teams.

### 1.2 The movement kernel

[`calculateDeltaXAndY(speed, x, y, destX, destY)`](../reference/swos-port/src/sprites/updateSprite.cpp#L231-L336)
is the whole of SWOS movement maths. Given a position, a destination and a scalar
speed it returns `(deltaX, deltaY)` as **16.16 fixed point**
([FixedPoint.h](../reference/swos-port/src/util/FixedPoint.h)) plus a fine angle:

1. `dx = |destX - x|`, `dy = |destY - y|`, remembering the signs.
2. **Halve both until both are < 32.** This is how an arbitrary offset is reduced
   to the 32×32 lookup domain, preserving the ratio (and losing precision on long
   vectors — deliberately).
3. `angle = kAngleTangent[dy][dx]` — an integer arctangent table, `round(32·y/x)`,
   splitting a quadrant into 64 steps
   ([updateSprite.cpp:16-49](../reference/swos-port/src/sprites/updateSprite.cpp#L16-L49)).
   `-1` means "no movement" (`dx == dy == 0`).
4. Fold the quadrant back into a full circle → `fullDirection`, **0–255, 0 = up,
   increasing clockwise**.
5. `cos = kSineCosineTable[angle]`, `sin = kSineCosineTable[(angle+64) & 0xff]` —
   a 256-entry `32767·sin` table
   ([updateSprite.cpp:56-89](../reference/swos-port/src/sprites/updateSprite.cpp#L56-L89)).
6. `delta = trig * speed >> 8`.
7. **PC only:** multiply by **41/64 (0.640625)**, done as
   `v - (v>>2) - (v>>4) - (v>>5) - (v>>6)` so the rounding matches the original
   bit-for-bit ([updateSprite.cpp:323-329](../reference/swos-port/src/sprites/updateSprite.cpp#L323-L329)).
   This is the PC-vs-Amiga frame-rate compensation: PC runs at **70 fps**, Amiga at
   **50** ([timer.h:3-4](../reference/swos-port/src/video/timer.h#L3-L4)).

So the direction is quantised to 1/256 of a circle before any speed is applied —
the movement vector is never an exact normalised `(dx, dy)`.

### 1.3 Integration

[`moveSprite`](../reference/swos-port/src/sprites/updateSprite.cpp#L157-L186), called
for all 22 players by [`movePlayers`](../reference/swos-port/src/sprites/updateSprite.cpp#L145-L155):

```cpp
if (sprite.deltaX) {
    sprite.x += sprite.deltaX;
    if (deltaX > 0 ? destX <= x : destX >= x) { x = destX; deltaX = 0; }   // arrive & snap
}
// ... same for y
```

Each axis stops **independently** the moment it reaches the destination component,
and the position is snapped exactly onto it. There is no z integration in the
player path — outfield movement is purely 2D; the jump in a jumping header is
animation, not height (contrast the ball, which has real `z`/`deltaZ`).

When both deltas are zero and the player is in `kNormal` state, the sprite falls
back to the standing animation table
([updateSprite.cpp:215-229](../reference/swos-port/src/sprites/updateSprite.cpp#L215-L229)).

---

## 2. Speed

### 2.1 The base table

[`updatePlayerSpeedAndFrameDelay`](../reference/swos-port/src/game/player.cpp#L17-L77)
runs for every player in `kNormal` state, every tick of that team's turn:

```cpp
static const int kPlayerSpeedsGameInProgress[] = { 928, 974, 1020, 1066, 1112, 1158, 1204, 1250 };
static const int kPlayerSpeedsGameStopped[]    = { 1136, 1152, 1168, 1184, 1200, 1216, 1232, 1248 };

player.speed = speedTable[playerInfo.speed];    // Speed attribute, 0..7
```

This is the answer to [LEGACY.md](LEGACY.md) §9's *"Speed presumably scales max
velocity, but by how much per point, and linearly or not, is unpublished"*:

- **Linear**, step **+46** per Speed point.
- **Range is narrow.** Speed 7 is only **34.7 %** faster than Speed 0. A "slow"
  SWOS player is not slow in the way modern football games are.
- **When play is stopped** (walking to positions, leaving the pitch) the spread
  collapses to +16 per point — everybody trots at roughly the same pace.

### 2.2 Modifiers, in application order

| Condition | Effect | Source |
|---|---|---|
| Goal just scored (`runSlower`) | `speed = 5·speed/8` (62.5 %) | [player.cpp:34-37](../reference/swos-port/src/game/player.cpp#L34-L37) |
| Injured, human-controlled team | `speed += kInjuriesSpeedHandicap[injuryLevel/32]` = `0, −96, −128, −160, −192, −224, −256, −288` | [player.cpp:39-43](../reference/swos-port/src/game/player.cpp#L39-L43) |
| **Is the controlled player and has the ball** | `speed -= speed/8` (87.5 %) | [player.cpp:45-48](../reference/swos-port/src/game/player.cpp#L45-L48) |
| Receiving a long/spin pass, running roughly with the ball | `speed` forced to `256` or `512` | [player.cpp:50-60](../reference/swos-port/src/game/player.cpp#L50-L60) |
| Half/full time whistle | `speed = max(speed − stoppageTimer·32, 0)` — gradual stop | [player.cpp:62-66](../reference/swos-port/src/game/player.cpp#L62-L66) |
| Walking off at half/full time | `speed = speed · min(stoppageTimer·4, 100) / 100` — gradual ramp up | [player.cpp:66-71](../reference/swos-port/src/game/player.cpp#L66-L71) |

The **ball-carrier penalty is the only "cost of dribbling" in the movement layer**
— a flat 12.5 %. Everything else that makes dribbling hard lives in
[CONTROL.md](CONTROL.md) (the ball is chasing an aim point of its own).

Note the injury handicap is applied **only** when `team.playerNumber ||
team.playerCoachNumber` — i.e. injuries slow human-controlled teams, not the CPU's.

### 2.3 Speed also drives the animation rate

```cpp
constexpr int kMaxSpeed = 1280;
player.frameDelay = std::max(kMaxSpeed - player.speed, 0) / 128 + 6;
```

Speed 7 → `frameDelay` 6 ticks/frame; Speed 0 → 8. This is the `frameDelay` the
frame stepper in [PLAYER_SPRITES.md](PLAYER_SPRITES.md) §7 counts down, so faster
players visibly churn their legs faster. Coupling the two in one place is worth
copying.

### 2.4 What the numbers mean in pixels

For axis-aligned motion `|trig| = 32767`, so per tick:

```
px/tick = ((32767 · speed) >> 8) · 41/64 / 65536
```

| Speed attr | `speed` | px/tick (PC) | px/s at 70 fps |
|---|---|---|---|
| 0 | 928 | 1.161 | 81.3 |
| 7 | 1250 | 1.564 | 109.5 |
| 7, carrying the ball | 1093 | 1.368 | 95.7 |

The playable pitch is 510 × 641 units (§4), so the fastest player covers its
length in ≈ 410 ticks ≈ **5.9 s**, the slowest in ≈ 7.9 s. Diagonals come out
isotropic (both components use the same table, `23170/32767 ≈ 0.707`).

---

## 3. The controlled player: eight directions into a destination

### 3.1 Input → direction

[`eventsToDirection`](../reference/swos-port/src/controls/gameControls.cpp#L163-L190)
collapses the four digital axes into one of eight compass values, `-1` for none
([swos.h:136-147](../reference/swos-port/src/swos/swos.h#L136-L147)):

| Value | Facing | Value | Facing |
|---|---|---|---|
| `-1` | `kNoDirection` | 4 | down |
| 0 | up | 5 | down-left |
| 1 | up-right | 6 | left |
| 2 | right | 7 | up-left |
| 3 | down-right | | |

Diagonals are tested **first**, so up+right always beats up. Opposite pairs
(up+down, left+right — reachable on a keyboard) are resolved by
[`filterOverlappedEvents`](../reference/swos-port/src/controls/gameControls.cpp#L225-L258),
which keeps whichever of the pair was *not* held last frame. Its comment is
explicit that without this "there are problems with doing long kicks" — the
original processes events such that up always trumps down, which breaks
aftertouch. Worth reproducing: it is an input-layer detail with gameplay
consequences.

The result is written to two fields
([gameControls.cpp:291-326](../reference/swos-port/src/controls/gameControls.cpp#L291-L326)):

- `currentAllowedDirection` — the direction the player is *allowed* to take this
  tick (may be overridden, see §5);
- `direction` — always the raw stick reading.

### 3.2 Direction → destination

For a player who is neither taking a throw-in nor tackling/heading, in play
([swos.asm:@@update_player_dest_x_y](../reference/swos-port/swos/swos.asm#L115297)):

```
destX = x + kDefaultDestinations[dir].x
destY = y + kDefaultDestinations[dir].y
```

and the table is, from the data segment
([swos.asm:245575](../reference/swos-port/swos/swos.asm#L245575) — *"values that
are used as x/y destination depending on direction player is moving; they're also
used for the ball while the game is running"*):

| `dir` | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
|---|---|---|---|---|---|---|---|---|
| `(dx, dy)` | `(0,−1000)` | `(1000,−1000)` | `(1000,0)` | `(1000,1000)` | `(0,1000)` | `(−1000,1000)` | `(−1000,0)` | `(−1000,−1000)` |

**1000 units is far off the pitch** (playable area is 510 × 641), so the
destination is never reached: it is a *heading*, expressed as a point. The
kernel of §1.2 then converts it back to an angle. Diagonals are exact 45°
because `dx = dy`.

This same table places the ball's dribble aim point ([CONTROL.md](CONTROL.md) §3),
the tackle slide target and the jump-header target (§9) — one table, five uses.

### 3.3 No input means stop, immediately

([swos.asm:@@skip_break_handling](../reference/swos-port/swos/swos.asm#L114675-L114690))

```
if (currentAllowedDirection < 0) { destX = x; destY = y; }
```

Destination equals position → `calculateDeltaXAndY` returns angle `-1` → both
deltas zero → the sprite is stationary **on the same tick**. There is no
deceleration anywhere in the outfield path. This, and the absence of a sprint
button ([LEGACY.md](LEGACY.md) §3), is the whole "feel" of SWOS movement: full
speed or nothing, instantly, in eight directions.

The same "destination = own position" idiom is used to stop a player everywhere
else in the codebase — when he loses the controlled slot, when he is about to
receive a pass, on `StopAllPlayers` at a restart.

### 3.4 Facing

After the deltas are computed
([swos.asm:@@update_player_speed_and_deltas](../reference/swos-port/swos/swos.asm#L117381)):

```
if (moving)                       direction = ((fullDirection + 16) & 0xff) >> 5;   // 8 octants
else if (controlled or kicker)    keep current direction;                            // don't spin on stop
else                              direction = ((fullDirection + 128 + 16) & 0xff) >> 5;
```

The quantisation is the one described in [PLAYER_SPRITES.md](PLAYER_SPRITES.md) §5.
Two things are worth pulling out:

- **A stationary controlled player keeps facing where he was.** Release the stick
  and he freezes mid-facing rather than snapping to a default — which is what makes
  "stand still and hold fire" aim where you expect.
- **For player sprites, `fullDirection` does not mean "movement angle".** At the
  end of each player's update the engine overwrites it with the fine angle **from
  the ball to the player**
  ([swos.asm:117480-117490](../reference/swos-port/swos/swos.asm#L117480-L117490)),
  computed with the same kernel at a dummy speed of 256. The `+128` above is
  therefore a 180° flip that makes an idle player **face the ball**. It is also
  what [player.cpp:53-57](../reference/swos-port/src/game/player.cpp#L53-L57) compares
  against `ballSprite.fullDirection` to ask "is this pass coming straight at him?".
  (`updateSpriteDirectionAndDeltas`, which stores the travel angle, is only used
  for the referee sprite; the ball and the players call the kernel directly.)

---

## 4. Pitch boundaries

Before the destination is written, the controlled player's position is tested
against the playable rectangle and a **mask of forbidden directions** is built
([swos.asm:115255-115280](../reference/swos-port/swos/swos.asm#L115258-L115295)):

| Test | Blocked directions | Mask |
|---|---|---|
| `x < 79` | 7, 6, 5 (facing left) | `0xE0` |
| `x > 592` | 1, 2, 3 (facing right) | `0x0E` |
| `y < 127` | 0, 1, 7 (facing up) | `0x83` |
| `y > 771` | 3, 4, 5 (facing down) | `0x38` |

If the requested direction is in the mask, `destX/destY` are set to the player's
own position — **he stops rather than sliding along the touchline**. Note the
asymmetry: he can still turn and run back in, because only the offending octants
are blocked, and it is a *stop*, not a clamp.

Related limits found elsewhere in the same code:

| Rectangle | Bounds | Where |
|---|---|---|
| Controlled-player stop test | x ∈ [79, 592], y ∈ [127, 771] | [swos.asm:115255](../reference/swos-port/swos/swos.asm#L115258) |
| Off-ball destination clamp | x ∈ [81, 590], y ∈ [129, 769] | [swos.asm:109700](../reference/swos-port/swos/swos.asm#L109683) |
| "Playable part of pitch" used by the keeper scaling | width 510, height 641, origin (81, 129) | [swos.asm:109740-109780](../reference/swos-port/swos/swos.asm#L109716-L109784) |
| Sliding player out-of-pitch test | x ∈ [73, 598], y ∈ [129, 769] | [swos.asm:115750](../reference/swos-port/swos/swos.asm#L115750) |
| Pitch centre spot | (336, 449) | [swos.asm:104028](../reference/swos-port/swos/swos.asm#L104028) |

The pitch is thus ~510 × 641 units with the centre at (336, 449), i.e. roughly
**1.26 : 1 tall**, and the units are pitch units, not screen pixels
([LEGACY.md](LEGACY.md) §15 still owns the scale question).

---

## 5. Turn restrictions at restarts (`playerTurnFlags`)

A single global byte gates which of the eight directions a player may face during
a **stoppage**. Bit *i* set = direction *i* allowed. Set at each restart, e.g.:

| Situation | Value | Meaning |
|---|---|---|
| Kick-off, team playing up | `0xC7` | E, NE, N, NW, W |
| Penalty at the top goal | `0x83` | NW, N, NE |
| Penalty at the bottom goal | `0x38` | SW, S, SE |
| Keeper holds the ball (top team) | `0xC7` | left / right / up combo |
| Keeper holds the ball (bottom team) | `0x7C` | left / right / down combo |
| Keeper holds it, CPU team | `AND 0b10111011` | additionally strips left and right |
| Free kick | `0xFF` | all directions |

([swos.asm:104034, 104508, 104579-104592, 107594-107783](../reference/swos-port/swos/swos.asm#L104508))

If the player's current facing is not permitted the engine walks **down from
direction 7** to find the first allowed one
([swos.asm:114588-114606](../reference/swos-port/swos/swos.asm#L114588-L114598)) — a
deterministic, slightly arbitrary fallback that also drives `cameraDirection`.

Crucially, **the in-play movement path never tests `playerTurnFlags`**. During
`ST_GAME_IN_PROGRESS` the only constraint on turning is the pitch boundary mask of
§4. There is no turn-rate limit, no facing inertia: any of the eight directions is
reachable from any other in one tick.

---

## 6. Which player you control

[`UpdateControlledPlayer`](../reference/swos-port/swos/swos.asm#L100851-L101034) runs
first thing in `UpdateAndApplyTeamControls`, for the team whose turn it is.

**It also computes `Sprite.ballDistance` for all 11 players**, and the metric is
**squared Euclidean distance** in whole pitch units:

```
ballDistance = (px - bx)² + (py - by)²      // 32-bit, never square-rooted
```

That resolves an open question in [CONTROL.md](CONTROL.md) §6 — it is Euclidean,
kept squared to avoid a `sqrt`, so every threshold in the codebase is a squared
threshold.

Selection is *closest eligible player wins*, where ineligible means: sent off;
off-screen while play is in progress; the goalkeeper (unless
`goaliePlayingOrOut`); the player currently passing/kicking; the player a pass is
being aimed at; or anyone in state `kTackling`, `kTackled`, `kJumpHeader`,
`kStaticHeader` or `kInjured`.

Two details that matter for feel:

- The swap is gated on the team's `ballOutOfPlay` flag, which the engine raises
  whenever the ball is struck or play breaks
  ([swos.asm:114977-114986](../reference/swos-port/swos/swos.asm#L114977-L114986)).
  So control **does not** hop to a nearer team-mate while you are dribbling; it
  re-evaluates once the ball is loose. On a kick, `controlledPlayerSprite` is
  cleared outright and reassigned on the next pass of this routine.
- When control moves, the **old** controlled player is stopped
  (`destX = x; destY = y`), and if the new one is the pass target he is stopped too
  "so he can take the pass".

This is the mechanism behind [LEGACY.md](LEGACY.md) §3's *"player selection is
automatic, you cannot cycle manually"*.

> **Correction to [CONTROL.md](CONTROL.md) §2.** That table reads
> `ballLessEqual4 … ballAbove17` as *planar distance* bands. In the code they are
> bands of the **ball's height** `ballSprite.z`, tested with `4/8/12/17`
> ([swos.asm:112720-112760](../reference/swos-port/swos/swos.asm#L112713-L112760)).
> The planar proximity flags are the other three, and they compare the **squared**
> distance:
>
> | Flag | Test | ≈ distance |
> |---|---|---|
> | `plVeryCloseToBall` | `ballDistance ≤ 32` | ≤ 5.7 u |
> | `plCloseToBall` | `ballDistance ≤ 72` | ≤ 8.5 u |
> | `plNotFarFromBall` | `ballDistance ≤ 2450` | ≤ 49.5 u |
>
> The gating logic CONTROL.md describes is right; only the labels are swapped.

---

## 7. Off-ball players are given a destination, not a direction

[`SetPlayerWithNoBallDestination`](../reference/swos-port/swos/swos.asm#L109581-L109784)
is called each tick for every player of the active team who is not the controlled
player and not in a special state. It is the concrete form of
[LEGACY.md](LEGACY.md) §7:

1. Pick the team's `Tactics` (or the `ballOutOfPlayTactics` variant during
   keeper's-ball / goal-out).
2. Index it by the **ball's quadrant** `ballQuadrantIndex`, 0…34 — the ball pitch
   is a **5 × 7 grid** (`ballXQuadrantLimits = 81,183,285,387,489`,
   `ballYQuadrantLimits = 129,220,312,403,495,586,678`).
3. Read one byte: the **player quadrant** he should occupy, packed as two nibbles
   `(x, y)` into a **15 × 16 grid**. Each player has his own 35-byte row
   (`ordinal−2` selects it; the keeper is handled separately).
4. For the bottom team, mirror: `index = 34 − ballQuadrantIndex` and
   `quadrant = 0xEF − quadrant`.
5. Convert to coordinates through
   `playerXQuadrantsCoordinates = 98,132,…,574` (step 34) and
   `playerYQuadrantCoordinates = 149,189,…,749` (step 40).
6. Add `playerXQuadrantOffset` / `playerYQuadrantOffset` — the ball's *sub-quadrant*
   offset, rescaled by `×5/15`, so the whole shape drifts continuously with the ball
   instead of snapping between cells
   ([swos.asm:110715-110800](../reference/swos-port/swos/swos.asm#L110715-L110805)).
7. Nudge `x` by **−4** (top team) or **+4** (bottom team) so players approach their
   spot from opposite sides, then clamp to x ∈ [81, 590], y ∈ [129, 769].

The goalkeeper instead gets a linear map of the ball's position into his own box:
`gx = 285 + ballX_rel · 103 / 510`, `gy` similarly into `[135,161]` (top) or
`[737,763]` (bottom).

Once the destination is set, off-ball players go through **exactly the same** speed
lookup and delta kernel as the controlled player. That is what makes them
indistinguishable in motion — there is one movement model, two ways of choosing a
destination.

---

## 8. The CPU is a virtual joystick

`AI_SetControlsDirection` ([swos.asm:117751](../reference/swos-port/swos/swos.asm#L117751))
is called in place of the input read when `team.playerNumber == 0`. Its first act
is to clear `currentAllowedDirection`, `firePressed`, `fireThisFrame`, `quickFire`,
`normalFire` — the **same five fields** the joystick path writes — then decide what
to press. Everything downstream (§3, §4, §5) is shared.

This confirms [LEGACY.md](LEGACY.md) §8's structural finding verbatim. The
practical consequence for us: **the AI must be an input source, not a movement
system.** If the CPU can write positions directly it will diverge from the player
model and the game will feel wrong in ways no amount of tuning fixes.

---

## 9. Non-normal movement

These bypass §3 and set `speed` and `destX/destY` themselves; the kernel and
integration are unchanged.

| State | Destination | Speed | Decay |
|---|---|---|---|
| **Sliding tackle** (`kTackling`) | `pos + kDefaultDestinations[dir]` | `kPlayerTacklingSpeed = 1792` | `−kPlayerGroundConstant (96)` per tick; at 0 → `SetPlayerDowntimeAfterTackle` |
| **Jumping header** (`kJumpHeader`) | `pos + kDefaultDestinations[dir]` | `kJumpHeaderSpeed = 2048` | `−kPlayerAirConstant (72)` per tick; halved once the down-timer passes 37 (or 17 after contact) |
| **Downed / tackled** (`kTackled`) | keeps sliding | inherited | `−96` per tick; **−25 %** first if inside the goal area (x ∈ [265,406], y < 159 or > 739) |
| **Sliding out of the pitch near a goal** | — | — | `speed >>= 4` then `\|= 1`, i.e. crushed to ~1/32 |
| **Substitute walking on/off** | scripted spot | `kSubstitutedPlayerSpeed = 1536` | none |

The tackle slide launches at **1792 vs a maximum running 1250** — 43 % faster than
any sprint — and burns off in `1792/96 ≈ 19` ticks (≈ 0.27 s at 70 fps), covering
roughly 21 units. That short, fast, uncontrollable lunge is exactly the risk
[LEGACY.md](LEGACY.md) §3 describes: *"a missed slide leaves you out of the play."*

`SetPlayerDowntimeAfterTackle` uses `kPlayerTacklingDownTime = 30,27,24,21,18,15,12,9`
(indexed by an attribute) for humans and a flat `3` for the CPU
([swos.asm:245858](../reference/swos-port/swos/swos.asm#L245843)) — the CPU gets up
almost instantly. That is a fairness asymmetry worth deciding about deliberately
rather than inheriting.

---

## 10. Constants quick reference

All values read from the data segment in
[swos.asm](../reference/swos-port/swos/swos.asm).

| Symbol | Value | Meaning |
|---|---|---|
| `kPlayerSpeedsGameInProgress` | `928, 974, 1020, 1066, 1112, 1158, 1204, 1250` | speed by **Speed** attribute, in play |
| `kPlayerSpeedsGameStopped` | `1136, 1152, 1168, 1184, 1200, 1216, 1232, 1248` | speed by attribute, play stopped |
| `kInjuriesSpeedHandicap` | `0, −96, −128, −160, −192, −224, −256, −288` | by `injuryLevel/32` |
| `kDefaultDestinations` | `(0,∓1000)`, `(±1000,∓1000)`, `(±1000,0)`… | per-direction destination offset |
| `kBallPlOffsets` | `0,−1, 1,−1, 1,0, 1,1, 0,1, −1,1, −1,0, −1,−1` | ball position relative to a controlling player |
| `kPlayerWithBallOffsets` | the negation of the above | ±1 dodge applied to the player with the ball |
| `kBallSpeedDeltaWhenControlled` | `130, 116, 102, 88, 74, 60, 46, 32` | by **Control**; ball speed offset while dribbling |
| `kPlayerTacklingSpeed` | `1792` | slide launch speed |
| `kJumpHeaderSpeed` | `2048` | header lunge speed |
| `kSubstitutedPlayerSpeed` | `1536` | substitute walking speed |
| `kPlayerGroundConstant` | `96` | ground friction per tick |
| `kPlayerAirConstant` | `72` | air friction per tick |
| `kPlayerTacklingDownTime` | `30, 27, 24, 21, 18, 15, 12, 9` | ticks on the ground (human) |
| `kComputerTacklingDownTime` | `3` (×8) | ticks on the ground (CPU) |
| `kPlAvgTacklingBallControlDiffChance` | `16, 17, 18, 19, 20, 21, 22, 23` | tackle contest threshold vs `rand()%32` |
| `playerXQuadrantsCoordinates` | `98, 132, … 574` (step 34, 15 entries) | tactics grid → x |
| `playerYQuadrantCoordinates` | `149, 189, … 749` (step 40, 16 entries) | tactics grid → y |
| `ballXQuadrantLimits` | `81, 183, 285, 387, 489` | ball grid, 5 columns |
| `ballYQuadrantLimits` | `129, 220, 312, 403, 495, 586, 678` | ball grid, 7 rows |
| PC delta scale | `41/64 = 0.640625` | applied to every delta on PC only |
| Target frame rate | 70 (PC) / 50 (Amiga) | [timer.h:3-4](../reference/swos-port/src/video/timer.h#L3-L4) |
| `frameDelay` | `max(1280 − speed, 0)/128 + 6` | animation ticks per frame |

`kPlAvgTacklingBallControlDiffChance` is the tackle contest
[LEGACY.md](LEGACY.md) §3 lists as `[UNKNOWN]`: the index is the difference of the
two players' *(Tackling + Control)/2*, a `rand() % 32` is drawn, and the ball is
won if the roll is greater. Full resolution belongs in a tackle document, not here.

---

## 11. Still open

- **The unit scale.** Everything above is in pitch units; the mapping to metres (or
  to the 8 units-per-tile pitch tiles) is still [LEGACY.md](LEGACY.md) §15 work.
- **Whether 70 fps is the simulation rate or just the render rate** on the original
  PC build, and how that interacts with the 41/64 factor (the two do not cancel
  exactly: PC ends up ≈ 10 % slower in absolute terms than Amiga).
- The **`ballOutOfPlay` semantics** in §6 — the disassembly's name is misleading and
  the flag is written from several places; worth a trace before relying on the
  "no switching while dribbling" reading.
- Whether the **stationary-player-faces-the-ball** rule (§3.4) applies to every
  non-controlled player or is gated by state; the branch is shared with a
  goalkeeper special case.
- Exact behaviour of `filterOverlappedEvents` on a real joypad hat vs keyboard.

---

## 12. Guidance for the reimplementation

- **Model movement as `(destination, scalar speed)`, not as a velocity vector.**
  Every SWOS behaviour — running, dribbling, sliding, off-ball positioning, the
  keeper tracking the ball — is expressed by choosing a destination. One kernel,
  many destination policies. Fighting this to get "proper" physics will reproduce
  none of the feel.
- **Keep the direction quantisation.** Angle → 1/256 circle → speed, with the
  destination reduced by repeated halving. Normalising `(dx,dy)` in floats gives a
  subtly different path and destroys determinism ([LEGACY.md](LEGACY.md) §8, §12).
- **Stop means stop.** Destination = own position, zero deltas, same tick. Do not
  add deceleration, and do not add a sprint button
  ([LEGACY.md](LEGACY.md) §3).
- **Eight directions, no turn cost.** Turn restrictions exist only at restarts.
  Any facing is reachable from any other in one tick during play.
- **Speed is a narrow linear band.** +46 per attribute point, ~35 % from worst to
  best, −12.5 % with the ball. Resist widening it; the whole risk model of the game
  assumes you cannot outrun anybody by much.
- **Make the AI write input, not positions.** `currentAllowedDirection` + fire flags
  are the only interface. This also gives replays and headless simulation for free.
- **Decide consciously about the alternating-team update.** The original updates one
  team's decisions per frame. Running both every frame is cleaner and probably
  better, but it is a deviation — measure it before assuming it is free.
- **Couple animation rate to speed in the simulation**, not the renderer
  (`frameDelay` above, and [PLAYER_SPRITES.md](PLAYER_SPRITES.md) §12).
- **Fit the tables from traces even though we now have the originals.** The values
  in §10 are a strong prior and a cross-check, not a licence to copy the data
  segment ([LEGACY.md](LEGACY.md) §15, §17, and the reference-tree policy in
  [PLAN.md](PLAN.md) §10).
