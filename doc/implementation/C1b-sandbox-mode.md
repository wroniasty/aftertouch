# C1b — Sandbox match mode

A second entry point beside MATCH that spawns a configurable **N-players-plus-a-keeper**
scenario for testing play feel and engine behaviour in isolation. App-side setup plus
one core rule (*players marked off the pitch stay off it*). Explicitly **not** a training
mode with drills, scoring or progression — and not a level editor; C1b builds one
`MatchState` and hands it to the unmodified engine.

Depends on: B4, B8, B9, B10, C1a, A5   Blocks: Wave 3 play-feel gate   Wave: 3

---

## 0. One-paragraph version

Pressing **SANDBOX** opens a config dialog — how many outfield players, which direction
they attack, each player's seven attributes, whether they get their own keeper — and
starts a match against a lone opposing goalkeeper. **R** rebuilds that exact kickoff at
any time, so a bug is reproduced rather than hunted. Everything except one core rule
lives in `src/app/`: the engine sees an ordinary `MatchState` whose unused pitch slots
are marked sent-off, and the core rule is what makes a sent-off player stay off the
pitch (which the engine did not previously do — a red card was undone by the next
kickoff).

---

## 1. Scope

### In

| Item | Why |
|---|---|
| **SANDBOX button + config dialog** | Second entry point; MATCH stays untouched |
| **1–10 outfield players, optional own keeper** | The point of the mode: isolate one striker, or a back four, without 22 dots |
| **Per-player attributes (0–7 × 7)** | Test speed / finishing / control in isolation; presets for the common cases |
| **Direction of play** | Fixed by the operator instead of rolled by `BeginMatchIfNeeded`'s RNG |
| **Lone opposing keeper** | Gives shots something to beat and restarts something to resume from |
| **`R` reset to kickoff** | Same seed, same config, bit-identical restart; `Shift+R` re-rolls the seed |
| **Off-pitch rule in core** | Absent slots must not be re-placed at kickoff or walked back by tactics |
| **Degenerate-side restart fixes** | A one-player side must still be able to take a throw-in |
| **Arrow-key movement + ImGui input gating** | Arrows are canonical; typing in the dialog must not drive the player |

### Out

| Excluded | Owner |
|---|---|
| Drills, targets, scoring, progression | not planned; this is a test fixture |
| Placing individual players by hand / scripted scenarios | possible later on top of `SandboxConfig`; not now |
| Saving sandbox configs to disk | D2 (persistence) |
| Team / tactics pickers, kit choice | D1 shell |
| Camera modes, sprites, HUD chrome | C2 / C3 / C4 |
| Substitutions, bench | C6 |
| Making the engine support squad sizes ≠ 11 | never — the arena is fixed by B1 |

---

## 2. Design

### 2.1 Absence is a marked slot, not a shorter array

B1 fixes the arena at 22 pitch entities and every per-side loop in the engine runs
`i = 0..10`. "Fewer players" therefore has to mean *some slots do not participate*, and
the engine already has that concept: `Entity::cards < 0` — sent off. It is honoured by
control selection (`IsEligibleForControl`), the CPU brain (`IsEligibleAi`,
`FindPassConeTeammate`), the keeper AI, and every set-piece taker, wall and marking loop.

Two places ignored it, which is why a red-carded player used to rejoin play:

| Place | Old behaviour | New behaviour |
|---|---|---|
| `PlacePlayersAtKickoff` (`match_engine.cpp`) | Re-places all 11 at their tactics cell | Off-pitch players park on the touchline |
| Per-slot loop in `ApplyTeamControls` (`movement.hpp`) | `ApplyOffBallDestination` walks them back on | Stopped and skipped |

The predicate is one function, `IsOffPitch(const Entity&)`, so there is one place to
change if the marker ever stops being `cards`. **No new field** is added to `Entity`:
that would change `HashState` and the ATTR wire format and force a re-pin of every
golden and corpus file for a debug feature.

Determinism: only states that already contain `cards < 0` behave differently. No golden
or corpus scenario contains a sending-off, so the pinned hashes do not move.

### 2.2 Two degenerate-side defects the mode exposes

A side with one player is legal input to the engine and must not deadlock it.

1. **Restart takers.** `PickRestartTaker` and `PlaceThrowInTaker` scan outfielders
   `i = 1..10`, skip `cards < 0`, and fall back to a hardcoded `base + 1` — an absent
   player, so the restart never gets taken. Both now fall back to `base` (the keeper),
   which is also the correct answer for a real match down to ten men on the pitch.
