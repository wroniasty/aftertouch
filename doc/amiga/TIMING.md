# TIMING.md

The frame, the order of work inside it, the match clock, and the stoppage state
machine that runs between the whistle and the restart.

This covers *when* things happen. What each routine does is in the subsystem
documents. The restart geometry the stoppage machine sets up is
[SETPIECES.md](SETPIECES.md).

> **Provenance.** `maingame` (asm:30970), `UpdateTime` (asm:26101) and
> `RunStoppageEventsAndSetAnimationTables` (asm:37250) are read directly. The
> `timeDelta` option table is a literal at asm:26526. The frame rate itself is *not*
> stated in the binary — it is inferred as 50 Hz from the Amiga PAL vertical blank,
> and §2 gives the arithmetic that corroborates it.

---

## 0. One-paragraph version

The game runs on the Amiga's level-3 vertical-blank interrupt at 50 Hz. `maingame`
is a flat loop that, once per frame, waits for the interrupt and then calls a fixed
sequence of subsystems in a fixed order: **clock, then input, then the ball, then
the players, then the keepers, then presentation**. The match clock is an
accumulator, not a counter: each frame subtracts a `timeDelta` chosen by the
match-length menu from a running total and, on underflow, adds 49 back and advances
the game clock by one second. Those four `timeDelta` values reproduce SWOS's
3 / 5 / 7 / 10 minute match lengths exactly. Minutes are stored as four decimal
digits packed into a longword, which is why the code compares the clock against
`$405` for half-time — that is literally "45". When the ball goes out or a foul is
given, `gameStatePl` flips from 100 to 101 and a separate stoppage machine takes
over: it runs a camera break, optionally saves a highlight and replays, positions
the ball and the players, and waits either for a fixed timer or for a player to
press fire.

---

## 1. The frame

`maingame` (asm:30970) is one loop with no sub-frame stepping. The per-frame body,
in source order from asm:41048:

| # | Call | What |
|---|---|---|
| 1 | `sub_108F90` | Frame setup |
| 2 | `sub_102C8E` | Camera |
| 3 | `sub_10C34E` | Sprite list open |
| 4 | **`UpdateTime`** | Match clock (§2) |
| 5 | **`Joystick_Wait`** | Read both joysticks (asm:37794) |
| 6 | **`UpdateBall`** | Ball physics ([BALL.md](BALL.md)) |
| 7 | `sub_108CAC` | Player sprite bookkeeping |
| 8 | `sub_109D04` | — |
| 9 | `sub_109F4A`, `sub_10A208`, `sub_10A28A`, `sub_109FF2` | Stats, statistics timeout, crowd |
| 10 | **`DoGoalKeeperSprites`** | Keeper sprite selection |
| 11 | `sub_10B070`, `sub_10B11A`, `sub_10B180`, `sub_10AEFE`, `sub_10B91A` | Scoreboard, banners, referee |
| 12 | `sub_10DA4E`, `sub_10CA72` | Input dispatch, bench |
| 13 | **`sub_11009A`** | **`UpdatePlayersAndBall`** — the 11-player sweep |
| 14 | `sub_10C34E` | Sprite list close |
| 15 | `sub_10C264` | Highlight capture, if flagged |

The ordering that matters for determinism:

- **The clock advances before anything reads it.** The goalmouth scatter
  ([BALL.md](BALL.md) §6) and several AI decisions key off `stoppageTimer`, so its
  value within a frame is fixed before any consumer sees it.
- **Input is sampled once, at step 5, into `TeamGeneralInfo` fields.** Nothing
  re-reads the hardware later in the frame. Both joysticks are latched together.
- **The ball moves before the players.** Every player in step 13 reacts to a ball
  position that already includes this frame's physics. There is no simultaneous
  update and no double-buffering of ball state.
- **Players are swept in a fixed order** — `players_loop` at asm:42087 walks
  `spritesTable` from index 0 to 10 — and each player writes its results
  immediately. A player at index 3 therefore sees the already-updated positions of
  players 0–2 and the stale positions of 4–10. This asymmetry is real and must be
  preserved; it is not an artefact of decompilation.

`Game_Starting_Wait` (asm:31442) is a separate loop used before kick-off, and the
paused state is handled by `sub_10EBC4` (asm:41074) which suspends the body but
keeps the interrupt ticking.

### The tick counter

`currentTick` is incremented by the level-3 interrupt handler and is used directly
as a cheap deterministic phase source. `CalculateIfPlayerWinsBall` tests
`btst #1, currentTick+1` (asm:35286) to gate the dribble speed impulse — i.e. the
touch only fires on two frames in four. This is a genuine gameplay dependency on
the frame counter, not a rendering detail.

---

## 2. The match clock

### The accumulator

`UpdateTime` (asm:26101) advances the clock only while `gameStatePl` is 100 (live
play) and a shootout is not running (asm:26155):

