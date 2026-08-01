# B5 — Possession

Proximity bands, capture/release of `player_has_ball`, dribble aim-ahead with
Control speed trim, and post-kick lockout countdown. Kick launch is B6; contests
are B7.

Depends on: B4   Blocks: B6–B7   Wave: 3

---

## 0. One-paragraph version

Possession is derived: the ball stays an independent entity. Each active-team
tick classifies planar squared distance and ball height into TeamControl bands,
captures when the controlled player is very close and the ball is controllable,
loses when they leave the close band, and while holding re-aims the ball ahead
via `kDefaultDestinations` while trimming speed by Control on alternate ticks.
`pass_kick_timer` blocks re-capture after a kick (timer set by B6). Acceptance is
a scripted dribble-and-turn hash — real SWOS ATTR remains an A3 follow-up.

---

## 1. Scope

**In:**

- Planar + z proximity bands on TeamControl.
- Capture / release of `player_has_ball`; `last_team_played` on capture.
- Dribble aim-ahead (`kBallPlOffsets` + `kDefaultDestinations`) + Control trim.
- `pass_kick_timer` countdown → restore `ball_can_be_controlled`.
- Wire inside `ApplyTeamControls` before per-player dest/speed.
- Golden unit tests + `test_dribble_turn` HashState pin.

**Out:**

| Excluded | Owner |
|---|---|
| Kick / pass launch, tap vs hold | B6 |
| Aftertouch | B6 |
| Tackle / header contests | B7 |
| CPU decisions | B9 |
| Camera follow | C2 |
| Real SWOS corpus ATTR | A3 follow-up |

---

## 2. Design

### 2.1 Tick order (unchanged)

Inside active side `ApplyTeamControls` (before `UpdateBall`): refresh distances →
bands → capture/release → if `player_has_ball`, dribble aim + Control trim. Ball
then integrates toward the new dest; on-ball player speed cut sees the same-tick
flag.

### 2.2 Bands (MOVEMENT §6 correction)

- Planar (squared `ball_distance`): `pl_very_close ≤ 32`, `pl_close ≤ 72`,
  `pl_not_far ≤ 2450`.
- Height (`ball.pos.z.Whole()`): `ball_less_equal_4` … `ball_above_17` at
  thresholds 4 / 8 / 12 / 17.

### 2.3 Capture / lose

Gain when controlled is `pl_very_close`, InProgress, `ball_can_be_controlled`,
`pass_kick_timer == 0`, not special state. Lose when leaving `pl_close` (hysteresis
so brief drift does not flicker). Clear `player_has_ball`; set `ball_out_of_play`
on lose so B4 auto-select can re-evaluate; clear it while stably holding.

### 2.4 Dribble

Runs after the carrier's speed is written. Never teleports the ball onto the
player. Stick held **and** `pl_very_close`: dest =
`player + kBallPlOffsets[dir] + kDefaultDestinations[dir]`; alternate ticks set
`ball.speed = player.speed + kBallSpeedDeltaWhenControlled[Control]`. Stick
released, or ball only `pl_close`: leave dest/speed alone (ball keeps rolling;
sharp turns with the ball out ahead can break possession).

### 2.5 Lockout

Decrement `pass_kick_timer` on the active team; at 0 set
`ball_can_be_controlled = 1`. B5 does not start kicks — B6 will set
`pass_kick_timer = 25` and clear controllable. Unit-test lockout by seeding the
timer.

---

## 3. Interfaces

| Path | Role |
|---|---|
| `possession.hpp` | Bands, capture, dribble, lockout, `GiveBallForTest` |
| `movement.hpp` | Calls `UpdatePossessionForSide` from `ApplyTeamControls` |

Wall: no SDL/I/O/float/clock in `src/core/`. No ATTR bump (reuse TeamControl).

---

## 4. Work items

1. Subfile  
2. Band helpers + unit tests  
3. Capture/release + lockout tick  
4. Dribble aim + Control trim; wire into ApplyTeamControls  
5. `test_dribble_turn` pin + re-pins + PLAN-CURRENTSTATE  

---

## 5. Tests and acceptance

| Test | Pins |
|---|---|
| `test_proximity_bands.cpp` | Planar 32/72/2450; z 4/8/12/17 |
| `test_possession_capture.cpp` | Enter very-close → has ball; leave close → clear; lockout |
| `test_dribble.cpp` | Dest = pos + offsets; Control trim on `tick & 2` |
| `test_dribble_turn.cpp` | Approach + turn while carrying; HashState pin |

Re-pin determinism / golden / corpus when Step behaviour changes hashes.

**Done when:** scripted dribble-and-turn `HashState` is stable under Amiga profile.

---

## 6. Open questions

- Leave-band hysteresis vs traces (leave `pl_close` vs leave very-close).  
- Control curve vs original binaries.
