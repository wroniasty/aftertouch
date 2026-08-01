# B2 — Frame & state machine

The 50 Hz tick spine: two-level match state machine, clock, period end,
statistics counters, and out-of-play *classification* helpers. Excludes ball
physics (B3), movement (B4), foul contact (B7), and restart/referee execution (B8).

Depends on: B1   Blocks: B3–B10, B12, C1, C5   Wave: 2

---

## 0. One-paragraph version

A match is one fixed-step loop. B2 replaces the A2 stub `Step` with the ordered
spine from [../SIMULATION.md](../SIMULATION.md) §1 — real `UpdateTime` and state
transitions, stubs for ball/players/referee — so a headless run reaches full time
with score 0–0. `GameStatePl` (live / stopped / waiting) gates the clock;
`GameState` (0–31) names the situation. Clock math is Amiga-profile (refill 49,
default `game_length = 0` → 8820 InProgress ticks for 90′). Fouls and cards are
hooks only; B7/B8 own detection and presentation.

---

## 1. Scope

**In:**

- `GameState` / `GameStatePl` enums; `MatchClock`; `TeamStats` ×2.
- `MatchEngine::Step` order: time → controls stub → ball stub → players stub →
  referee stub → stats.
- Kick-off → InPlay, half-time swap, full-time end (no ET/pens by default).
- Injury-time gate (`prolongLastMinute`).
- Pure `ClassifyBallOutOfPlay` helpers (not wired into Step).
- ATTR v3 for clock + stats; full-match 0–0 acceptance test.

**Out:**

| Excluded | Owner |
|---|---|
| Ball friction / bounce / predictors | B3 |
| Player movement / placement | B4 |
| Possession / kicks / aftertouch | B5–B6 |
| Foul contact, headers | B7 |
| Restart execution, referee actor | B8 |
| AI decisions | B9 |
| Device input | B10 |
| ET / penalties / away-goals machines | B8 / D3 |
| Thin C1 debug view | Wave 2 follow-up |

---

## 2. Design

### 2.1 Tick order

```
last_roll = gameplay_rng.Draw()
UpdateTime                 ; clock + period ends (real)
UpdateControlsStub         ; ++team_switch_counter; A2 ball walk if InProgress
UpdateBallStub             ; no-op (B3)
MovePlayersStub            ; no-op (B4)
UpdateRefereeStub          ; no-op (B8)
UpdateStats                ; possession++ while InProgress
++tick
```

### 2.2 State machine

| `GameStatePl` | Value |
|---|---|
| InProgress | 100 |
| Stopped | 101 |
| WaitingOnPlayer | 102 |

`GameState` 0–31 as SIMULATION §2. Coarse `MatchPhase` derived: KickOff → InPlay →
HalfTime → InPlay → FullTime.

Kick-off without B8: roll `team_starting` / `team_playing_up`, centre spot in
`foul_x/y`, brief Stopped, then InProgress. Half-time: `stoppage_event_timer = 100`,
swap ends (`3 − x`), resume. Full time at 90′ + injury clear → `GameEnded`.

### 2.3 Clock (Amiga 50 Hz)

`kGameLenSecondsTable = {30,18,12,9}`; refill **49** (PC 70 behind profile switch).
Default `game_length = 0` → 5400×49/30 = **8820** InProgress ticks for 90′.
No `lastFrameTicks` — each accumulator wrap adds one game-second.

Injury time (Amiga): `end_game_counter` starts at 50; prolonged while ball in box
(`y≤216` or `y>682`) or attack live. Ball placed at centre `(336,449)` at kick-off
so the default 0–0 run is not stuck in prolong.

### 2.4 Statistics

Seven counters per side: possession, corners, fouls, bookings, sendings-off,
attempts, on-target. Possession increments each InProgress tick for
`last_team_played` (1 or 2); 0 → nobody. Other counters are hooks for later parts.

### 2.5 Out of play

`ClassifyBallOutOfPlay` is pure and unit-tested. **Not called from Step** until B3
can put the ball outside the playable area — keeps acceptance at 0–0.

---

## 3. Interfaces

| Path | Role |
|---|---|
| `match_state.hpp` | Enums, `MatchClock`, `TeamStats` |
| `match_clock.hpp` / engine | `UpdateTime`, period transitions |
| `out_of_play.hpp` | Pure classification |
| `match_engine.cpp` | Step spine |
| `trace.hpp` | ATTR v3 |

Wall: still no SDL/I/O/float/clock syscall in `src/core/`.

---

## 4. Work items

1. Types + HashState  
2. UpdateTime + SM in Step  
3. OOP helpers + tests  
4. ATTR v3 + golden/corpus/determinism  
5. `test_full_match` + PLAN-CURRENTSTATE  

---

## 5. Tests and acceptance

| Test | Pins |
|---|---|
| `test_match_clock.cpp` | Accumulator; frozen when Stopped |
| `test_game_state.cpp` | KickOff→InPlay→HT→InPlay→FullTime |
| `test_oop_classify.cpp` | Geometry → GameState |
| `test_full_match.cpp` | length 0, empty input, FullTime, score 0–0, tick ≥ 8820 |

**Done when:** headless 90′ ends 0–0 in CI via `core_tests`.

---

## 6. Open questions

- CPU booking bias — defer with B7/B8.  
- ET/pens — storage allowed; machines later.  
- `TeamStatsData` DOS layout — never; our struct only.  
- Whether OOP auto-wires in B3 Step or stays a call from ball code — B3 decides.