2. **The CPU keeper.** In `ApplyTeamControls` slot 0 only receives `ApplyGoalkeeperAI`
   when it is *not* that side's controlled slot; otherwise the outfield brain drives it
   and the keeper chases the ball upfield. With `controlled_slot = -1` every consumer
   short-circuits cleanly and the keeper AI takes over. `UpdateControlledPlayer` now
   releases the slot to `-1` in open play whenever a side has no eligible field player
   (`HasEligibleFieldPlayer`), so a restart that temporarily hands the keeper the ball
   does not leave him brain-driven afterwards. The release deliberately runs **before**
   the `ball_out_of_play` early-out: a keeper who has just taken a goal kick still holds
   possession, and that was exactly the path that walked him to the far end.

   What the keeper does after that is B9's business, not C1b's: the rest position is
   halfway between the ball and his own goal line, so a keeper-only side's keeper
   advances toward the halfway line when the ball is deep in the other half, and a
   restart he takes himself leaves him wherever the spot was until play resumes.

### 2.3 `SandboxConfig` → `MatchState`

`BuildSandboxState` is a pure function over a plain struct, in `src/app/mode/`, with **no
SDL and no ImGui** — so `harness_tests` can build it and assert on the result.

```
struct SandboxPlayer { PlayerAttrs attrs; uint8_t goalie_skill; };
struct SandboxConfig {
    uint8_t  outfield_count;      // 1..10
    bool     own_keeper;
    bool     attack_down;         // true: test side attacks high y (bottom goal)
    bool     spawn_as_attackers;  // which tactic roles the spawned men take
    uint32_t seed;
    uint8_t  game_length;         // 0..3, MatchClock semantics
    bool     reset_at_half_time;
    SandboxPlayer field[10];      // in spawn order, not by role
    SandboxPlayer keeper;         // test side, used only if own_keeper
    SandboxPlayer opponent_keeper;
};
int  SandboxFieldSlot(const SandboxConfig&, int k);   // pitch slot of spawn k
void BuildSandboxState(const SandboxConfig&, MatchState&);
void StartSandbox(MatchEngine&, const SandboxConfig&); // seed + build + load
```

**Which roles get spawned matters.** Off-ball destinations come from the tactic row for
`ordinal − 2`, so spawning three players as ordinals 2–4 gives three defenders who sit in
their own half — useless for testing a shot. `spawn_as_attackers` (default) instead takes
the *last* N outfield ordinals, so a small side plays forward. Attribute rows follow the
player, not the role: `field[k]` is "player k+1" in the dialog.

Construction order:

1. Sheets, kits and the tactics snapshot are copied from `MakeFictionalLeague()` teams 1
   and 2 so colours and the off-ball grid are real data; names are overridden.
2. Squad records get the configured attributes and generated names/shirts.
3. Present pitch slots: side 0 → slot 0 if `own_keeper`, slots `1..outfield_count`;
   side 1 → slot 11 (its keeper) only.
4. Absent slots: `cards = -1`, `sent_away = 1`, `visible = 0`, parked at
   `(kOffPitchParkX, kCentreSpotY)` well outside the playable box and outside every
   distance filter that scans teammates.
5. **Bootstrap is done here, not by the engine.** `BeginMatchIfNeeded` rolls
   `team_playing_up` from the RNG and only runs while `match_started == 0`, so the mode
   sets `match_started = 1`, writes the chosen direction, places the ball on the centre
   spot, enters `StartingGame` / `Stopped` with `stoppage_event_timer = 2`, and calls
   `PlacePlayersAtKickoff` itself. The first `Step` then continues an already-started
   match and leaves the direction alone.
6. Control: side 0 is human (`player_number = 1`) with the first present outfielder
   selected; side 1 is CPU with `controlled_slot = -1` (see §2.2).
7. RNG streams are seeded by `MatchEngine::Reset(seed)` and copied into the sheet, the
   same policy `SeedPlayableMatch` uses — the caller owns seeding (B1).

### 2.4 Shell

`AppPhase` grows `SandboxSetup`. The dialog lives in `ui_imgui/screens/sandbox_menu.cpp`
because wall 2 allows `imgui.h` only under `src/app/ui_imgui/` — `main.cpp`'s exemption is
temporary and not to be spent on a new screen. The **button** that opens it is one line in
`main.cpp`'s inline menu, which D1 retires along with the rest of that menu; growing the
`IUiBackend` / `AppModel` path for one button now would be building D1 by accident.

Restarting a mode is a **rebuild callback** captured when the mode starts
(`std::function<void(MatchEngine&)>`), so `R` serves MATCH and SANDBOX identically and
neither the hotkey nor the recorder needs to know which mode is running.

Half-time calls `SwapEnds`, which would silently flip the direction the operator chose.
In sandbox, `reset_at_half_time` (default on) triggers the same rebuild `R` does.

### 2.5 Input

| Action | Binding |
|---|---|
| P1 move | Arrow keys (WASD dropped) |
| P1 kick | Space |
| P2 move / kick | `I J K L` / Enter, Right Ctrl (unchanged) |
| Gamepad | Unchanged (pad 0 → P1, pad 1 → P2) |

