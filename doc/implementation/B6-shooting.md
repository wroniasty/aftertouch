# B6 — Kicking

Tap vs hold → pass/shot launch, shot-on-goal attribute bonus, post-kick lockout
arming, and the 10-tick aftertouch window (lateral curl + drive/lob). Contests
are B7; set-piece dest tables are B8.

Depends on: B5   Blocks: B7   Wave: 3

---

## 0. One-paragraph version

Fire is classified into `quick_fire` (tap/pass) or `normal_fire` (hold/shot).
When the controlled player is in the close bands with a direction, the strike
aims the ball, sets base speed/height, applies a Velocity/Finishing zone bonus
for shots, clears possession, arms `pass_kick_timer = 25`, and opens
`spin_timer = 0`. Every game tick, aftertouch may latch a curl side and nudge
`ball.dest`, and at tick 4 may overwrite vertical launch. Acceptance is a
scripted curled-shot hash — real SWOS ATTR remains an A3 follow-up.

---

## 1. Scope

**In:**

- Tap/hold classify (`kFireHoldThreshold`; 4 planned, 12 shipped — see
  [B6a](B6a-kick-fidelity.md) §6).
- `ApplyKickOrPass` launch + lockout + spin open.
- Open-play aim via provisional dest table (`kDefaultDestinations`).
- Shot-on-goal Finishing/Velocity bonus; pass cone + pass speed bump.
- Aftertouch window (curl, tick-4 drive/lob, close at 10).
- Golden tests + `test_curled_shot` HashState pin.

**Out:**

| Excluded | Owner |
|---|---|
| Set-piece dest tables | B8 |
| Tackle / header contests | B7 |
| CPU joystick / AI aftertouch | B9 |
| Real SWOS `shot_curl` ATTR | A3 follow-up |
| Amiga hold-scaled power | Measurement |

---

## 2. Design

### 2.1 Fire classify

Hold ≥4 while down → one-tick `normal_fire`. Release with counter 1..3 →
one-tick `quick_fire`. No hold-scaled power.

### 2.2 Launch

Gated on InProgress, direction, `pl_very_close || pl_close`. Sets dest, base
speed/`delta_z`, attr bonus (shots), clears `player_has_ball`, arms lockout,
opens spin. **Pass = ground** (`delta_z = 0`); **shot = lofted** base arc.
Pass prefers current `pass_to` if still in facing cone and within Passing-scaled
max range (~70 + 8×Passing); else nearest cone teammate in range; else facing
ground kick (`pass_to = -1`). Pass speed uses `kBallSpeedPassingIncrease[Passing]`.

### 2.3 Aftertouch

Runs every Step for both teams with live `spin_timer`. Human stick is refreshed
**every tick** while the window is open (team controls alone are every other
tick). Latch left/right; dest nudge; tick 4 vertical; increment; at 10 → `-1`.

### 2.4 Provisional tables

Launch/spin/attr table **values** are fit targets (addresses in SHOOTING/
AFTERTOUCH). Placeholders ship for playable Fire; refine via A3 traces.

---

## 3. Interfaces

| Path | Role |
|---|---|
| `shooting.hpp` | Classify helpers, `ApplyKickOrPass`, launch tables |
| `aftertouch.hpp` | Window apply + spin tables |
| `movement.hpp` | Fire classify + kick before dribble |
| `ball.hpp` | Aftertouch at top of `UpdateBall` |

---

## 4. Work items

1. Subfile  
2. Fire classify + test  
3. Launch + lockout + wire  
4. Aftertouch + tests  
5. Curled-shot pin + re-pins + PLAN-CURRENTSTATE  

---

## 5. Tests and acceptance

| Test | Pins |
|---|---|
| `test_fire_classify.cpp` | Tap / hold ≥4 |
| `test_kick_launch.cpp` | Launch + lockout + spin |
| `test_shot_bonus.cpp` | Zone branch |
| `test_aftertouch.cpp` | Latch, nudge, tick 4, close |
| `test_curled_shot.cpp` | HashState acceptance |

**Done when:** scripted curled-shot `HashState` stable under Amiga profile.

> Superseded by [B6a](B6a-kick-fidelity.md) §3: a `HashState` pin is a
> determinism gate, not an acceptance criterion. Six structural defects in this
> part survived it. B6a's behavioural suite is the acceptance criterion now.

---

## 6. Open questions

- Exact hold threshold vs traces.  
- Table values and Amiga hold-power.  
- Open-play dest table vs `getBallDestCoordinatesTable`.

Structural follow-ups (curl geometry, window arming, fire level, charge
possession, shot zone, pass loft) are [B6a](B6a-kick-fidelity.md).
