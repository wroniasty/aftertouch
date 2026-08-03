# B10 — Match input

Device layer that produces `MatchInput` for the engine: two-level game-control
events, fixed keyboard/gamepad binds in app, pure direction collapse in core.
Alternation (one team per tick) is already B4. Configuration UI is out of scope.

Depends on: B4   Blocks: Wave 2 gate (with C1a)   Wave: 2

---

## 0. One-paragraph version

The engine already reads `MatchInput` into the seven TeamControl fields. B10 adds
the missing device path: SDL keyboard/gamepad → bitflag `GameControlEvents` →
pure `EventsToDir` → `MatchInput`, with a fixed default binding table and an
opposite-axis filter. Core stays SDL-free; app owns polling. Done when a human
can drive the home side from keyboard or gamepad in a live MATCH.

---

## 1. Scope

**In:**

- Core: `GameControlEvents`, `EventsToDir`, opposite-axis filter.
- App: `MatchInputSource` (keyboard + gamepad), main-loop wiring.
- Match bootstrap: home human / away CPU + tactics seed for playable dots.

**Out:**

| Excluded | Owner |
|---|---|
| Config UI / ini profiles | later |
| Bench / pause as engine events | C6 / later |
| CPU virtual joystick | B9 |
| Thin match view | C1a |

---

## 2. Design

Default P1: arrow keys, Space fire. P2: IJKL + Enter / Right Ctrl. Gamepad:
D-pad / left stick + South. ESC remains app-level leave-match.

WASD and Z were a second P1 binding until C1b; they were dropped so the letter
keys stay free for debug hotkeys and menu shortcuts. `SDL_GetKeyboardState` is
polled irrespective of focus, so the app gates the poll (and its hotkeys) on
`WantCaptureKeyboard` while a dialog has the keyboard — `PollNeutral` still
produces one `MatchInput` per tick so transcripts stay tick-aligned.

---

## 3. Interfaces

| Path | Role |
|---|---|
| `game_events.hpp` | Pure events → Dir |
| `input/match_input_source.*` | SDL → MatchInput |
| `main.cpp` | Poll each tick |

---

## 4–5. Work items / tests

`test_game_events.cpp`; manual MATCH walk. Walls: no SDL in `src/core/`.

---

## 6. The fire state machine

Per tick, per side, in `RefreshHumanFire` ([shooting.hpp](../../src/core/include/core/shooting.hpp)):

| Button | `fire_counter` | Raised | Meaning |
|---|---|---|---|
| down, first tick | 0 → 1 | `fire_this_frame` | press edge — contest entry (B7) |
| down | < threshold | — | charging |
| down | ≥ threshold | `normal_fire` **every tick** | hold — shot, re-tried until strikeable |
| released | 1 .. threshold−1 | `quick_fire` | tap — pass |
| released | ≥ threshold | — | the shot already went |

`normal_fire` is a **level**, not a pulse: the on-ball dispatch re-reads it each
frame (SHOOTING §1), so a hold that is refused on the threshold tick — no
direction, ball out of the close band — strikes as soon as it becomes
strikeable rather than being swallowed ([B6a](B6a-kick-fidelity.md) §2 S3).
`fire_counter` saturates at `kFireCounterMax` so a long hold cannot wrap into a
tap.

## 7. Open questions

- Stick deadzone tuning vs traces.  
- The tap/hold threshold value itself (B6a Track M).
