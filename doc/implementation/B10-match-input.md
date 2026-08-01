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

Default P1: arrows or WASD, Space/Z fire. P2: IJKL + Enter. Gamepad: D-pad /
left stick + South. ESC remains app-level leave-match.

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

## 6. Open questions

- Stick deadzone tuning vs traces.  
- Tap/hold fire thresholds (B6).
