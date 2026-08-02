# C1a — Debug match view (thin C1 slice)

Rectangle-and-dot renderer so Wave 2–3 matches are watchable and playable for
the feel gate. Explicitly **not** full C1 (atlas, tiles, weather). Camera *modes*,
lead-ahead and clipping are C2; sprites are C3; HUD chrome / replays are C4.

Depends on: B2, B4, B9, B10, A5   Blocks: Wave 3 play-feel gate   Wave: 2–3

**Status.** Playability update landed: ball-follow camera, landmarks, role cues,
play HUD, A5 kickoff bootstrap, FT freeze. Live MATCH writes sparse transcripts
to `traces/match_<seed>.txt` (plus `.atin`). Ready for the Wave 3 feel pass.

---

## 0. One-paragraph version

`DrawMatch` paints a green pitch in 320×200, draws ball + 22 players as coloured
dots, and (after this update) keeps a **ball-centred debug window** large enough
to read play, with a HUD that names clock, phase and restart. Match bootstrap
uses a real fictional fixture via `ApplyKickoff`. No atlas, no kits, no C2
lead-ahead. Full C1 remains Wave 4; do not grow this into the real renderer.

---

## 1. Scope

### Already landed (Wave 2)

- Fixed frustum `[53,618]×[100,799]` → 320×200 letterbox
- Pitch fill + outline + halfway line
- Home/away dots; controlled slot larger
- Ball shadow / height lift / dest guide
- HUD: tick, score, raw `gs`/`pl`, spin/has/fire hints
- `PitchToScreen` + `test_pitch_to_screen`
- `SeedPlayableMatch` in `main.cpp` (hand-rolled attrs/tactics)

### In — playability update

| Item | Why |
|---|---|
| **Ball-follow debug camera** | Whole-pitch view makes dots ~2 px; aftertouch and contests are unreadable |
| **Readable play HUD** | Feel gate needs clock, half, restart name — not raw enum integers alone |
| **Possession / role cues** | Ring on ball-holder; mark pass-target; optional GK tint |
| **Pitch landmarks** | Centre circle + penalty boxes as lines (still not tiles) |
| **A5 match bootstrap** | `MakeFictionalLeague` + `ApplyKickoff` + human home / CPU away |
| **Period UX** | Freeze / banner at HalfTime and FullTime; key to resume HT |

### Out

| Excluded | Owner |
|---|---|
| Atlas, surface tiles, weather | C1 |
| Five camera modes, lead-ahead ±40, 1/16 ease, dual clip | C2 |
| Kit sheets, octant sprites, animation tables | C3 |
| Scoreboard chrome, card hand-in art, replays | C4 |
| Audio | C5 |
| Bench / subs UI | C6 |
| Growing C1a into “almost C1” | plan sequencing note |

---

## 2. Design

### 2.1 Debug camera (not C2)

Keep mapper pure in `pitch_view.hpp`. Replace the fixed world frustum with a
**sliding window** centred on the ball (fallback: controlled home player if ball
off-field):

| Parameter | Starting value | Notes |
|---|---|---|
| Window size | ~280×175 pitch units | ~half the dead-ball box; tunable |
| Anchor | ball `(x,y)` planar | Ignore `z` for pan |
| Clamp | dead-ball box `[53,618]×[100,799]` | Window stays inside; no bench slide |
| Smoothing | optional lerp ≤ 8 u/tick | Snap is fine; ease is *not* C2 lead-ahead |

API sketch:

```
DebugView WindowAround(int16_t cx, int16_t cy);   // clamped
ScreenPos PitchToScreen(x, y, view, match_w, match_h);
```

Legacy whole-pitch mode stays behind a compile-time or runtime toggle
(`V` key or `kDebugFollowBall`) so corpus screenshots / old mental model remain.

**Hard rule:** no `cameraDirection`, no booking/bench modes, no lead velocity.
Those are C2 acceptance criteria.

### 2.2 Draw pass

Order stays fill → landmarks → players → ball overlays.

Additions:

1. **Landmarks** — centre spot, centre circle (approx), both penalty boxes, six-yard
   if cheap; goal-line markers at pitch ends.
2. **Players** — existing colours; **has-ball** white ring; **pass target** dim
   second ring on home; GK slightly darker shade of team colour.
3. **Ball** — keep shadow + lift + dest line (dest line is a feel instrument).

### 2.3 HUD

Window-pixel debug text (already not integer-scaled). Replace / extend lines:

