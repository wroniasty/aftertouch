# B3 — Ball physics

Per-tick ball motion between events: friction, gravity, bounce, dead-ball
barrier, goal-frame contact, landing predictor, and surface coefficients.
Destination rewriting, not velocity negation. Kick launch is B6; dribble is B5;
restart execution after out-of-play is B8.

Depends on: B2   Blocks: B4–B7, B9   Wave: 2

---

## 0. One-paragraph version

`UpdateBall` is the sole integrator. Each tick recomputes `delta.x/y` from
scalar `speed` and `(dest_x, dest_y)` via A2 `CalculateDeltaXAndY`, subtracts
Amiga-profile friction (air replaces ground; pitch factor only on a loose ball),
integrates position, applies live gravity while airborne, bounces with a hard
`0xA000` settle, fences the ball with a destination-mirrored barrier when play
is stopped, handles bar/post on the goal frame, publishes the coarse-banded
landing predictor into `ball_next_*`, and wires `ClassifyBallOutOfPlay` when the
ball leaves the playable rectangle. Acceptance is golden unit vectors plus a
scripted trajectory hash — real SWOS ATTR remains an A3 follow-up.

---

## 1. Scope

**In:**

- Profile constants: ground / air / gravity (Amiga default; PC behind switch).
- `MatchSurface` (three coeffs); Normal defaults at match start; ATTR **v4**.
- `UpdateBall` pipeline; remove A2 controls ball walk.
- Dead-ball barrier, simplified goal bar/post, landing predictor.
- Wire OOP → Stopped + foul spot; goals bump score / `MatchPhase::Goal`.
- `LaunchBall` test helper; golden friction / bounce / barrier / predictor /
  trajectory / OOP-wire tests.

**Out:**

| Excluded | Owner |
|---|---|
| Kick launch / aftertouch curl | B6 |
| Dribble re-aim / possession bands | B5 |
| Restart execution after OOP | B8 |
| Weather / pitch-type selection | match init / PITCH |
| Player movement | B4 |
| Contests / deflections | B7 |
| Full net / own-goal subdivisions | later ball pass |
| Real SWOS reference ATTR | A3 follow-up |
| Thin C1 debug view | Wave 2 follow-up |

---

## 2. Design

### 2.1 Tick order (Step)

```
last_roll = gameplay_rng.Draw()
UpdateTime
UpdateControlsStub          ; ++team_switch_counter only (no ball walk)
UpdateBall                  ; this part
MovePlayersStub             ; B4
UpdateRefereeStub           ; B8
UpdateStats
++tick
```

### 2.2 UpdateBall pipeline

Skip presentation (anim / shadow / hideBall image). Order from [BALL.md](../BALL.md) §2:

1. `CalculateDeltaXAndY(speed, from, dest)` → `delta.x/y`, `full_direction`
2. Octant: `direction = ((full + 16) & 0xFF) >> 5`
3. Friction (§2.3)
4. Save pre-integration `pos`
5. Integrate `x/y`
6. Live gravity when airborne or vertical velocity non-zero; integrate `z`; bounce (§2.4)
7. Dead-ball barrier if not InProgress (§2.5)
8. Goal frame bar/post (§2.6)
9. Landing predictor → `ball_next_x/y` (§2.7); alias `ball_next_y_ground_y`
10. OOP wire if InProgress and outside playable (§2.8)

### 2.3 Friction

```
decel = kBallGroundConstant
if (!home.player_has_ball && !away.player_has_ball)
    decel += pitch_ball_speed_factor
if (z.whole() != 0)
    decel = kBallAirConstant          // replaces
speed -= decel
if (speed < 0) speed = 0
```

Amiga: ground **16**, air **10**. PC: **13** / **4**.

### 2.4 Gravity and bounce

Airborne (or `delta.z != 0`): `delta.z -= kGravityConstant` (Amiga **4608**).
Then `z += delta.z`. If `z < 0`:

```
speed -= (speed * ball_speed_bounce_factor) >> 8
z = 0
d = -delta.z
d -= (d >> 8) * ball_bounce_factor
d |= 1
if (d > 0xA000) delta.z = d; else delta.z = 0
```

### 2.5 Dead-ball barrier

Active only when `game_state_pl != InProgress`. Allowed whole units:
`[53,618]×[100,799]`. On violation: mirror dest about post-integrate position
(`dest = 2*p - dest`), `speed >>= 1`, restore saved `pos`.

### 2.6 Goal frame

Bar: mouth x and `z > 15`. Post: thin strips (~8u) beside the mouth uprights
only — clear byline exits in the wider attempt band fall through to OOP
(corner / goal-kick / goal).

When past byline (`y < 129` or `y > 769`):

- **Bar** — mouth x and `z.whole() > 15`: negate `delta.z`, restore `z`, nudge
  saved `y` by one whole unit, then converge.
- **Post** — thin strips (~8u) beside the mouth uprights only (not the full
  attempt band): `reverseDestY`, clear both spin timers, then converge. Clear
  byline exits elsewhere fall through to OOP (corner / goal-kick / goal).
- Converge: `speed -= speed >> 2`, restore `x/y` from saved (plus bar y nudge).

Net / own-goal / commentary coin-flip are out of scope (coin-flip intentionally
not reproduced).

### 2.7 Landing predictor

Forward sim to `z < 0` with the same gravity constant and coarse bands
(rising ×8; z bands 35/30/20). `speed == 0` → landing = current whole position.
`ball_next_y_ground_y = ball_next_y` until a measured difference exists.

### 2.8 OOP wire

If InProgress and outside `[81,590]×[129,769]`, call `ClassifyBallOutOfPlay`,
set situation + `Stopped`, write `foul_x/y`. Goals: bump `score`,
`MatchPhase::Goal`. Restart ceremony is B8 — leave Stopped.

### 2.9 Surface

`MatchSurface { pitch_ball_speed_factor, ball_speed_bounce_factor,
ball_bounce_factor }`. Default **Normal**: `0 / 64 / 96`. Set in
`BeginMatchIfNeeded`. Full seven-pitch tables live in code for later selection.

---

## 3. Interfaces

| Path | Role |
|---|---|
| `profile.hpp` | Ground / air / gravity constants |
| `match_state.hpp` | `MatchSurface` |
| `ball.hpp` | `UpdateBall`, `LaunchBall`, helpers |
| `match_engine.cpp` | Call `UpdateBall`; no ball walk |
| `out_of_play.hpp` | Classification (unchanged) |
| `trace.hpp` | ATTR v4 |

Wall: still no SDL/I/O/float/clock syscall in `src/core/`.

---

## 4. Work items

1. Subfile  
2. Profile constants + `MatchSurface` + ATTR v4  
3. `UpdateBall` pipeline  
4. Remove A2 walk; wire OOP; `LaunchBall`  
5. Golden tests + regen corpus/determinism + PLAN-CURRENTSTATE  

---

## 5. Tests and acceptance

| Test | Pins |
|---|---|
| `test_ball_friction.cpp` | Ground/air/pitch-factor; Amiga constants |
| `test_ball_bounce.cpp` | Settle `0xA000`; `d\|=1`; factors |
| `test_ball_barrier.cpp` | Stopped-only; dest rewrite + speed/2 |
| `test_ball_predictor.cpp` | Rising ×8; known landing |
| `test_ball_oop_wire.cpp` | Scripted exit → Stopped + GameState |
| `test_ball_trajectory.cpp` | Scripted kick HashState pin |

**Done when:** trajectory + OOP-wire green in `core_tests`; full-match 0–0 still holds.

---

## 6. Open questions

- `ballNextYGroundY` vs `ballNextY` — aliased until measured.  
- Goal-net / top-of-goal / own-goal subdivisions — unread beyond bar/post.  
- Whether port constants match original binaries — verify against traces (LEGACY §17).  
- Exact bar/post region predicates beyond the simplified B3 geometry.
