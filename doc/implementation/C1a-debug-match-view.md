# C1a — Debug match view (thin C1 slice)

Rectangle-and-dot renderer so Wave 2 movement is watchable. Explicitly **not**
full C1 (atlas, tiles, weather). Camera modes are C2; sprites are C3.

Depends on: B2, B4, A4   Blocks: Wave 2 gate (with B10)   Wave: 2

---

## 0. One-paragraph version

`DrawMatch` paints a green pitch in 320×200 logical space, maps pitch coordinates
through a fixed debug frustum covering the dead-ball box, and draws the ball plus
22 players as coloured dots (controlled slot highlighted). No atlas, no kits, no
lead-ahead camera. Full C1 remains Wave 4.

---

## 1. Scope

**In:** Pitch fill, simple lines, entity dots, `PitchToScreen` mapper, HUD tick/score.

**Out:** Atlas, tiles, weather, camera modes, kit sprites, HUD chrome beyond one-liners.

---

## 2. Design

View frames `[53,618]×[100,799]` into 320×200 (letterbox if needed). Home/away
colours differ; ball distinct; controlled larger/brighter.

---

## 3–5. Interfaces / tests

`pitch_view.hpp` (pure, no SDL); `match_renderer.cpp`; `test_pitch_to_screen.cpp`
in harness. Done when MATCH shows moving dots under B10 input.

---

## 6. Open questions

- Whether debug view should follow ball later (still not C2).
