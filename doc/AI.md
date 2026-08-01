# AI.md

Everything SWOS decides for you: which player you are driving, where the other
ten run, what the goalkeeper does, and how the CPU plays a whole match through a
simulated joystick. Traced through the reference DOS port in
[../reference/swos-port/](../reference/swos-port/) and its annotated disassembly
([swos/swos.asm](../reference/swos-port/swos/swos.asm)), which carries the AI's
data tables as real values.

Companion to [MOVEMENT.md](MOVEMENT.md) (how a destination becomes motion — the AI
only ever chooses destinations and inputs), [CONTROL.md](CONTROL.md),
[SHOOTING.md](SHOOTING.md) and [AFTERTOUCH.md](AFTERTOUCH.md). This is the
document [LEGACY.md](LEGACY.md) §7 (tactics / off-ball AI) and §8 (on-ball AI)
were written as questions for.

> **Provenance.** Decompiled 68000/x86 plus a commented IDA dump. Control flow is
> reliable and the constants are the originals, but several of the disassembler's
> field names are misleading — where a name and its use disagree I say so. Read to
> understand the design; write our own code ([LEGACY.md](LEGACY.md) §15, §17).

---

## 0. One-paragraph version

There is no single "AI". There are **four independent mechanisms** and they run for
both human and CPU teams alike. (1) **Selection**: each tick the engine measures
every player's squared distance to the ball and hands control of the *closest
eligible* one to whoever is driving that team, plus a *second* player marked as the
pass receiver. (2) **Off-ball movement**: everyone else is sent to a grid cell
looked up from a 35-entry-per-player tactics table, indexed purely by **which of 35
zones the ball is in** — no opponent is ever consulted, which is exactly why the
CPU famously does not close down. (3) **The goalkeeper**: a separate routine that
narrows the angle, forward-simulates the ball's flight to predict the landing
point, and decides between running, diving, catching and deflecting from a 30-word
per-skill table. (4) **The CPU brain**, `AI_SetControlsDirection`, which does not
move anybody: it writes `currentAllowedDirection`, `quickFire`, `normalFire` and
`firePressed` — **the same four fields a joystick writes** — and then the ordinary
player code runs. Its whole decision ladder rests on two numbers recomputed each
tick: the squared distance from the ball to the opponent's goal, and the fine angle
from the ball to that goal.

---

## 1. The shape: four systems, one input interface

