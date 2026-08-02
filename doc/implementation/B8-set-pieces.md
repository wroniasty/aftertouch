# B8 — Set pieces & referee

Restart setup (four writes), foul→penalty/FK classification, throw-in/corner/GK
execution with aiming tables, resume to open play, plus cards/injury rolls and
the referee state machine. CPU taker selection is B9; card camera/sprites are C2/C3.

Depends on: B7   Blocks: B9   Wave: 3

---

## 0. One-paragraph version

Every stoppage is a `GameState` plus four atomic writes (spot, camera facing,
turn flags, taking team) and `GameStatePl::Stopped`. Fouls classify by rectangle
into penalty, one of seven free-kick zones, or plain `Foul`. OOP already picks
throw-in/corner/GK states; B8 completes placement (throw-in taker ±3 off pitch)
and per-state aiming tables on the take. A successful kick/throw returns
`InProgress`. Carded fouls summon the referee sprite machine; injury uses the
SIMULATION §6 gates. Acceptance is a scripted restart-cycle HashState pin.

---

## 1. Scope

**In:**

- `BeginRestart` four writes + ball park.
- Foul classify → pen / FK / Foul; wire from B7 foul path.
- OOP complete-setup (turn flags, taker place, hide ball on throw-in).
- Set-piece aim tables; restart kick/throw → resume open play.
- Cards + injury rolls; referee states 0–5.
- Unit tests + `test_restart_cycle` HashState pin.

**Out:**

| Excluded | Owner |
|---|---|
| CPU who-takes / wall tactics | B9 |
| Card camera / blink art | C2 / C3 |
| Bench blocked while ref active | D |
| Penalty shootout mini-game | Later |
| Real SWOS ATTR corpus | A3 follow-up |

---

## 2. Design

### 2.1 Four writes

`BeginRestart(state, spot, turn_flags, camera_dir, taking_team)` sets GameState,
foul_x/y, camera_direction, player_turn_flags, last_team_played_before_break,
break_camera_mode=255, Stopped, parks ball.

### 2.2 Foul geometry

Penalty box → white spots; outer bands → FK zones by x (mirrored by offender);
else Foul at victim position.

### 2.3 Take / resume

While Stopped on a restart state, taking side may strike without `player_has_ball`
when near spot — only after tap/hold classification (`quick_fire` / `normal_fire`),
not on button-down. Throw-ins and free kicks: tap → pass to `pass_to`; hold →
aim-table kick/throw. After ~2 s idle a teammate approaches ahead of facing as
pass target (`long_pass` countdown). Success → InProgress + StartingGame, flags
0xFF, hide_ball clear.

### 2.4 Cards / injury / ref

SIMULATION §6 rolls via `resolve_rng`. Card → `ActivateReferee`. Injury allowance
= at most 2 injured outfield players per side (no ATTR bump).

---

## 3. Interfaces

| Path | Role |
|---|---|
| `set_pieces.hpp` | Restarts, classify, aim, take, cards/injury |
| `referee.hpp` | Ref state machine |
| `shooting.hpp` / `movement.hpp` | Restart take path |
| `ball.hpp` / `tackling.hpp` | Wire OOP / foul |
| `match_engine.cpp` | `UpdateReferee` |

---

## 4–5. Work items / tests

Per plan deliverables. **Done when:** every restart family resumes to InProgress;
scripted HashState stable under Amiga profile.

---

## 6. Follow-ups (play-feel)

- **Aim-only on take states:** `ApplyControlledDestination` stops translation while
  `IsRestartTakeState`; stick updates facing / turn flags only. Fire still takes.
- **`BeginRestart`:** `StopAllPlayers` + both sides `ball_out_of_play = 1`.
- **`PickRestartTaker`:** shared human+CPU selection in `set_pieces.hpp` (GK only
  for `KeeperHoldsBall`; else nearest outfield / best finishing on pen).
- Uncontrolled GK rest AI parks while not `InProgress`.

## 7. Open questions

- Goal-kick / keeper-holds release nuance.  
- `ST_FOUL` ceremony length vs immediate take.  