```
secondsSwitchAccumulator -= timeDelta
if secondsSwitchAccumulator < 0:
    secondsSwitchAccumulator += 49
    gameSeconds += 1
    if gameSeconds == 60:
        gameSeconds = 0
        advance gameTime by one minute
```

So a game-second takes **49 / timeDelta** frames, and `timeDelta` is picked once at
`InitGame` (asm:23724) from a four-entry table indexed by the match-length setting.

### The four match lengths

This is the derivation that pins the frame rate down.

| Setting | `timeDelta` | Frames per game-second | Frames per 45' half | Seconds at 50 Hz | Full match |
|---|---|---|---|---|---|
| 0 | 30 | 1.633 | 4 410 | 88.2 | **≈ 3 min** |
| 1 | 18 | 2.722 | 7 350 | 147.0 | **≈ 5 min** |
| 2 | 12 | 4.083 | 11 025 | 220.5 | **≈ 7 min** |
| 3 | 9 | 5.444 | 14 700 | 294.0 | **≈ 10 min** |

Four arbitrary-looking constants — 30, 18, 12, 9 — combined with an arbitrary-looking
reload of 49, land on 3 / 5 / 7 / 10 minutes. Those are exactly the options SWOS
offers. The chain that produces them assumes 50 Hz and one `UpdateTime` call per
frame; that it terminates on the four numbers a player actually sees is strong
evidence both assumptions hold.

### Clock storage

`gameTime` (asm:26522) is a longword holding four **decimal digits**, one per byte:
thousands, hundreds, tens, units. Incrementing carries manually at 10 (asm:26170).
This is why the comparisons look strange:

| Comparison | Line | Means |
|---|---|---|
| `gameTime == 1` | 26177 | minute 1 |
| `gameTime == $405` | 26188 | minute 45 |
| `gameTime == $406` | 26183 | minute 46 |
| `gameTime == $900` | 26201 | minute 90 |
| `gameTime == $10005` | 26243 | minute 105 |
| `gameTime == $10200` | 26254 | minute 120 |

The format exists so the scoreboard can render digits without dividing. Any
reimplementation should store minutes as an integer and format at the boundary —
but must reproduce the *comparison points*, which are the real content here.

---

## 3. Periods

| Event | Minute | Action |
|---|---|---|
| First whistle | 1 | `cseg_72019` — mark every fit player as having started |
| Half-time | 45 | Whistle, `EndFirstHalf` (asm:40756), `SetPlayersHalfPlayed` |
| Second half begins | 46 | Re-mark starters |
| Full time | 90 | Whistle; branch on the score (§below) |
| First extra period ends | 105 | Whistle, `sub_114492` (turn around) |
| Second extra period ends | 120 | Whistle; branch on the score again |

At the last second of any period the clock latches `gameSeconds = −1` and sets a
50-frame countdown (`word_10B82E`, asm:26322) — a one-second grace during which
play continues before the whistle actually sounds. That is where injury time comes
from: it is a flat fifty frames, not a computed allowance.

### Deciding the result

At full time (asm:26205) and again after extra time, the scores are compared. If
level, two settings gate what happens next:

1. If **away goals** are enabled (`word_12F6F4`) and this is the second leg
   (`word_12F6F2 == 1`), an aggregate comparison runs (asm:26216): each side's
   away-goal tally is doubled and added to the other leg's total, and the winner is
   whoever comes out ahead. That doubling is the away-goals rule expressed as
   integer arithmetic.
2. Otherwise, if extra time is enabled, `extraTimeState` is set and play continues.
   If not, and penalties are enabled, `StartPenalties` runs. If neither,
   `winngTeamPtr` is cleared and the match is a draw.

`SetPlayersHalfPlayed` (asm:26481) walks both squads and promotes each fit player's
`PlayerGame` $4E marker from 1 to 2 — the "played both halves" flag that career
mode reads for appearances. Players with $34 ≥ 2 in the team record (injured or
sent off) are skipped.

---

## 4. The stoppage machine

When play stops, `gameStatePl` becomes 101 and
`RunStoppageEventsAndSetAnimationTables` (asm:37250) drives the sequence. It is
staged by `breakCameraMode`:

| Stage | Meaning | Duration |
|---|---|---|
| 0 | Play live | — |
| 1 | Ball has come to rest; break begins | `stoppageEventTimer` = 50 frames, or **75 after a goal** (asm:37543) |
| 2 | Positions being taken up; ball placed | until players are set |
| −1 | Restart armed | until fire, or the AI's timeout |

Stage 1 will not begin until the ball's horizontal deltas are both zero
(asm:37535) — the break waits for the ball to actually stop, it is not triggered by
the whistle. The exception is a keeper holding the ball (`gameState` 3), which
skips the wait.