Everything below hangs off `UpdateAndApplyTeamControls`
([swos.asm:100568](../reference/swos-port/swos/swos.asm#L100568),
[gameControls.cpp:67-90](../reference/swos-port/src/controls/gameControls.cpp#L67-L90)),
which handles **one team per frame**, alternating ([MOVEMENT.md](MOVEMENT.md) §1.1):

```
UpdateControlledPlayer()      ; §2 — measure ball distances, pick the controlled player
UpdatePlayerBeingPassedTo()   ; §2 — pick the pass receiver
if (team is human)  read joystick        -> currentAllowedDirection, fire flags
UpdatePlayers()               ; per-player: AI_SetControlsDirection (§5) if CPU,
                              ;             SetPlayerWithNoBallDestination (§3) if off-ball,
                              ;             goalkeeper branch (§4) if ordinal 1
```

| System | Runs for | Writes | Reads opponents? |
|---|---|---|---|
| Selection (§2) | both teams | `controlledPlayerSprite`, `passToPlayerPtr`, `Sprite.ballDistance` | no |
| Off-ball destinations (§3) | both teams | `destX/destY` of 10 players | **no** |
| Goalkeeper (§4) | both teams | keeper `destX/destY`, `speed`, `state` | no |
| CPU brain (§5) | CPU teams only | `currentAllowedDirection`, `quickFire`, `normalFire`, `firePressed`, `fireThisFrame` | yes (one) |

That last column is the single most important structural fact in this document.
**Three of the four systems never look at the opposition at all.** The only code in
the entire AI that considers an opponent is the CPU brain's pressure check (§5.4)
and `AI_Kick` (§5.8). This is the mechanical explanation for the two empirical
fingerprints in [LEGACY.md](LEGACY.md) §8 — the CPU does not close down, and it
lives off rebounds.

---

## 2. Selection: who is being driven

### 2.1 The controlled player

[`UpdateControlledPlayer`](../reference/swos-port/swos/swos.asm#L100851-L101034) runs
first, for the team whose turn it is. It loops all 11 players and, as a side effect,
**computes `Sprite.ballDistance` for each** as **squared Euclidean distance** in
whole pitch units:

```
ballDistance = (px - bx)² + (py - by)²        // 32-bit, never square-rooted
```

Every proximity test in the codebase is therefore a squared test. A player is
**ineligible** to be selected if any of:

- `sentAway`;
- `!onScreen` while play is in progress;
- he is the goalkeeper and `goaliePlayingOrOut == 0`;
- he is `passingKickingPlayer` (the one who just struck the ball) or `passToPlayerPtr`;
- his state is `kTackling`, `kTackled`, `kJumpHeader`, `kStaticHeader` or `kInjured`.

Among the rest, **smallest `ballDistance` wins**. Two consequences worth keeping:

- The swap only happens when the team's `ballOutOfPlay` flag is set — raised when
  the ball is struck or play breaks
  ([swos.asm:114977-114986](../reference/swos-port/swos/swos.asm#L114977-L114986)).
  So control does **not** hop to a nearer team-mate mid-dribble. On a kick,
  `controlledPlayerSprite` is cleared outright and reassigned next pass.
- When control moves, the **outgoing** player is stopped (`destX = x; destY = y`),
  and if the incoming one was the pass target he is stopped too, "so he can take
  the pass".

This is [LEGACY.md](LEGACY.md) §3's *"player selection is automatic, you cannot
cycle manually"*, and it is the same routine for human and CPU teams.

### 2.2 The second selected player

[`UpdatePlayerBeingPassedTo`](../reference/swos-port/swos/swos.asm#L101045-L101321)
picks a **second** player: the closest one to the ball *excluding* the controlled
player, stored in `passToPlayerPtr`. The disassembly's comment calls him
"temporarily controlled by cpu". He is stopped so he can receive, and he is
excluded from selection in §2.1 — so the two never collide.

Gates: only while `ballInPlay`, only while `playerSwitchTimer == 0`, and if a pass
is already in flight to somebody (`passingToPlayer && passToPlayerPtr`) it is left
alone. Eligibility is nearly the same list as §2.1 plus "must have been drawn last
frame" (`onScreen`).

### 2.3 The switch lockout

When a pass actually arrives, three sites do the same thing
([swos.asm:116441-116450](../reference/swos-port/swos/swos.asm#L116441-L116450) and
two others):

```
controlledPlayerSprite = passToPlayerPtr;
passToPlayerPtr        = 0;
playerSwitchTimer      = 25;
```

`playerSwitchTimer` counts down once per **team turn**, so 25 turns ≈ 50 frames
≈ 0.71 s at 70 fps. During that window no new pass receiver is chosen. That short
lockout is what stops the receiver flickering between team-mates the instant the
ball arrives.

---

## 3. Off-ball movement: a zonal grid, and nothing else

[`SetPlayerWithNoBallDestination`](../reference/swos-port/swos/swos.asm#L109581-L109784)
is called each tick for every player of the active team who is not the controlled
player, not the keeper, and not in a special state. Summarised in
[MOVEMENT.md](MOVEMENT.md) §7; here is the data model.

### 3.1 The tactics record

```cpp
struct PlayerPositions { byte positions[35]; };
struct TeamTactics {
    char          name[9];
    PlayerPositions positions[10];   // one row per outfield player
    byte          unkTable[10];
    byte          ballOutOfPlayTactics;   // index of the tactic to use at restarts
};                                        // 370 bytes
```
([swos.h:410-423](../reference/swos-port/src/swos/swos.h#L410-L423))

18 tactics exist — 12 built-in (`kTacticDefault`, `541`, `451`, `532`, `352`, `433`,
`424`, `343`, `Sweep`, `523`, `Attack`, `Defend`) plus 6 user slots
([swos.h:545-566](../reference/swos-port/src/swos/swos.h#L545-L566)).

**A tactic is 350 bytes: for each of 10 players, for each of 35 ball zones, one
byte saying which cell of the pitch he should stand in.** That is the whole
off-ball AI. It is a pure lookup — the "green ticks and red crosses" editor of
[LEGACY.md](LEGACY.md) §7 is literally editing this array.

### 3.2 The two grids

| Grid | Size | Limits |
|---|---|---|
| **Ball** zones (the index) | 5 × 7 = 35 | x: `81, 183, 285, 387, 489`; y: `129, 220, 312, 403, 495, 586, 678` |
| **Player** cells (the value) | 15 × 16 | x: `98, 132, … 574` (step 34); y: `149, 189, … 749` (step 40) |

The stored byte packs the target cell as two nibbles, `(x << 4) | y`.

### 3.3 The lookup, step by step

1. Choose the tactic — or `ballOutOfPlayTactics` if the state is keeper's-ball or
   goal-out.
2. `row = tactic.positions[playerOrdinal - 2]` (the keeper takes a different path, §4).
3. **Top team:** `cell = row[ballQuadrantIndex]`.
   **Bottom team:** `cell = 0xEF - row[34 - ballQuadrantIndex]` — the whole shape is
   point-mirrored by inverting both the index and the packed nibbles.
4. `destX = playerXQuadrantsCoordinates[cell >> 4] + playerXQuadrantOffset`
   `destY = playerYQuadrantCoordinates[cell & 15] + playerYQuadrantOffset`
5. The two offsets are the ball's position **within** its own zone, rescaled ×5/15
   ([swos.asm:110715-110805](../reference/swos-port/swos/swos.asm#L110715-L110805)).
   This is what makes the shape slide smoothly with the ball instead of snapping
   between 35 discrete formations — a genuinely clever touch for 1994.
6. `destX -= 4` for the top team, `+= 4` for the bottom, so the two teams approach
   their spots from opposite sides and do not stack.
7. Clamp to x ∈ [81, 590], y ∈ [129, 769].

The destination then feeds the ordinary movement kernel
([MOVEMENT.md](MOVEMENT.md) §1.2) at the ordinary speed for that player's **Speed**
attribute. Off-ball players are not special-cased in motion at all.

### 3.4 What it deliberately does not do

No opponent position, no ball velocity, no possession state, no man-marking, no
offside line, no pressing trigger. A defender "closes down" only insofar as the
tactics author put his cell near the ball's zone. Ten players re-target every tick
from one byte each.

This is worth stating plainly because it is the cheapest correct thing in the whole
engine, and because [LEGACY.md](LEGACY.md) §8's *"the CPU does not close down"* is
not a bug or a difficulty setting — it is the absence of any code that could.

---

## 4. The goalkeeper

The keeper is the one genuinely bespoke AI in SWOS. He never uses the tactics grid
(§3 branches him out on `playerOrdinal == 1`) and he has his own skill scale.

### 4.1 `goalieSkill` and the shot-chance table

[LEGACY.md](LEGACY.md) §9 records that keepers have no normal attributes and that
quality is derived from transfer value. The derived value is `goalieSkill`, 0–7,
and it selects one of eight 30-word rows
([team.cpp:5-19](../reference/swos-port/src/game/team.cpp#L5-L19),
[updatePlayerShotChanceTable](../reference/swos-port/swos/swos.asm#L105407-L105432)).
Outfield players get a single shared row, `kPlayerShotChanceTable`, so the same code
can run for anybody.

The eight rows, straight from the data segment:

| skill | full row (30 words) |
|---|---|
| 0 | `7, 424, -50, 832, 160, 4,5,6,7,7,7,7,7,7,7,7,7,7,7,7,7, 3,5,8,5,11,2,6,8,5` |
| 1 | `6, 588, -4, 864, 176, 3,4,5,6,6,6,6,6,6,6,6,6,6,7,7,7, 4,5,7,6,10,3,6,7,6` |
| 2 | `5, 752, 42, 896, 192, 2,3,4,5,5,5,5,5,5,5,5,5,5,6,7,7, 5,5,6,7,9,4,6,6,7` |
| 3 | `4, 916, 88, 928, 208, 1,2,3,4,4,4,4,4,4,4,4,4,4,5,6,7, 6,5,5,8,8,5,6,5,8` |
| 4 | `3, 1080, 134, 960, 224, 0,1,2,3,3,3,3,3,3,3,3,3,3,4,5,6, 6,6,4,9,7,6,6,4,9` |
| 5 | `2, 1244, 180, 992, 240, 0,1,1,2,2,2,2,2,2,2,2,2,2,3,4,5, 7,6,3,10,6,7,6,3,10` |
| 6 | `1, 1408, 226, 1024, 256, 0,0,0,1,1,1,1,1,1,1,1,1,1,2,3,4, 8,6,2,11,5,8,6,2,11` |
| 7 | `99, 1408, 226, 1024, 256, 0,0,0,0,0,0,0,0,0,0,0,0,0,1,2,3, 9,6,1,12,4,9,6,1,12` |
| — | `kPlayerShotChanceTable` = `8, 1024, 112, 800, 144, 7,7,7,3,4,5,6,7,7,7,7,7,7,7,7,7, 1,6,9,4,12,1,6,9,4` |

Only five of the thirty columns are read in this build. Decoded:

| Word index | Values across skill 0→7 | Use |
|---|---|---|
| **3** | `832, 864, 896, 928, 960, 992, 1024, 1024` | the keeper's **movement speed** while positioning (§4.2) |
| **4** | `160, 176, 192, 208, 224, 240, 256, 256` | a slow speed used when he is already set |
| **5…20** | the 16-value ramp, indexed by `currentGameTick & 15` | **anticipation error** used to time the dive (§4.4) |
| **24** | `5, 6, 7, 8, 9, 10, 11, 12` | **catch vs deflect** threshold (§4.6) |
| **29** | `5, 6, 7, 8, 9, 10, 11, 12` | whether he reacts to the ball at all this tick |

Indices 0, 1, 2 and 21–23, 25–28 are never read by any code path in this build —
worth flagging as either dead data or something the port has not wired up.

### 4.2 Resting position: narrow the angle

When there is nothing urgent, the keeper is given
([swos.asm:113105-113160](../reference/swos-port/swos/swos.asm#L113105-L113160)):

```
destX = 336 + (ball.x - 336) / 2          // halfway between the ball's x and the pitch centre
destY = ball.y + (goalLine.y - ball.y)/2  // halfway between the ball and his own goal line
speed = shotChanceTable[3]
```

That is textbook angle-narrowing expressed in two shifts. If the ball is already in
his six-yard area the halving is dropped and `destX = ball.x` outright.

A separate, coarser rule applies during breaks and in
`SetPlayerWithNoBallDestination`'s keeper branch: a **linear map** of the ball's
position into his own box,
`gx = 285 + (ballX − 81) · 103 / 510`, and `gy` into `[135,161]` (top) or
`[737,763]` (bottom) ([swos.asm:109716-109784](../reference/swos-port/swos/swos.asm#L109716-L109784)).

### 4.3 Coming for the ball: landing prediction

[`CalculateBallNextGroundXYPositions`](../reference/swos-port/swos/swos.asm#L106054-L106151)
**forward-simulates the ball** — stepping `deltaZ -= kGravityConstant`, `z += deltaZ`,
`x += deltaX`, `y += deltaY` until `z` goes negative — and publishes
`ballNextGroundX / ballNextYGroundY`. (It first drops `z` in chunks of 20.0, easing
to 13.0, as a coarse fast-forward.) A sibling routine,
[`UpdateBallVariables`](../reference/swos-port/swos/swos.asm#L105474-L106046),
publishes `ballDefensiveX` — the ball's x **at the moment it crosses the keeper's y**
— and `ballNotHighX/Y/Z`, its position when it first drops below head height.

The keeper commits to the landing spot when two conditions hold
([swos.asm:112960-113050](../reference/swos-port/swos/swos.asm#L112960-L113050)):

1. the predicted landing point is inside his penalty area
   (x ∈ [193,478], y ∈ [137,216] top / [682,761] bottom); **and**
2. `4 · dist²(keeper, landing) ≤ dist²(ball, landing)` — he is at least **twice as
   close** to the landing spot as the ball currently is.

Then `destX/destY = landing point`, `speed = kGoalkeeperMoveToBallSpeed (1024)`.

That "twice as close" rule is the whole of the keeper's cross-claiming judgement,
and it uses no attribute at all.

### 4.4 The dive decision

[`ShouldGoalkeeperDive`](../reference/swos-port/swos/swos.asm#L106562-L106714), with
`dy = ball.y − keeper.y` sign-corrected so positive means "in front":

1. **Ball behind him?** More than 10 units behind → never dive. Up to 10 behind →
   dive anyway.
2. **Penalty?** Always dive once the ball is within `kKeeperPenaltySaveDistanceFar`
   (20) or `…Near` (12), chosen at random 25 % / 75 %. The disassembly comments
   this as *"at penalty goalkeeper always dives, even if no chance of saving"*.
3. **Open play:** if `dy > kKeeperSaveDistance` (**16** on PC, **24** in Amiga mode)
   → too far, no dive.
4. `framesToBallY = dy / |ball.deltaY|`, and
   `framesToReachX = |ballDefensiveX − keeper.x| / |keeper.deltaX|`, both via
   [`GetFramesNeededToCoverDistance`](../reference/swos-port/swos/swos.asm#L106733-L106778)
   — a shift-and-subtract division with no `div` instruction.
5. **If he can run there in time (`framesToReachX ≤ framesToBallY`) he does not
   dive.** Running is always preferred.
6. Otherwise: pick an assumed dive speed
   `kGoalkeeperDiveDeltas[ max(errorRamp[tick & 15] − 1, 0) ]`, compute
   `framesForDive` at that speed, and dive only when
   `framesForDive ≥ framesToBallY`.

Step 6 is a **timing** gate, not a feasibility gate: he dives at the last moment
the dive can still cover the gap. Because a high-skill keeper's error ramp is all
zeros, he assumes the *slowest* dive delta (2.5 units/tick) and therefore commits
**earlier**; a skill-0 keeper assumes 4.0–5.5 and commits late. Skill is expressed
as anticipation, not as speed.

`kGoalkeeperDiveDeltas` = `2.5, 3.0, 3.5, 4.0, 4.5, 5.0, 5.5, 6.0` on PC
(`3.0 … 6.5` in Amiga mode,
[amigaMode.cpp:9-14](../reference/swos-port/src/game/amigaMode.cpp#L9-L14)).

### 4.5 Dive execution

[`GoalkeeperJumping`](../reference/swos-port/swos/swos.asm#L106167-L106285):

| Condition | Dive speed |
|---|---|
| `ballDistance ≤ 128` (≈ 11 units) | `kGoalkeeperNearJumpSpeed` = 1024 |
| further | `kGoalkeeperFarJumpSpeed` = 2048, or `…SlowerSpeed` = 1280, or `1024 + (tick & 0xFF)` |

Destination is `pos + kDefaultDestinations[dir]` like any lunge
([MOVEMENT.md](MOVEMENT.md) §9). `ballNotHighZ > 5` selects the **high** dive
animation, otherwise the **low** one; a separate left/right table picks the side.
The state becomes `kGoalieDivingHigh` / `kGoalieDivingLow` with a 75-tick down timer.

### 4.6 Catch or deflect

Once the diving keeper is `plVeryCloseToBall`
([swos.asm:113896-113925](../reference/swos-port/swos/swos.asm#L113896-L113925)):

```
roll = (currentGameTick & 0xF0) >> 4;      // 0..15
if (roll < shotChanceTable[24])  GoalkeeperClaimedTheBall();   // catch
else                             GoalkeeperDeflectedBall();     // parry
```

`shotChanceTable[24]` is `5,6,7,8,9,10,11,12`, so the catch rate runs from
**5/16 ≈ 31 % at skill 0 to 12/16 = 75 % at skill 7**. A separate gate with the
same numbers at index 29 decides whether he reacts to a loose ball at all. On a
catch, both keeper and ball speed go to zero; on a deflect, indices 26 and 27
choose how hard the parry is.

### 4.7 The keeper's dice are the clock

Note what the "roll" is: **bits of `currentGameTick`**, not the RNG. Several keeper
decisions, the catch/deflect split among them, are therefore periodic with period
16 or 256 frames rather than random. `Rand()` is used elsewhere (§6) but not here.
That is a real, reproducible quirk — a shot arriving on the same tick phase gets
the same outcome — and it is worth deciding deliberately whether to keep it.

---

## 5. The CPU brain

[`AI_SetControlsDirection`](../reference/swos-port/swos/swos.asm#L117751-L119122)
(≈1400 lines of asm;
[updatePlayers.cpp:15980-19313](../reference/swos-port/src/game/updatePlayers/updatePlayers.cpp#L15980)).
It is called from `UpdatePlayers` in place of the joystick read whenever
`team.playerNumber == 0`.

### 5.1 It is a joystick

Its first act is to clear the five input fields and then decide what to "press":

```
currentAllowedDirection = -1;
firePressed = fireThisFrame = quickFire = normalFire = 0;
```

Everything downstream — the destination offset, the pitch-boundary stop, the turn
flags, speed, animation — is the code from [MOVEMENT.md](MOVEMENT.md) §3–§5,
unchanged. This confirms [LEGACY.md](LEGACY.md) §8's structural finding verbatim,
and it is the single design decision most worth copying.

### 5.2 The two numbers everything hangs on

Recomputed every tick before any decision
([swos.asm:117800-117850](../reference/swos-port/swos/swos.asm#L117800-L117850)):

- **`D6`** = squared distance from the **ball** to the centre of the opponent's goal
  line — (336, 769) for the top team, (336, 129) for the bottom. So the top team
  attacks downward.
- **`D5`** = the fine angle (0–255) from the ball toward that point, obtained by
  running the movement kernel at a dummy speed of 256.
- **`D7`** = the controlled player's current `direction` (0–7).
- **`AI_rand`** = one `Rand()` draw, taken once per tick and reused by every branch.

Plus three timers: `AI_counter` (post-header goal-seeking, 15 ticks), 
`AI_resumePlayTimer` (15-tick action cooldown), and `team.AI_timer`.

### 5.3 Shoot

Reached when the CPU's controlled player is `plVeryCloseToBall || plCloseToBall`
([swos.asm:118298-118340](../reference/swos-port/swos/swos.asm#L118298-L118340)):

```
if (facing is exactly left/right && ball is on the byline near the goal) -> no shot
if (D6 > 28800)              -> no shot                       // > ~170 units out
if (D6 >= 12800)             -> only if (AI_rand & 3) == 0    // 1 in 4 from ~113..170
tolerance = (D6 <= 3200) ? 0x32 : 0x0F                        // ~70° inside ~57 units, else ~21°
error = (int8_t)(direction*32 - D5)
if (|error| <= tolerance)    -> SHOOT
```

This is the shoot decision [LEGACY.md](LEGACY.md) §8 lists as unknown, in full. The
range gate is on the **ball's** distance to goal, the aim gate is on the **player's**
facing versus the ball→goal angle, and the tolerance more than triples inside the
box.

The shot itself ([swos.asm:118745-118765](../reference/swos-port/swos/swos.asm#L118745-L118765)):

```
currentAllowedDirection = D7;         // his current facing — the aim error is NOT corrected
normalFire = 1;
AI_ballSpinDirection = (error >= 0) ? -1 : +1;
AI_resumePlayTimer = 15;
```

The CPU shoots with its **uncorrected** facing and then **curls the ball back with
aftertouch in the opposite sense to its own aiming error** (§5.9). That is a much
more interesting model than "snap to the goal", and it is why CPU shots bend.

### 5.4 Pass

If it does not shoot, it considers a pass. The pressure test comes first
([swos.asm:118360-118410](../reference/swos-port/swos/swos.asm#L118360-L118410)),
using the *opponent's* controlled player or pass target — the only place in the
whole off-ball AI that reads an opponent:

| Their `ballDistance` | Meaning | Result |
|---|---|---|
| `< 800` (≈ 28 units) | closing in | look for a pass now |
| `< 5000` (≈ 71 units) | nearby | look, but throttled by `AI_rand ≤ 8` and `(tick & 0x0C) == 0` |
| otherwise / none | free | keep the ball |

Other gates: `D6 < 9800` (inside ~99 units of goal) → **don't pass, keep going**;
`D6 > 180000` (~424 units out) → skip to the long-kick path.

The target search is
[`FindClosestPlayerToBallFacing`](../reference/swos-port/swos/swos.asm#L119329-L119401):

- scans **both teams**, 22 players;
- skips the controlled player, sent-off players and anyone not in `kNormal`;
- accepts a player whose `fullDirection` is within **±16/256 (≈ ±22.5°)** of the
  requested direction. Recall from [MOVEMENT.md](MOVEMENT.md) §3.4 that a player's
  `fullDirection` is the angle **from the ball to him** — so this asks *"who lies
  inside a 45° cone out of the ball, along the direction I am facing?"*;
- returns the one closest to the ball.

The caller then compares `teamNumber`. **If the nearest player in that cone is an
opponent, the pass is abandoned.** That single check is the CPU's entire
interception avoidance — and, since the search is over both teams, it is also why
the CPU will happily thread a ball into a gap but not straight at a marker.

On success ([swos.asm:118574-118600](../reference/swos-port/swos/swos.asm#L118574-L118600)):
`currentAllowedDirection = D7; quickFire = 1;` — a tap of the fire button, exactly
as a human passes ([LEGACY.md](LEGACY.md) §3).

### 5.5 Dribble and jink

With the ball and no pass on
([swos.asm:118500-118520](../reference/swos-port/swos/swos.asm#L118500-L118520)):

```
currentAllowedDirection = ((D5 + 16) & 0xff) >> 5;    // the octant pointing at the opponent's goal
```

Straight at the goal, quantised to eight directions. If instead a pass was being
sought and the cone found an opponent, the CPU **jinks**: `direction ± 1` octant,
with the sign flipping every 128 frames (`currentGameTick & 0x80`)
([swos.asm:118480-118498](../reference/swos-port/swos/swos.asm#L118480-L118498)).
A 128-frame square wave is the whole of the CPU's dribbling repertoire.

### 5.6 Chasing when off the ball

When nobody on the CPU team is near the ball
([swos.asm:118932-119020](../reference/swos-port/swos/swos.asm#L118932-L119020)):

- If the pass receiver is closer to the ball than the controlled player by ≥ 50
  (squared, ≈ 7 units), **swap their roles** — the CPU re-targets who it drives
  without waiting for §2's `ballOutOfPlay` gate.
- Otherwise `@@decide_if_flipping_direction`: turn toward the ball if he is already
  facing that way, **or** if `ballDistance < 800` (≈ 28 units), **or** on ticks where
  `(currentGameTick & 0x0E) == 0` — i.e. **2 ticks in 16**. The rest of the time he
  keeps running the way he already was.

That last rule is the origin of the CPU's characteristic lazy, curving pursuit: at
range it only re-aims at the ball 12.5 % of ticks, and it locks on properly only
inside ~28 units.

When *both* teams are CPU-controlled a further branch adds a random ±32/256 rotation
(`AI_randomRotateTable = {-32, 32}`) on 12.5 % of ticks, so CPU-vs-CPU matches do
not look mechanical.

### 5.7 Header and tackle trigger

[`AI_DecideWhetherToTriggerFire`](../reference/swos-port/swos/swos.asm#L119202-L119294)
runs before both the shoot and the chase branches. All of these must hold:

1. not the goalkeeper;
2. facing roughly at the opponent's goal — directions `{3,4,5}` for the top team,
   `{7,0,1}` for the bottom;
3. `ballDistance ≤ 648` (≈ 25 units);
4. the ball is in a **height window**: rising or grounded → `z ∈ [8, 14]`;
   falling (`deltaZ < 0`) → `z ∈ [12, 20]`;
5. the octant from the player to the ball (`fullDirection + 128`, rounded) equals his
   current facing.

Then `fireThisFrame = 1`, `currentAllowedDirection` = the octant toward the ball,
`AI_counter = 15`, and `AI_attackHalf` latches which goal to attack. For those 15
ticks
[`AI_SetDirectionTowardOpponentsGoal`](../reference/swos-port/swos/swos.asm#L119139-L119188)
overrides the direction with a crude three-band rule on the ball's x:

| ball x | attacking the bottom goal | attacking the top goal |
|---|---|---|
| `< 300` | 3 (down-right) | 1 (up-right) |
| `300…371` | 4 (down) | 0 (up) |
| `> 371` | 5 (down-left) | 7 (up-left) |

i.e. "after you win a header, run at the goalmouth for 15 ticks".

### 5.8 Challenging the carrier

[`AI_Kick`](../reference/swos-port/swos/swos.asm#L119410-L119478), called from the
players loop rather than the brain, fires when the opponent has the ball, a CPU
player is within `ballDistance ≤ 200` (≈ 14 units), and the two players' `direction`
fields differ by more than **32/256 (45°)** — i.e. they are not running together.
Then `firePressed = fireThisFrame = normalFire = 1`: a full-power clearance in the
challenger's facing.

(The disassembler labels the two `direction` reads as
`TeamGeneralInfo.allowedDirections` because both live at offset 42; on a `Sprite`
that offset is `direction`.)

### 5.9 Aftertouch

Once a shot or pass is away, `spinTimer` is live and the brain enters the
aftertouch path ([swos.asm:119048-119122](../reference/swos-port/swos/swos.asm#L119048-L119122)),
which is how the CPU gets the same bend a human gets
([AFTERTOUCH.md](AFTERTOUCH.md)):

```
if (!penalty && (AI_rand & 1) && AI_ballSpinDirection != 0)
     table = (spin < 0) ? AI_leftSpinTable {-1,-2,-3} : AI_rotateRightTable {1,2,3};
else table = AI_longKickTable {0, -999, 4};
currentAllowedDirection = (kickDirection + table[AI_afterTouchStrength]) & 7;
```

`AI_afterTouchStrength` (0/1/2) was chosen at the moment of the strike from the
game state and `D6`:

| Situation | Strength |
|---|---|
| penalty / penalty shoot-out | 0 (weak) |
| corner | 1 (medium) |
| free kick, or 3 in 8 of open play | by distance: `D6 < 28800` → 0, `< 57800` → 1, else 2 |
| otherwise | random among 0/1/2 via `AI_rand & 0x18` |

So the CPU curls harder from further out, and applies aftertouch on only half its
strikes (`AI_rand & 1`).

### 5.10 Restarts

During a stoppage the brain sweeps its aim: every 16 ticks it rotates the intended
direction by ±1 octant, flipping the sense via `AI_turnDirection`, and only accepts
a direction permitted by `playerTurnFlags` ([MOVEMENT.md](MOVEMENT.md) §5)
([swos.asm:118161-118215](../reference/swos-port/swos/swos.asm#L118161-L118215)).
If both teams are CPU-controlled it also presses fire to dismiss the score display
after 660 (or 385) ticks.

Every CPU action is throttled by `AI_resumePlayTimer = 15` and by
[`AI_ResumeGameDelay`](../reference/swos-port/swos/swos.asm#L119307-L119315), which
simply reports `passKickTimer < 13`. With `passKickTimer = 25` set on every kick
([CONTROL.md](CONTROL.md) §4), that gives the CPU a ~12-tick pause after any strike
before it will act again — the reason it does not machine-gun passes.

---

## 6. The RNG

[LEGACY.md](LEGACY.md) §8 calls a faithful RNG a prerequisite. It is tiny
([random.cpp:10-63](../reference/swos-port/src/util/random.cpp#L10-L63),
[swos.asm:7807-7838](../reference/swos-port/swos/swos.asm#L7807-L7838)):

```cpp
static int random(byte& seed, byte& xorKey, byte& xorIndex) {
    if (!seed)
        xorKey = kRandomTable[++xorIndex];      // re-key once per 256 draws
    return kRandomTable[seed++] ^ xorKey;
}
```

A fixed 256-byte permutation walked by an 8-bit seed, XORed with a key that only
changes when the seed wraps. Output is 0–255; the full period is 65 536 draws.
There are **two independent streams** (`seed`/`seed2`, each with its own key and
index), so gameplay and presentation can draw without perturbing each other.

Reproducing this exactly is cheap and is the precondition for replay-identical
matches ([LEGACY.md](LEGACY.md) §12).

---

## 7. Constants quick reference

| Symbol / value | Value | Meaning |
|---|---|---|
| `playerSwitchTimer` | 25 team-turns | lockout after control passes to the receiver |
| `plVeryCloseToBall` / `plCloseToBall` / `plNotFarFromBall` | `ballDistance ≤ 32 / 72 / 2450` | squared proximity bands |
| Ball zone grid | 5 × 7 = 35 | `ballXQuadrantLimits`, `ballYQuadrantLimits` |
| Player cell grid | 15 × 16 | `playerXQuadrantsCoordinates`, `playerYQuadrantCoordinates` |
| Tactic size | 370 bytes (10 × 35 cells) | `TeamTactics` |
| Tactics available | 12 built-in + 6 user | `Tactics` enum |
| `kKeeperSaveDistance` | 16 (PC) / 24 (Amiga) | max `dy` for a dive |
| `kKeeperPenaltySaveDistanceFar/Near` | 20 / 12 | penalty dive trigger, 25 % / 75 % |
| `kGoalkeeperDiveDeltas` | 2.5 … 6.0 (PC), 3.0 … 6.5 (Amiga) | assumed dive speed for the timing model |
| `kGoalkeeperGameSpeed` | 1024 | keeper base speed |
| `kGoalkeeperMoveToBallSpeed` | 1024 | running to the predicted landing spot |
| `kGoalkeeperNearJumpSpeed` | 1024 | dive within ~11 units |
| `kGoalkeeperFarJumpSpeed` / `…SlowerSpeed` | 2048 / 1280 | longer dives |
| `kGoalkeeperCatchSpeed` | 768 | after claiming |
| Keeper claim rule | `4·d²(keeper,landing) ≤ d²(ball,landing)` | come for the cross |
| Catch vs deflect | `(tick>>4)&15 < table[24]` = 5…12 | 31 % → 75 % by skill |
| Shoot range gates (`D6`) | `≤ 3200` wide aim, `< 12800` always, `< 28800` 1-in-4, else never | squared ball→goal distance |
| Shoot aim tolerance | `0x0F` (~21°) / `0x32` (~70°) close in | signed octant error vs `D5` |
| Pass pressure gates | opponent `ballDistance < 800` / `< 5000` | immediate / throttled |
| Pass suppression | `D6 < 9800` | close to goal: keep the ball |
| Pass cone | ±16/256 (±22.5°) | `FindClosestPlayerToBallFacing` |
| Header/tackle trigger | `ballDistance ≤ 648`, `z ∈ [8,14]` rising / `[12,20]` falling | `AI_DecideWhetherToTriggerFire` |
| `AI_Kick` trigger | `ballDistance ≤ 200`, facing differs > 45° | challenge the carrier |
| `AI_counter` | 15 ticks | post-header goal-seeking |
| `AI_resumePlayTimer` | 15 ticks | global CPU action cooldown |
| `AI_ResumeGameDelay` | `passKickTimer < 13` | ~12-tick pause after any strike |
| `AI_leftSpinTable` / `AI_rotateRightTable` / `AI_longKickTable` | `{-1,-2,-3}` / `{1,2,3}` / `{0,-999,4}` | aftertouch octant offsets by strength |
| `AI_randomRotateTable` | `{-32, 32}` | CPU-vs-CPU chase jitter |
| RNG | 256-byte table, 8-bit seed, re-keyed every wrap | period 65 536, two streams |

---

## 8. What this settles, and what is still open

**Answered from [LEGACY.md](LEGACY.md) §7 / §8:**

- The off-ball AI is a **35-entry-per-player zonal lookup** with sub-zone
  interpolation and no opponent awareness at all. ✓
- The on-ball AI is a **virtual joystick** writing the same five fields as a human. ✓
- **The shoot decision**: squared-distance bands, a `1-in-4` mid-range throttle, an
  aim tolerance that widens inside the box, and aftertouch used to correct the
  residual error. ✓
- **The pass decision**: triggered by opponent proximity, targeted by a ±22.5° cone
  out of the ball, abandoned when the nearest body in the cone is an opponent. ✓
- **Ball pursuit**: re-aim 2 ticks in 16 at range, continuously inside ~28 units,
  with a role swap when the receiver is closer. ✓
- **Goalkeeper quality** is `goalieSkill` 0–7 selecting a 30-word row; skill buys
  anticipation (earlier dive commitment) and catch rate, only marginally speed. ✓
- The RNG. ✓

**Still open:**

- Shot-chance table indices 0–2 and 21–28 are never read here. Dead data, or a path
  the port has not reproduced?
- `TeamTactics.unkTable[10]` — one byte per player, purpose unknown.
- Whether the keeper's use of `currentGameTick` as a dice source is original or a
  decompilation artefact; it makes several outcomes periodic rather than random.
- The tactics **byte values** themselves: this document explains the format, not the
  12 built-in formations' contents. They are extractable but are game data, not
  design ([LEGACY.md](LEGACY.md) §15).
- Whether the CPU ever changes tactics or makes substitutions mid-match — no code
  path was found, but absence of evidence in a 250 k-line dump is weak.
- The exact meaning of `ballOutOfPlay` gating selection (§2.1), which
  [MOVEMENT.md](MOVEMENT.md) §11 also flags.

---

## 9. Guidance for the reimplementation

- **Build the AI as an input source.** One interface: direction + fire. If CPU code
  can write positions or velocities, the two teams stop being the same game, and
  replays and headless league simulation ([LEGACY.md](LEGACY.md) §8, §12) become
  impossible.
- **Keep off-ball positioning zonal and data-driven.** A per-player, per-ball-zone
  cell table is small, editable, and reproduces the original's shape-holding. Resist
  adding marking or pressing to "fix" it — that *is* the game.
- **Keep the sub-zone interpolation.** Without the `×5/15` offset the formation
  snaps between 35 poses and looks immediately wrong.
- **Two selected players, not one.** The controlled player plus a designated
  receiver, with a switch lockout, is what makes passing feel deterministic.
- **Squared distances everywhere.** No `sqrt` in the decision layer; keep thresholds
  as squares so they stay integers and stay fittable.
- **Model the keeper as: narrow the angle → predict the landing point → run if you
  can, dive if you cannot, and roll for catch-vs-parry.** Four rules, one skill
  number. It is a better keeper model than its reputation suggests.
- **Make skill buy anticipation, not speed.** The keeper table's most interesting
  idea is that a good keeper *commits earlier* because he assumes a slower dive.
- **Decide about the tick-as-dice quirk explicitly.** Either reproduce it (periodic,
  exactly faithful) or replace it with the real RNG (fairer, slightly different
  feel) — but do it once, in one place, and write down which you chose.
- **Reproduce the RNG exactly.** It is 10 lines and it gates determinism.
- **Fit the decision thresholds from traces.** The values in §7 are a strong prior
  and a cross-check, not a licence to copy the data segment
  ([LEGACY.md](LEGACY.md) §15, §17; reference-tree policy in [PLAN.md](PLAN.md) §10).