`SDL_GetKeyboardState` is polled irrespective of focus, so both the device poll and the
`SDL_EVENT_KEY_DOWN` hotkeys are gated on `!ImGui::GetIO().WantCaptureKeyboard`. When
gated the engine is still stepped with an **empty** `MatchInput` rather than skipped, so
the recorder stays one input per tick and traces remain replayable.

---

## 3. Interfaces

| Path | Role |
|---|---|
| `core/match_state.hpp` | `IsOffPitch`, `kOffPitchParkX`, `ParkOffPitch` — the marker, in one place |
| `core/match_engine.cpp` | `PlacePlayersAtKickoff` honours it |
| `core/movement.hpp` | Team-controls loop skips it; `UpdateControlledPlayer` releases to `-1` |
| `core/set_pieces.hpp` | Taker fallbacks to the keeper |
| `src/app/mode/sandbox.hpp/.cpp` | `SandboxConfig`, `BuildSandboxState` — pure, no SDL |
| `src/app/ui_imgui/screens/sandbox_menu.cpp` | The dialog; the only file here that sees ImGui |
| `src/app/main.cpp` | Phase, rebuild callback, `R` hotkey, input gating |
| `render/match_renderer.cpp` | Off-pitch entities are not drawn |

Walls: core gains no dependency and no new state field; the mode adds no engine
branch — the engine cannot tell a sandbox match from a real one.

---

## 4. Work items

Ordered, each independently committable:

1. **Off-pitch rule** — `IsOffPitch` / `ParkOffPitch`; kickoff placement and the
   team-controls loop honour it. Test `test_offpitch_players`.
2. **Degenerate sides** — taker fallback to the keeper; `controlled_slot` release.
   Test `test_restart_one_man_side`.
3. **Builder** — `SandboxConfig` + `BuildSandboxState`. Test `test_sandbox_setup`.
4. **Input** — arrows only, ImGui gating; B10 keybinding table updated.
5. **Shell** — SANDBOX button, dialog screen, phase, rebuild callback, `R` / `Shift+R`,
   half-time reset, HUD legend and `SBX` tag.
6. **Renderer** — skip off-pitch entities in the draw list.
7. **Docs** — this file; `PLAN-CURRENTSTATE.md` row.

---

## 5. Tests and acceptance

**Automated** (§7 technique: unit tests plus the existing HashState pins)

- `tests/core/test_offpitch_players.cpp` — an off-pitch player survives
  `PlacePlayersAtKickoff` and 200 ticks of `Step` without moving; is never selected as
  controlled, pass target or restart taker; an on-pitch teammate still is.
- `tests/core/test_restart_one_man_side.cpp` — a side whose only survivor is its keeper
  is awarded a throw-in and a goal kick: the taker resolves to the keeper and play
  returns to `InProgress` inside the restart budget.
- `tests/harness/test_sandbox_setup.cpp` — the builder honours `outfield_count`,
  `own_keeper`, `attack_down` and `spawn_as_attackers`; absent slots are marked and
  outside the playable box; attributes reach the right squad slots and a higher speed
  attribute really is faster; two builds of the same config hash equal and stay equal
  over 120 stepped ticks (`HashState`); 5 000 ticks of scripted play leave the absent
  players off the pitch, resolve whatever restart is pending, and keep the lone keeper
  out of the far box and goal-side of the ball.
- Full `ctest`, including `golden_trace` and `corpus`: unchanged. No pinned scenario
  contains a sending-off, so the core rule must not move a single hash. If one moves,
  the change is wrong until proven otherwise.

**Manual demonstration (closes the part)**

1. SANDBOX → 3 outfielders, speed 15, finishing 15, attack down → kick off, dribble,
   shoot past the lone keeper; goal is scored and the restart is a kickoff.
2. `R` returns to the identical kickoff (same seed, same positions); `Shift+R` differs.
3. Typing in the attribute editor moves no player; arrows drive the controlled player.
4. Ball out for a throw-in to the keeper-only side: the keeper takes it and play resumes.
5. Transcripts still land in `traces/` and replay.

---

## 6. Open questions

- **`goalie_playing_or_out` in a one-keeper side.** The keeper AI is reached via the
  `controlled_slot = -1` path; whether the keeper should ever become selectable in
  sandbox (to test keeper control directly) is deferred until someone wants it.
- **Last-man fouls.** `IsLastManFoul` scans outfielders and skips absent ones, so with a
  keeper-only defence every foul reads as last-man and draws a red. Correct by the letter
  of the rule and harmless here; revisit if a sandbox scenario needs cards suppressed.
- ~~**Attribute scale.**~~ **Resolved by [B13](B13-amiga-oracle.md) / R2:** the range
  is 0–7, so the dead band this question was about does not exist. The observation that
  8–15 behaved identically for speed was the symptom; the cause was that 8–15 are not
  legal attribute values at all. The sandbox slider is now bounded at 7.
- **Config persistence.** In-memory only; belongs to D2 if it ever matters.
