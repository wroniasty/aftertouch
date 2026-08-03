# C2 — Camera

The view that follows play: modes resolved by priority, the accumulated lead offset that
makes the camera anticipate rather than chase, a `/16` ease with a hard speed ceiling,
and two clipping stages against deliberately different bounds.

Depends on: C1   Blocks: C4   Wave: 4

**Status.** Landed.

---

## 0. One-paragraph version

`Camera::Update` runs once per simulation tick. It resolves one `CameraParams` — a
destination, a side margin and a lead offset — from a strict priority list (frozen ▸
booking ▸ shootout ▸ substitution ▸ standard), then applies the same three steps to all
of them: clip the destination against the mode's side margin, ease the position by one
sixteenth of the remaining distance capped at five units, clip the position against the
hard pitch limits. The lead is the part that matters: `±2` per tick toward `±40` in
whichever direction the ball is travelling, **added to the destination rather than to the
position**, so a player running at goal sees the goal. The camera lives outside
`at_core`, so none of this can touch a replay.

---

## 1. Scope

### In

- `Camera` in `src/app/render/` — position, lead, mode
- Priority list: frozen (`show_fans_counter`), booking, penalty shootout, substitution,
  standard; standard sub-switches on `GameState`
- Lead offset with the ball's delta in play and the taker's facing while stopped
- Ease + per-tick cap; destination clip and position clip, separately
- Random kick-off end, drawn from `presentation_rng`
- `DrawMatch` takes a camera instead of C1a's `follow_ball` boolean

### Out

| Excluded | Owner / reason |
|---|---|
| Bench mode and the left-edge slide | C6 — there is no bench to look at yet, and CAMERA.md §6's gate is unresolved even in the reference |
| Zoom | Rejected: a port addition, not the original's (PITCH.md §5) |
| Replay camera | C4 |

---

## 2. Design

### 2.1 Where it lives

Outside `at_core`, per CAMERA.md §11: the camera reads simulation state and affects
nothing, so keeping it in the app means a camera change can never break determinism.
The one thing that would have broken that rule is the kick-off coin flip, and it is
resolved by taking it from `presentation_rng` — the stream `HashState` already excludes
— through `MatchEngine::DrawPresentationRng()`. `test_camera` asserts the hash is
unchanged across two hundred camera updates.

### 2.2 The world is 880 tall, and the camera proves it

The camera position is the top-left of the 320×200 window, so its travel is world minus
window: `x ∈ [0, 352]`, `y ∈ [16, 680]`. Those are CAMERA.md §9's `kCameraMaxX` and
`kCameraMaxY` exactly — and `680 = 880 − 200` only holds in an 880-tall world. That is
independent confirmation of C3's Finding 3, arrived at from a different document, and
`test_pitch_tiles` pins it as an equality so the two cannot drift apart.

### 2.3 The two clips are not redundant

| | x | y |
|---|---|---|
| Destination clip (mode-dependent) | `[limit, 352 − limit]` | `[16, 664]` |
| Position clip (absolute) | `[0, 352]` | `[16, 680]` |

The destination is where the camera wants to be; the position clip is the wall. They
differ at the bottom by 16, so the camera can briefly sit where its destination was never
allowed. Merging them changes behaviour at the pitch edges, which is why they are two
functions and two tests.

### 2.4 One place CAMERA.md must not be followed literally

CAMERA.md §3 lists `kStartingGame` among the states that look at `(590, 449)` — the
"watch them walk off" view. **Our engine does not use that state the same way.**
`match_clock.hpp` starts play by flipping `Pl` to `InProgress` and `phase` to `InPlay`
while leaving `game_state` at `StartingGame`, and `set_pieces.hpp` returns to it after
every goal — so in our state machine it means *open play from a centre kickoff*, not
*players walking out*. Copying the reference's table verbatim pinned the camera to the
right touchline for most of a match, and for the whole of a sandbox session, which
starts in exactly that state and never leaves it.

`PlayersToInitialPositions` is our equivalent of the reference's walking-out state and
carries the walk-off view instead. Two tests hold the line: open play in `StartingGame`
must keep the ball on screen, and a sandbox driven 300 ticks through the real engine must
converge on the ball.

### 2.5 Side margins

63 in open play, 37 for corners and throw-ins, 51 while substituting. The narrow one is
what makes a corner visible at all; `test_camera` asserts a corner really does get the
camera nearer the touchline than open play does.

---

## 3. Interfaces

| Path | Role |
|---|---|
| `src/app/render/camera.{hpp,cpp}` | The camera. No SDL — `harness_tests` compiles it directly |
| `src/app/render/match_renderer.*` | `DrawMatch(..., const Camera*, ...)`; `ViewFor` replaces C1a's follow/full toggle |
| `src/core/include/core/match_engine.hpp` | `DrawPresentationRng()` |
| `tests/harness/test_camera.cpp` | Lead ramp, ease, clips, freeze, shootout, corner margin, hash invariance |

---

## 4. Tests and acceptance

**Automated** — `harness_tests`:

- the lead ramps by 2 to exactly ±40, holds at the cap, and reverses on a turn
- easing takes a sixteenth then caps at five, and never stalls one unit short
- the position stays inside the hard limits and the window inside the 672×880 world
- the kick-off end differs with the coin flip
- a ball travelling right ends up left of centre by exactly the full lead
- `show_fans_counter` freezes position *and* lead accumulation
- the shootout camera ignores the ball entirely
- a corner gets nearer the touchline than open play
- two hundred updates leave `HashState` untouched
- open play in `StartingGame` keeps the ball on screen (§2.4 regression)
- a sandbox driven 300 ticks through the real engine converges on the ball

**Manual:** `aftertouch --match` or `--sandbox` (add `--shot <path> --shot-after N` to
save a frame and quit) — the camera starts at one end, eases to play, sits ahead of the
ball, and never shows the far touchline during open play.

**Done when:** following play feels anticipatory rather than reactive — PLAN.md C2's
deliberately subjective gate. The lead offset is what that gate is about, and it is
built first for exactly that reason.

---

## 5. Open

- **Bench mode / the left slide** is deferred to C6 with the reference's own unresolved
  comment attached (CAMERA.md §6).
- **`break_camera_mode`** is written by every restart and read by nobody, here as in the
  reference. Left alone rather than invented.
- **Lead ramp constants** (2 / 40) are the porters' transcription. CAMERA.md §10 calls
  confirming them against a trace the highest-value measurement in the document, and it
  still is.
- **The `kFirstHalfEnded` fall-through** to follow-the-ball is reproduced as written.
