# B9 — AI

Player selection polish, zonal off-ball (already largely landed), goalkeeper, and
the CPU brain-as-joystick. CPU never moves players directly — it writes the same
stick/fire fields a human joystick writes.

Depends on: B8, A5   Blocks: B11   Wave: 3

Sources: [AI.md](../AI.md), SWOS `AI_SetControlsDirection` / keeper / selection.

---

## 0. One-paragraph version

Four systems: (1) controlled + pass-target selection with exclusions and switch
lockout, (2) tactics zonal off-ball (B4), (3) goalkeeper rest/claim/dive/catch,
(4) `AI_SetControlsDirection` which only writes input fields. Acceptance is a
CPU-vs-CPU short match with ball activity plus a scripted HashState pin.

---

## 1. Scope

**In:** Pass-target select, refine controlled selection, CPU brain
(shoot/pass/dribble/chase/restart/aftertouch), GK AI, CPU restart taker, wire
into `ApplyTeamControls`, tests + acceptance.

**Out:** Difficulty tiers, marking/pressing, SWOS ATTR CPU corpus (A3), card camera.

---

## 2. Design

### 2.1 Selection

- `UpdatePlayerBeingPassedTo` — facing-cone teammate (±1 octant) when the owner
  has a facing; else nearest eligible. Gated on InProgress / `ball_in_play`,
  throw-in / free-kick take (pass assist), and `player_switch_timer == 0`.
- Pass target is parked (`dest = pos`) while waiting / ball in flight.
- `TryCompletePassArrival` (B5) arms `player_switch_timer = 25` on handoff.
- `UpdateControlledPlayer` — swap when `ball_out_of_play`, exclude pass target +
  kicker, honour switch lockout.
- `PickCpuRestartTaker` — nearest outfield (or best finishing on pen) for CPU.

### 2.2 Brain (`AI_SetControlsDirection`)

Writes only `current_allowed_direction` / `direction` / fire pulses /
`ai_ball_spin_direction` / `ai_aftertouch_strength`. Order: aftertouch window →
restart take → header/tackle trigger → shoot → pass-under-pressure → dribble →
chase (+ swap to nearer pass target).

### 2.3 Goalkeeper

Rest narrows angle; claim when predicted landing in own box and keeper is much
closer than the ball; dive near the line; catch vs parry via tick dice vs skill
table.

### 2.4 Wire

`ApplyTeamControls`: controlled selection → possession → CPU brain or human
stick → pass candidate (facing-aware) → FK/throw shortfall tick → kick/contest →
GK dest for ordinal 1, else controlled/off-ball. Human fire refresh skips CPU;
aftertouch refresh steers CPU from spin fields.

---

## 3. Interfaces

| Path | Role |
|---|---|
| `ai.hpp` | Selection, brain, restart taker |
| `goalkeeper.hpp` | GK rest/claim/dive/catch |
| `movement.hpp` | Wire brain vs joystick; GK dest |
| `shooting.hpp` / `aftertouch.hpp` | CPU fire / spin |

---

## 4. Work items

| Item | State |
|---|---|
| Selection + exclusions + switch timer | landed |
| `AI_SetControlsDirection` shoot/pass/dribble/chase | landed |
| CPU aftertouch + restart take | landed |
| Goalkeeper rest/claim/dive/catch | landed |
| Wire `ApplyTeamControls` / fire / aftertouch | landed |
| Unit + acceptance tests | landed |

---

## 5. Tests / acceptance

| Test | Checks |
|---|---|
| `pass target excludes controlled and kicker` | selection |
| `cpu shoot sets fire…` | brain shoot |
| `cpu chase aims at ball` | chase octant |
| `goalkeeper claims landing…` | GK dest |
| `cpu restart taker fires…` | restart → InProgress |
| `cpu vs cpu short match…` | ball activity envelope |
| `scripted cpu chase/shoot hash…` | HashState pin |

**Done when:** CPU vs CPU short match moves the ball; scripted HashState stable.

---

## 6. Open questions

- Full SWOS dive delta tables vs simplified thresholds.
- Exact `AI_afterTouchStrength` pick at strike.
- Opponent-in-pass-cone abandon (simplified: teammates only).