```
45'12"  1-0  1H … sim 1.00x   phase + clock + cam + sim speed
KO / TI-L / FK / PEN …        short name from GameState (+ Waiting if pl≠InProgress)
P9 has  tgt=P11               controlled slot + pass target
z=.. spd=.. spin=..           keep existing kick/aftertouch line
tap=pass  -/+=speed  0=1x …   control legend + sim pace keys
[HT] SPACE continue           only when phase == HalfTime
[FT] ESC menu                 when phase == FullTime — stop Stepping
```

Sim speed (presentation only): `-` / `=` step through
`0.25× … 0.90× … 1.00× … 1.10× … 2.00×`; `0` resets to 1.00×. Scales the
fixed-timestep accumulator; engine ticks stay 50 Hz when they run.

Clock display: derive displayed minute/second from existing clock counters (same
math as B2); do not invent a second clock.

### 2.4 Match bootstrap (`SeedPlayableMatch`)

Replace hand-rolled tactics/attrs with A5:

1. `MakeFictionalLeague()`
2. Pick two team ids (fixed for determinism, e.g. 0 and 1)
3. `ApplyKickoff(league, home, away, sheet)` → `LoadState`
4. Force `sides[0].control.player_number = 1` (human), away `= 0` (CPU)
5. Leave kick-off / `StartingGame` to the engine — do **not** force
   `InProgress` unless a known bug requires it; if kick-off needs a fire press,
   document that in the HUD legend
6. Keep `game_length = 0` (Amiga default ≈ 3 min real time for 90′) — short
   enough for a feel pass; optional later: menu radio for length

App may link `at_data` for this; core stays untouched.

### 2.5 Period UX (app-only)

| Phase | Behaviour |
|---|---|
| `InPlay` / `KickOff` / `Goal` | Step every tick as now |
| `HalfTime` | Stop consuming accumulator; draw HT banner; **Space** resumes by calling the same half-time exit the engine already exposes (or one Step that advances the stoppage if that is already automatic — verify against B2 and match) |
| `FullTime` | Stop Stepping; banner with final score; ESC → menu |

No substitution menu. No pause mid-play beyond ESC → menu (existing).

---

## 3. Interfaces

| Path | Role |
|---|---|
| `pitch_view.hpp` | Pure view window + `PitchToScreen`; no SDL |
| `match_renderer.cpp` | Draw + HUD strings |
| `main.cpp` | Bootstrap via A5; HT/FT input gating |
| `test_pitch_to_screen` | Extend for follow-window clamp + centre mapping |

Wall: still no SDL in `src/core/`. Camera state lives in app (or is derived each
frame from `MatchState` with no persistent lead accumulators).

---

## 4. Work items

Ordered, each independently committable:

1. **View window API** — `DebugView`, clamp, `PitchToScreen` overload; unit tests
   for centre → screen mid and edge clamp.
2. **Follow in `DrawMatch`** — ball-centred window; `V` toggles whole-pitch.
3. **Landmarks** — boxes + halfway (already) + centre marker.
4. **Role cues** — has-ball ring, pass-target ring, GK tint.
5. **HUD rewrite** — clock, phase, restart name table, FT/HT lines.
6. **Bootstrap** — fictional `ApplyKickoff`; human/CPU flags; remove synthetic
   attr fill (or keep only as fallback if data link fails).
7. **Period gating** — don't Step on FT; HT continue key; smoke-test one half
   manually.
8. **CURRENTSTATE** — mark C1a playability update done when §5 passes.

---

## 5. Tests and acceptance

**Automated**

- `test_pitch_to_screen`: window centre, clamp at corners, toggle path still
  covers full frustum.
- Existing harness keeps linking no engine SDL.

**Manual demonstration (closes the part)**

1. MATCH → kick-off visible; home controlled dot distinct.
2. Drive, pass (tap), shoot (hold), aftertouch — ball dest line and spin HUD
   readable because follow-cam is zoomed.
3. CPU opponent moves and contests; a restart shows a named state on HUD.
4. Reach half-time (or force via test seed / short run), continue, finish;
   FT banner shows score; ESC returns to menu.
5. Feel notes recorded for Wave 3 gate (sim issues → B parts; view issues that
   need sprites/camera → defer to C2/C3, do not patch into C1a).

**Done when:** a human can play a full match end-to-end on dots and honestly
answer “does the match feel right?” without squinting at a postage-stamp pitch.

---

## 6. Open questions

- Exact window size (280×175 vs tighter) — tune in play, pin a default in
  `pitch_view.hpp`.
- Whether HT already auto-resumes after `stoppage_event_timer` (B2); if yes,
  banner is informational only and Space is unnecessary.
- Menu team picker — nice; fixed two fictional clubs is enough for the gate.