During stage 1 the auto-replay and highlight-save hooks fire (asm:37567), gated on
`goalCameraMode` — so replays are offered for goals and near-misses, not for throw-ins.

### Who ends the stoppage

- **A human side** ends it by pressing fire. The check accepts fire from either
  joystick and from either coach slot (asm:37320–37350).
- **A CPU-versus-CPU match** uses timers instead (asm:45380): 600 frames (12 s) on
  the half-time and full-time result screens, 350 frames (7 s) otherwise, measured
  on `stoppageTimerTotal`.
- **Kick-off after a goal** has its own gate: `PrepareForInitialKick` will not run
  until `stoppageTimerActive` reaches 750 frames — **15 seconds** — for a
  CPU-controlled side (asm:37529).

While stopped, players still move but at reduced speed: `UpdatePlayerSpeed`
(asm:35509) scales speed by `stoppageTimerTotal × 4`, capped at 100 %, during
states $17/$18, and subtracts `stoppageTimerTotal × 32` during states $1D/$1E. Both
are ramps that let players jog into position and then settle.

Note also that outfield players use a *different, tighter* speed table when play is
stopped — `playerSpeedsGameStopped` spans 1136–1248 against 928–1250 for live play
([MOVEMENT.md](MOVEMENT.md) §3). Everyone walks back at nearly the same pace
regardless of ability, which is why the fastest player has no advantage getting to a
throw-in.

---

## 5. Constants quick reference

| Symbol | Line | Value | Meaning |
|---|---|---|---|
| Frame rate | — | 50 Hz | Inferred; corroborated in §2 |
| `timeDelta` options | 26526 | 30, 18, 12, 9 | Match length 3/5/7/10 min |
| Accumulator reload | 26163 | 49 | Frames per game-second = 49/`timeDelta` |
| Half-time | 26188 | minute 45 | |
| Full time | 26201 | minute 90 | |
| Extra periods end | 26243, 26254 | minutes 105, 120 | |
| Injury-time grace | 26322 | 50 frames | Flat, per period |
| Break after normal stop | 37543 | 50 frames | |
| Break after a goal | 37546 | 75 frames | |
| CPU restart wait, result screens | 45383 | 600 frames | |
| CPU restart wait, other | 45392 | 350 frames | |
| CPU kick-off wait | 37529 | 750 frames | |
| Dribble impulse phase | 35286 | `currentTick` bit 1 | 2 frames in 4 |

---

## 6. What this resolves, and what still needs measurement

Confirmed:

- ✓ Single fixed-order call chain per frame; no sub-stepping, no interpolation.
- ✓ Ball updates before players; players update in index order and see each other's
  partial results.
- ✓ Input latched once per frame.
- ✓ Clock is an accumulator with reload 49 and four `timeDelta` settings.
- ✓ Those settings reproduce 3/5/7/10-minute matches at 50 Hz.
- ✓ Minutes stored as packed decimal digits; the comparison points are known.
- ✓ Injury time is a flat 50 frames.
- ✓ Away-goals aggregate is implemented as a doubling.
- ✓ CPU-versus-CPU restarts are timer-driven with three distinct timeouts.

Open (measurement targets, [../LEGACY.md](../LEGACY.md) §15):

- Whether `timerDifference` (asm:32002) ever makes `UpdateTime` advance by more than
  one tick — i.e. whether the original has frame-skip catch-up under load. It is
  read in the last-minute path (asm:26330) but the main path uses `timeDelta`
  directly. If it does catch up, matches are wall-clock-stable but frame-count
  variable, which would be a determinism hazard worth knowing about.
- Whether NTSC Amigas ran at 60 Hz with the same tables, making matches 17 % shorter.
  Nothing in the binary selects on this.
- The exact meaning of the ten `gameState` values $15–$1E; they are period and
  result screens but are only partially separated here.

---

## 7. Guidance for the reimplementation

- **Fix the tick at 50 Hz inside `at_core` and never vary it.** Presentation may
  interpolate; the simulation must not. Every constant in every document here is
  per-frame.
- **Reproduce the call order exactly**, including the fact that players see partial
  state. This is the single easiest place to introduce a divergence that only shows
  up hundreds of frames later in a trace comparison.
- **Keep the accumulator clock.** Storing elapsed frames and dividing is equivalent
  in the limit but not frame-for-frame, and the half-time boundary is a
  discontinuity where that matters.
- **Store minutes as an integer**, format for display, but keep the comparison
  points as named constants. Do not port the packed-decimal representation.
- **Treat `currentTick` as simulation state**, not as a rendering counter. It is
  read by gameplay code and belongs in the deterministic core and in the trace.
- **Model the stoppage machine as an explicit state machine with the timer values
  above.** The 50/75-frame split and the 350/600/750 CPU timeouts are what make a
  CPU-versus-CPU match take the length it does, and they are cheap to get right now
  and expensive to fit later.
