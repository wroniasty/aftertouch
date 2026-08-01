# B4 — Player movement

Per-tick player motion: alternating team control decisions, speed tables,
eight-way destination heading, pitch boundaries, restart turn flags, off-ball
tactics destinations, and MovePlayers integration with axis snap. Possession
(B5), contests (B7), CPU joystick (B9), and device input (B10) are out of scope.

Depends on: B3   Blocks: B5–B7, B9–B10   Wave: 2

---

## 0. One-paragraph version

Decision and integration are separate. Each tick one team runs
`ApplyTeamControls` (via `team_switch_counter`): map `MatchInput` into the seven
control fields for a human side, choose destinations for the controlled player
(`kDefaultDestinations` or stop) and off-ball teammates (tactics grid), look up
speed, and write deltas through A2 `CalculateDeltaXAndY`. After `UpdateBall`,
`MovePlayers` integrates all 22 sprites with per-axis arrive-and-snap. Kickoff
placement uses the same tactics→coord path. Acceptance is a 200-tick scripted
hash over twenty-two movers — real SWOS ATTR remains an A3 follow-up.

---

## 1. Scope

**In:**

- `PlacePlayersAtKickoff` (B4 owns pitch positions).
- `ApplyTeamControls` (one team/tick) + `MovePlayers` (all 22 every tick).
- Speed tables + injury / on-ball modifiers; `frameDelay` from speed.
- Controlled eight-way dest; boundary stop-mask; turn flags when stopped.
- Off-ball tactics destinations (including bottom-team mirror).
- Minimal auto-select when `ball_out_of_play`.
- Non-normal speed decay for existing tackle/header/down states.
- Golden unit tests + `test_move_players` 200-tick pin.

**Out:**

| Excluded | Owner |
|---|---|
| Device / SDL / mapping tables | B10 |
| CPU virtual joystick decisions | B9 |
| Possession / dribble / proximity | B5 |
| Tackle/header state *entry* | B7 |
| Kick / aftertouch | B6 |
| Thin C1 debug view | Wave 2 follow-up |
| Real SWOS ATTR | A3 follow-up |

---

## 2. Design

### 2.1 Tick order

```
UpdateTime
ApplyTeamControls     ; ++team_switch_counter; one side decides
UpdateBall
MovePlayers           ; integrate all 22
UpdateRefereeStub
UpdateStats
++tick
```

### 2.2 Controlled player

`MatchInput` p1 → side 0, p2 → side 1 when `player_number != 0`; else dir `-1`.
`dest = pos + kDefaultDestinations[dir]`, or `dest = pos` if None / boundary /
turn-blocked. Facing octant from travel heading while moving; controlled idle
keeps facing.

### 2.3 Off-ball

Ball quadrant 0…34 from limits; tactics cell → 15×16 coords; bottom side mirrors
index and quadrant. Clamp dest to playable. Keeper: linear map into own box.
Sub-quadrant drift uses cell centres in B4 (×5/15 polish deferred).

### 2.4 Speed

In-progress `{928…1250}` / stopped `{1136…1248}` by Speed attr 0…7 (clamp).
Modifiers: injury handicap; controlled+has_ball `−speed/8`. Non-normal: −96 / −72.

### 2.5 Integrate

Per axis: add delta; if past dest component, snap and clear that delta.

---

## 3. Interfaces

| Path | Role |
|---|---|
| `movement.hpp` | Placement, controls, integrate |
| `match_engine.cpp` | Step wiring |
| `angle.hpp` / `trig.hpp` | Dest table + kernel |

Wall: no SDL/I/O/float/clock in `src/core/`.

---

## 4. Work items

1. Subfile  
2. Placement + MovePlayers  
3. ApplyTeamControls (input, speed, controlled)  
4. Off-ball + boundary/turn + auto-select  
5. Non-normal + wire Step + tests + PLAN-CURRENTSTATE  

---

## 5. Tests and acceptance

| Test | Pins |
|---|---|
| `test_player_speed.cpp` | Tables + on-ball cut |
| `test_player_dest.cpp` | ±1000 / stop |
| `test_player_boundary.cpp` | Edge mask |
| `test_player_integrate.cpp` | Axis snap |
| `test_player_turn_flags.cpp` | Stopped-only |
| `test_offball_dest.cpp` | Tactics + mirror |
| `test_move_players.cpp` | 22 players, 200 ticks, HashState |

**Done when:** `test_move_players` green in `core_tests`.

---

## 6. Open questions

- Exact `ball_out_of_play` write sites for auto-switch — confirm vs traces.  
- Idle face-ball rule fidelity for non-controlled players.  
- Sub-quadrant `×5/15` drift — deferred polish.
