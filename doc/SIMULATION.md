# SIMULATION.md

The match as a whole: the tick loop, the state machine that sequences kick-offs and
restarts, the clock, the laws of the game as SWOS actually implements them (out of
play, fouls, cards, injuries), how a period ends, and how matches you *don't* play
get their scores. Traced through the reference DOS port in
[../reference/swos-port/](../reference/swos-port/) and its annotated disassembly
([swos/swos.asm](../reference/swos-port/swos/swos.asm)).

This is the frame that [MOVEMENT.md](MOVEMENT.md), [AI.md](AI.md),
[CONTROL.md](CONTROL.md), [SHOOTING.md](SHOOTING.md) and
[AFTERTOUCH.md](AFTERTOUCH.md) all run inside. It answers most of
[LEGACY.md](LEGACY.md) §6 (pitch types), §11 (match flow and rules) and §12
(replays).

> **Provenance.** Decompiled 68000/x86 plus a commented IDA dump. Some of this — the
> clock, the statistics, the pitch tables — survives in the port as clean C++ and is
> quotable directly. The off-screen result generator (§10) is *not* decoded; it is
> described only as far as I could verify. Read to understand the design; write our
> own code ([LEGACY.md](LEGACY.md) §15, §17).

> **Second oracle.** [amiga/TIMING.md](amiga/TIMING.md) traces the frame, the clock
> and the stoppage machine through the Amiga original. It confirms the clock
> arithmetic, the period boundaries and the away-goals rule exactly — and it shows
> that **§1.2's wall-clock leak is a port artefact, not original behaviour**. It also
> independently corroborates §10's most important structural claim: the off-screen
> result generator is not in the match code at all. See §14.

---

## 0. One-paragraph version

A match is one loop. Each iteration reads input, updates one team's decisions, moves
the ball, moves all 22 players, then draws — and the *only* thing that varies with
real time is a tick delta used by timers, never by the simulation. Two variables
sequence everything: `gameStatePl` (in progress / stopped / waiting) and `gameState`
(34 named situations from `kick-off` through `throw-in back left` to
`penalties`). The clock is a fixed number of game-seconds per frame, chosen so that
90 displayed minutes take 3, 5, 7½ or 10 real minutes; the last minute of each
period **keeps extending** while the ball is in a penalty area or an attack is live.
Going out of play is classified by geometry plus a single variable, `lastTeamPlayed`.
Fouls need contact within ~5.7 units of the opposing *controlled* player, near the
ball, from behind or off the ball; the card that follows is rolled against a
per-match referee strictness drawn once at kick-off from a table indexed by match
length. Replays are recorded **output**, not input, so they cannot be re-simulated.
And matches you don't watch get a score from a separate routine in the menu code
that has nothing to do with the match engine at all.

---

## 1. The frame

### 1.1 The loop

The body of `GameLoop`
([swos.asm:7659-7687](../reference/swos-port/swos/swos.asm#L7659-L7687)), in order:

```
ReadTimerDelta              ; how many ticks since last iteration -> lastFrameTicks
(pause / stats / replay handling, key polling)
MoveCamera / ScrollToCurrent
SaveCoordinatesForHighlights
PlayEnqueuedSamples
UpdateTime                  ; §3 the clock, period ends
UpdateAndApplyTeamControls  ; §AI — one team's decisions this frame
UpdateBall                  ; ball physics; calls CheckIfBallOutOfPlay (§5)
MovePlayers                 ; integrate all 22 sprites
UpdateReferee
UpdateCornerFlagSprite / SpinBig_S_Sprite / ManageAdvertisements
DoGoalkeeperSprites
UpdateControlledPlayerNumbers / MarkPlayer / SetCurrentPlayerName
UpdateBookedPlayerNumberSprite / ShowResult
DrawAnimatedPatterns / DrawSprites / DrawBenchAndSubsMenu
BenchCheckControls
UpdateAndDrawStatistics     ; §7
SaveCoordinatesForHighlights / SaveHighlightScene
Flip
```

Three things are worth noticing about that order:

- **`UpdateBall` runs before `MovePlayers`.** The ball is integrated first, then the
  players, so within one tick players react to the ball's *new* position.
- **Decision and integration are separate.** `UpdateAndApplyTeamControls` only sets
  destinations, speeds and flags; `MovePlayers` does the arithmetic
  ([MOVEMENT.md](MOVEMENT.md) §1).
- **Only one team's decisions run per frame.** `UpdateAndApplyTeamControls`
  alternates ([AI.md](AI.md) §1), so a team's AI, proximity classification and
  off-ball re-targeting all run at half the frame rate. Ball and player *motion* run
  every frame for both.

### 1.2 The tick model, and where wall-clock leaks in

`currentGameTick` is advanced by the frame pacer, not the loop
([timer.cpp:145-158](../reference/swos-port/src/video/timer.cpp#L145-L158)):

```cpp
int framesElapsed = std::max(std::lround(ticksElapsed / m_ticksPerFrame), 1l);
framesElapsed = std::min(framesElapsed, 6);
swos.currentTick += framesElapsed;
if (!isGamePaused())
    swos.currentGameTick += framesElapsed;
```

[`ReadTimerDelta`](../reference/swos-port/swos/swos.asm#L18874-L18888) then publishes
`lastFrameTicks = currentGameTick − lastGameTick`, clamped to at least 1.

**`lastFrameTicks` is read by exactly five things**, and they are all timers — the
match clock, the referee's walk timer, the result display, the menu fire counter:

| Consumer | File |
|---|---|
| game clock and end-of-period counter | [gameTime.cpp:67, 84](../reference/swos-port/src/game/gameTime.cpp#L67) |
| referee timer | [referee.cpp:111](../reference/swos-port/src/game/referee.cpp#L111) |
| result display timer | [result.cpp:130](../reference/swos-port/src/game/result.cpp#L130) |
| menu long-fire timer | [menuControls.cpp:115](../reference/swos-port/src/menus/engine/menuControls.cpp#L115) |

Nothing in movement, ball physics, AI or collision reads it. So:

- **The simulation is fixed-step per loop iteration.** One iteration = one tick of
  everything physical, always.
- **The clock is wall-clock-driven.** On a machine that cannot keep up, the match
  runs *shorter in simulation steps* — the clock skips ahead rather than the world
  slowing down. On a 70 fps machine the two coincide exactly (`lastFrameTicks == 1`).
- **`currentGameTick` is also used as a dice source** by the goalkeeper, the card
  roll and several AI branches ([AI.md](AI.md) §4.7, §5). A lagging frame therefore
  *skips* dice values. Any faithful reimplementation should decide this once: derive
  the clock from a tick counter, and keep the "dice" on that same counter.

### 1.3 Determinism inventory

Everything that can vary between two runs of the same inputs:

| Source | Deterministic? |
|---|---|
| `Rand()` / `Rand2()` | yes — 256-byte table, 8-bit seed, two streams ([AI.md](AI.md) §6) |
| `currentGameTick` bits used as dice | **only if the frame rate holds** |
| Player/ball motion, AI decisions | yes |
| Pitch type, referee strictness, coin tosses | drawn from `Rand()` at kick-off (§4) |

That middle row is the one thing standing between SWOS and full replay determinism.

---

## 2. The state machine

Two words drive everything.

**`gameStatePl`** — the coarse mode:

| Value | Name | Meaning |
|---|---|---|
| 100 | `ST_GAME_IN_PROGRESS` | the ball is live |
| 101 | `ST_STOPPED` | a break is being set up or played out |
| 102 | `ST_WAITING_ON_PLAYER` | waiting for a player to do something |

**`gameState`** — the specific situation
([swos.asm:1400-1435](../reference/swos-port/swos/swos.asm#L1400-L1435); the C++
`GameState` enum at
[swos.h:567-594](../reference/swos-port/src/swos/swos.h#L567-L594) omits several):

| # | State | # | State |
|---|---|---|---|
| 0 | players to initial positions | 15–17 | throw-in forward / centre / back **right** |
| 1–2 | goal out left / right | 18–20 | throw-in forward / centre / back **left** |
| 3 | keeper holds the ball | 21 | starting game |
| 4–5 | corner left / right | 22 | camera going to showers |
| 6–8 | free kick left 1/2/3 | 23–24 | going to half-time / players going to shower |
| 9 | free kick centre | 25–26 | result on half-time / after the game |
| 10–12 | free kick right 1/2/3 | 27–28 | first extra starting / ended |
| 13 | foul (no wall) | 29–30 | first half ended / game ended |
| 14 | penalty | 31 | penalties (shoot-out) |

Each restart writes four things together — the restart spot, the camera facing, and
the direction mask ([MOVEMENT.md](MOVEMENT.md) §5):

```
gameState        = <the state>
foulXCoordinate  = <restart x>       foulYCoordinate = <restart y>
cameraDirection  = <0..7>
playerTurnFlags  = <8-bit allowed-direction mask>
gameStatePl      = ST_STOPPED
```

then calls `StopAllPlayers` and resets the stoppage timers. `lastTeamPlayedBeforeBreak`
records who restarts. The off-ball AI switches to the tactic's
`ballOutOfPlayTactics` variant for the duration ([AI.md](AI.md) §3.3).

---

## 3. The clock

[gameTime.cpp](../reference/swos-port/src/game/gameTime.cpp) is one of the cleanest
files in the port and can be read directly.

### 3.1 Rate

```cpp
static const int kGameLenSecondsTable[] = { 30, 18, 12, 9 };   // by gameLengthInGame 0..3
m_timeDelta = kGameLenSecondsTable[swos.gameLengthInGame];
...
m_secondsSwitchAccumulator -= m_timeDelta;
if (m_secondsSwitchAccumulator < 0) {
    m_secondsSwitchAccumulator += amigaModeActive() ? 49 : 70;
    m_gameSeconds += swos.lastFrameTicks;
    if (m_gameSeconds >= 60) { m_gameSeconds = 0; bumpGameTime(); }
}
```

`m_timeDelta` game-seconds pass per 70 frames — i.e. per real second at 70 fps. A
match is 90 **displayed** minutes = 5400 game-seconds, so:

| `gameLengthInGame` | delta | frames for 90′ | real time (PC, 70 fps) |
|---|---|---|---|
| 0 | 30 | 12 600 | **3:00** |
| 1 | 18 | 21 000 | **5:00** |
| 2 | 12 | 31 500 | **7:30** |
| 3 | 9 | 42 000 | **10:00** |

Those are exactly SWOS's four advertised match lengths, and this is where they come
from. In Amiga mode the accumulator refills with 49 at 50 fps, giving very slightly
shorter real durations.

The clock does not advance while `gameStatePl != kInProgress`, nor during a penalty
shoot-out — so **stoppages are genuinely free time**.

### 3.2 Periods and injury time

Period ends are keyed on the displayed minute
([gameTime.cpp:242-253](../reference/swos-port/src/game/gameTime.cpp#L242-L253)):
**45** → end first half, **90** → end second half, **105** → end first extra,
**120** → end second extra.

Entering the last minute sets `m_gameSeconds = -1` and
`m_endGameCounter = 55` (PC) / 50 (Amiga). While that counter runs down the whistle
is deferred, and `prolongLastMinute()` can keep resetting it
([gameTime.cpp:178-190](../reference/swos-port/src/game/gameTime.cpp#L178-L190)):

```cpp
auto ballInsidePenaltyArea = ballY <= 216 || ballY > 682;
auto attackingTeam  = ballY > 449 ? &topTeamData : &bottomTeamData;
auto attackInProgress = lastTeamPlayed == attackingTeam;
return ballInsidePenaltyArea || attackInProgress;      // true -> keep playing
```

**That is SWOS's entire injury-time model**, and it is a good one: the half will not
end while the ball is in either box, or while whoever last touched it is attacking.
It also means a period can, in principle, be prolonged indefinitely.

Play is also never stopped mid-air: the whistle only fires once
`gameStatePl == kInProgress` is false or the prolong condition clears.

### 3.3 Per-player half tracking

At minute 1 and minute 46 every uncarded player gets `halfPlayed = 1`; at the end of
each half those become `halfPlayed = 2`
([gameTime.cpp:286-300](../reference/swos-port/src/game/gameTime.cpp#L286-L300)).
This is what feeds appearance records in a career.

---

## 4. What a match rolls at kick-off

Before the first whistle, `InitGameRestoreTeams`
([swos.asm:96274](../reference/swos-port/swos/swos.asm#L96274)) draws several values
that stay fixed for the whole match:

| Rolled | How | Effect |
|---|---|---|
| `teamPlayingUp` | `(rand() & 1) + 1` | which team defends the top goal |
| `teamStarting` | `(rand() & 1) + 1` | who kicks off |
| **pitch type** | weighted table (§4.1) | ball friction and bounce (§4.2) |
| **`playerCardChance`** | `rand() & 0x1E` → index 0–15 into a per-match-length table | the referee's strictness for this match (§6.3) |
| `gameRandValue` | `rand()` | general per-match salt |

The per-match referee strictness is the nicest of these: SWOS gives every match a
referee, once, and then never re-rolls it.

### 4.1 Pitch type

Seven types — `Frozen, Muddy, Wet, Soft, Normal, Dry, Hard`
([pitch.cpp:23-33](../reference/swos-port/src/game/pitch/pitch.cpp#L23-L33)). Either
chosen explicitly, or rolled from a flat table, or rolled from a **12 × 7 seasonal**
table ([pitch.cpp:226-241](../reference/swos-port/src/game/pitch/pitch.cpp#L226-L241)):

| Month | Frozen | Muddy | Wet | Soft | Normal | Dry | Hard |
|---|---|---|---|---|---|---|---|
| Jan | 30 | 20 | 30 | 20 | 0 | 0 | 0 |
| Feb | 20 | 30 | 20 | 20 | 10 | 0 | 0 |
| Mar | 10 | 30 | 10 | 30 | 20 | 0 | 0 |
| Apr | 0 | 10 | 10 | 30 | 40 | 10 | 0 |
| May | 0 | 0 | 0 | 10 | 40 | 40 | 10 |
| Jun | 0 | 0 | 0 | 0 | 40 | 40 | 20 |
| Jul | 0 | 0 | 0 | 0 | 30 | 30 | 40 |
| Aug | 0 | 0 | 0 | 0 | 50 | 30 | 20 |
| Sep | 0 | 0 | 0 | 20 | 40 | 30 | 10 |
| Oct | 0 | 20 | 0 | 40 | 30 | 10 | 0 |
| Nov | 10 | 30 | 10 | 40 | 10 | 0 | 0 |
| Dec | 20 | 30 | 20 | 30 | 0 | 0 | 0 |

Non-seasonal default: `5, 5, 10, 20, 30, 20, 10`.

### 4.2 What the pitch actually changes

Three numbers ([game.cpp:1386-1401](../reference/swos-port/src/game/game.cpp#L1386-L1401)):

| Table | Frozen | Muddy | Wet | Soft | Normal | Dry | Hard |
|---|---|---|---|---|---|---|---|
| `pitchBallSpeedFactor` (PC) | −3 | +4 | +1 | 0 | 0 | −1 | −1 |
| `pitchBallSpeedFactor` (Amiga) | −2 | +2 | +3 | 0 | 0 | −1 | −1 |
| `ballSpeedBounceFactor` | 24 | 80 | 80 | 72 | 64 | 40 | 32 |
| `ballBounceFactor` | 88 | 112 | 104 | 104 | 96 | 88 | 80 |

The first is a **friction offset**
([ball.cpp:239-290](../reference/swos-port/src/game/ball/ball.cpp#L239-L290)):

```
friction = kBallGroundConstant;                 // 13 PC, 16 Amiga
if (nobody has the ball)  friction += pitchBallSpeedFactor;
if (ball.z != 0)          friction = kBallAirConstant;   // 4 PC, 10 Amiga
ball.speed -= friction;
```

So a frozen pitch drops ground friction from 13 to 10 and the ball runs away from
everyone; mud raises it to 17. Note the guard: **the pitch factor only applies when
nobody has the ball** — a dribbled ball ignores the surface entirely. That is the
mechanical form of [LEGACY.md](LEGACY.md) §6, with values.

---

## 5. Out of play

`UpdateBall` detects that the ball has left the field and calls
[`CheckIfBallOutOfPlay`](../reference/swos-port/swos/swos.asm#L110997) to classify
what happens. It is pure geometry plus one variable — `lastTeamPlayed`.

### 5.1 Pitch geometry

| Feature | Coordinates |
|---|---|
| Playable area | x ∈ [81, 590], y ∈ [129, 769] |
| Centre spot | (336, 449) |
| Goal mouth | x ∈ [303, 367] (65 units wide) |
| Crossbar | z ≤ 15 |
| Penalty areas | x ∈ [193, 478], y ≤ 216 (top) / y ≥ 682 (bottom) |
| Goal areas | x ∈ [265, 406], y ≤ 159 / y ≥ 739 |
| Penalty spots | (336, 187) and (336, 711) |
| Free-kick band | y ∈ [216, 331) top, y ∈ (567, 682] bottom |
| "Goal attempt" window | x ∈ [240, 431] |

### 5.2 Goal

`z ≤ 15` **and** `x−1 ∈ [302, 366]` **and** the ball past the goal line. The scoring
team is determined by which half the ball is in, then `GoalScored` runs, the goal
camera engages, and own goals are detected from `lastPlayerPlayed`. A goalkeeper is
explicitly excluded from being credited with an own goal
([swos.asm:110997-111010](../reference/swos-port/swos/swos.asm#L110997-L111010) — the
disassembler's comment is *"don't allow goalkeeper to be own goal scorer... why?"*).

### 5.3 Near miss

If the ball crosses the byline with `speed ≥ 768`, `x ∈ [290, 381]` and `z + 2 ≤ 25`,
the crowd sighs and the commentary fires a near-miss line
([swos.asm:111205-111245](../reference/swos-port/swos/swos.asm#L111205-L111245)). No
gameplay effect — but it is why SWOS *feels* like it knows what a near miss is.

### 5.4 Corner, goal kick, throw-in

Beyond a goal line: **corner if `lastTeamPlayed` is the defending team, goal kick
otherwise**. Left/right by `x < 336`. Restart spots are hard-coded:

| Restart | Spot | `cameraDirection` | `playerTurnFlags` |
|---|---|---|---|
| Corner, upper left | (86, 134) | 2 | `0x1C` |
| Corner, upper right | (585, 134) | 6 | `0x70` |
| Corner, lower left | (86, 764) | 2 | `0x07` |
| Corner, lower right | (585, 764) | 6 | `0xC1` |
| Goal kick, upper right | (396, 154) | 4 | `0x7C` |

Beyond a touchline: a **throw-in** to the opponent of `lastTeamPlayed`, x snapped to
81 or 590, and one of six states chosen from the y third (`< 342`, `342…555`,
`≥ 556`) and the throwing team's direction — forward / centre / back, left / right
([swos.asm:111391-111480](../reference/swos-port/swos/swos.asm#L111391-L111480)).
`cornersWon` is the only statistic incremented here.

---

## 6. Fouls, cards, injuries

[`PlayerTacklingTestFoul`](../reference/swos-port/swos/swos.asm#L106953) runs when a
sliding player meets the **opposing team's controlled player**.

### 6.1 Contact

1. Squared distance between the two ≤ **32** (≈ 5.7 units).
2. The victim is not a goalkeeper (keepers just slow the tackler) and is inside
   x ∈ [81, 590], y ∈ [129, 769].
3. The tackler's speed is cut to **¼** (`>>2`, minimum 1) and `PlayerTackled` runs on
   the victim (§6.4).
4. If the victim's `ballDistance > 800` (≈ 28 units) — nowhere near the ball —
   **no foul**. It is just a collision.

### 6.2 Is it a foul?

```
if (tackleState == 0)                            -> FOUL      // never touched the ball
else if (tackleState == TS_GOOD_TACKLE)          -> no foul
else if (|tacklerDirection - victimDirection| <= 1) -> FOUL   // from behind
else                                                -> no foul
```

"Both facing the same way, within one octant" is SWOS's definition of a tackle from
behind. `foulsConceded` is incremented for the offending team.

### 6.3 The card

Cards are skipped entirely in training games and when `cardsDisallowed`. Otherwise:

**Last-man test.** The code finds, among the *tackler's* outfield team-mates
(excluding the keeper and the tackler), the one closest to their own goal centre —
(336, 129) or (336, 769). If the **fouled player** is closer to that goal than any of
them, he was through on goal, and the odds invert:

| Situation | `Rand() < 32` (12.5 %) | otherwise |
|---|---|---|
| Ordinary foul, or in the penalty area | **red** | yellow |
| Denying a clear chance (last man) | yellow | **red** |

So a professional foul is an 87.5 % red card. In 1994.

**Referee strictness.** Before any of that, a gate
([swos.asm:107196-107202](../reference/swos-port/swos/swos.asm#L107196-L107202)):

```
if (((currentGameTick & 0x1E) >> 1) >= playerCardChance)  -> no card
```

`playerCardChance` is the once-per-match draw from §4, out of 16, from a table
indexed by match length
([swos.asm:245655-245663](../reference/swos-port/swos/swos.asm#L245655-L245663)):

| Match length | 16 possible strictness values |
|---|---|
| 3 min | `4,4,4,4,5,5,5,5,6,6,6,7,7,8,9,10` |
| 5 min | `2,2,3,3,3,3,3,3,4,4,4,4,4,5,5,6` |
| 7½ min | `1,1,2,2,2,2,2,2,3,3,3,3,3,4,4,4` |
| 10 min | `1,1,1,1,1,2,2,2,2,2,2,2,2,2,3,3` |

A 3-minute match therefore books people ~4× more readily *per foul* than a 10-minute
one — the tables are tuned so cards per match stay roughly constant regardless of
length. The same trick is used for injuries (§6.4). This is a genuinely good design
idea and worth stealing outright.

**A CPU bias.** [`TryBookingThePlayer`](../reference/swos-port/swos/swos.asm#L107255)
refuses to book a CPU-team player who already has a card, or whose team has no
injury allowance left. The disassembler's comment — *"don't book cheating scum
CPU"* — is not editorialising; the branch is real. Worth reproducing only
deliberately.

### 6.4 Restart

[`TestFoulForPenaltyAndFreeKick`](../reference/swos-port/swos/swos.asm#L107571):

- Foul inside the penalty area → `ST_PENALTY`, ball on the spot (336, 187 / 711),
  `playerTurnFlags` restricted to the three directions facing the goal.
- Foul in the band just outside the area (y ∈ [216, 331) / (567, 682]) →
  a **free kick**, with one of seven states chosen by the foul's x against the
  boundaries `153, 261, 309, 362, 410, 518` — `LEFT1/2/3`, `CENTER`, `RIGHT1/2/3`.
  Those states drive the wall and player arrangement.
- Anywhere else → `ST_FOUL`: restart from the spot, no wall.

### 6.5 Injuries

[`PlayerTackled`](../reference/swos-port/swos/swos.asm#L107823):

1. Base gate `Rand() & 3` — **25 %** of fouls are even considered.
2. The team must have injury allowance left (`teamNNumAllowedInjuries`).
3. `Rand()` against `kTackleInjuryProbability` = `48, 28, 20, 14` by match length
   (or `96, 57, 41, 28` if the player is already injured — **double** the chance).
4. Severity by `Rand()` against `kInjuryLevels` = `42, 7, 5, 4, 3, 2, 1`
   (or `14, 15, 12, 9, 7, 5, 2` when already injured).

`injuryLevel / 32` then indexes the speed handicap in
[MOVEMENT.md](MOVEMENT.md) §2.2 — up to −288, about 23 % of a top player's speed.

---

## 7. Statistics

Seven counters per team, in `TeamStatsData`
([swos.h:212-221](../reference/swos-port/src/swos/swos.h#L212-L221)): possession,
corners won, fouls conceded, bookings, sendings off, goal attempts, on target.

**Possession is a tick count**
([stats.cpp:74-105](../reference/swos-port/src/game/stats.cpp#L74-L105)):

```cpp
if (gameStatePl == kInProgress)
    lastTeamPlayed->teamStatsPtr->ballPossession++;
```

So "possession" means *share of live ticks since your team last touched the ball* —
a loose ball counts for whoever kicked it last, and stoppages count for nobody. The
percentage is computed at display time from the two counters.

**Goal attempts** are latched once per shot: when the ball enters the goalkeeper
area travelling toward the goal, the striking team last played, nobody is carrying
it, and the projected crossing point `strikeDestX` falls in [240, 431] →
`goalAttempts++`; if it falls in the goal itself [303, 367] → `onTarget++` as well.

---

## 8. Ending a period

[gameTime.cpp:127-215](../reference/swos-port/src/game/gameTime.cpp#L127-L215) plus
the state setters in the asm.

| Minute | Handler | Effect |
|---|---|---|
| 45 | `endFirstHalf` | `ST_FIRST_HALF_ENDED`, `stoppageEventTimer = 100` |
| 90 | `endSecondHalf` | resolve the tie (below) |
| 105 | `endFirstExtraTime` | swap ends, `ST_FIRST_EXTRA_ENDED` |
| 120 | `endSecondExtraTime` | resolve, then penalties or finish |

**Ends are swapped** at half-time and between extra-time periods:
`teamPlayingUp = 3 − teamPlayingUp`, likewise `teamStarting`
([swos.asm:103781-103786](../reference/swos-port/swos/swos.asm#L103781-L103786)).
Extra time **re-rolls both** rather than swapping
([swos.asm:103925-103940](../reference/swos-port/swos/swos.asm#L103925-L103940)).

**Tie resolution** at 90 and 120 minutes:

```cpp
if (team1Total == team2Total) {
    total1 = statsTeam1Goals + 2 * team1GoalsFirstLeg;   // away goals count double
    total2 = statsTeam2Goals + 2 * team2GoalsFirstLeg;
    bool tied = !secondLeg || playing2ndGame != 1 || total1 == total2;
    if (tied) {
        if (extraTimeState)     startFirstExtraTime();
        else if (penaltiesState) startPenalties();
        else                     winner = nullptr;       // draw
        return;
    }
}
winner = total1 > total2 ? topTeam : bottomTeam;
```

That second computation is the **away-goals rule**: on a level aggregate in a second
leg, first-leg goals (scored away) are weighted double. Whether extra time and
penalties happen at all is a per-competition setting.

The penalty shoot-out runs as `ST_PENALTIES` with `playingPenalties` set; the clock
is frozen, all players are made eligible (`cards` and `sentAway` are cleared —
*"this is why it's possible for a red carded player to shoot penalties"*), and each
team's shooter index walks backwards from 11
([swos.asm:100500-100560](../reference/swos-port/swos/swos.asm#L100500-L100560)).

---

## 9. Replays and highlights

[LEGACY.md](LEGACY.md) §12 asks what a replay actually stores. The answer is:
**rendered state, not input.**

Per frame ([ReplayDataStorage.cpp:65-79](../reference/swos-port/src/replays/ReplayDataStorage.cpp#L65-L79)):
next/previous frame offsets, camera x and y, both scores packed into one dword, and
the game time. Then, per visible sprite, `(spriteIndex, x, y)`. Statistics and sound
effects are interleaved as tagged records.

Consequences worth being explicit about:

- A replay **cannot be re-simulated** — there is nothing to re-simulate from. It is
  a movie in coordinates.
- It is therefore large: the port reserves 4.5 M dwords and 39 000 elements per
  saved highlight scene.
- Recording inputs plus the RNG seed instead would be smaller by orders of
  magnitude, would enable headless re-simulation and league play, and is only
  possible if §1.2's wall-clock leak is closed first.

---

## 10. Matches you don't play

This is a different system entirely, living in the menu/career code rather than the
match engine.

`CalculateViewResult` ([swos.asm:32548-32948](../reference/swos-port/swos/swos.asm#L32548))
produces `lastTeam1ViewResultGoals` / `lastTeam2ViewResultGoals` for a fixture the
player is viewing rather than playing. Its caller caches the result, so a given
fixture is scored once
([swos.asm:32478-32496](../reference/swos-port/swos/swos.asm#L32478-L32496)).

What I can state with confidence:

- It uses **`Rand2()`**, the second RNG stream — so generating other results does not
  perturb the match engine's stream.
- It reads both `TeamFile`s directly, walking `PlayerFile` records (38-byte stride)
  and their skill bytes, through a lineup indirection (`playerNumbers`, or
  `currentMatchPlayers` for the career team).
- Two data tables drive it: a 6 × 8 table of small counts (`0…4`, rising with some
  team-strength index) and a set of 8-byte sequences of player-slot indices. Both are
  sampled with `Rand2() & 7`.
- Once the score is known, a separate routine
  ([swos.asm:33160](../reference/swos-port/swos/swos.asm#L33160)) **picks the
  scorers**: it loads the team's tactic, follows it to a positions table, and rolls
  `Rand2()` against per-position weights once per goal. A `Rand2() >= 248` branch
  (~3 %) takes a different path — plausibly an own goal or unattributed scorer.

**What I could not verify:** the actual scoring model — how team strength is reduced
to the table index, what the two tables mean, and how the halves are combined. The
routine is 400+ lines of fully unnamed decompiled code (`cseg_*`/`dseg_*`) with no
comments. I have deliberately not guessed. If off-screen results matter to us, this
is a bounded but real reverse-engineering task, and it is entirely separable from
the match engine — a reimplementation can substitute any model here without
affecting played matches.

---

## 11. Constants quick reference

| Symbol / value | Value | Meaning |
|---|---|---|
| Frame rate | 70 (PC) / 50 (Amiga) | `kTargetFpsPC`, `kTargetFpsAmiga` |
| `framesElapsed` clamp | 1…6 | max catch-up per iteration |
| `kGameLenSecondsTable` | `30, 18, 12, 9` | game-seconds per real second → 3 / 5 / 7½ / 10 min |
| Seconds accumulator refill | 70 (PC) / 49 (Amiga) | |
| Period ends | 45, 90, 105, 120 | displayed minutes |
| End-of-period counter | 55 (PC) / 50 (Amiga) ticks | deferred whistle |
| Injury-time condition | ball y ≤ 216 or > 682, or attack live | `prolongLastMinute` |
| Playable area | x ∈ [81, 590], y ∈ [129, 769] | |
| Centre spot | (336, 449) | |
| Goal mouth / crossbar | x ∈ [303, 367], z ≤ 15 | |
| Penalty area | x ∈ [193, 478], y ≤ 216 / ≥ 682 | |
| Penalty spots | (336, 187), (336, 711) | |
| Free-kick band | y ∈ [216, 331) / (567, 682] | else `ST_FOUL` |
| Free-kick x bands | `153, 261, 309, 362, 410, 518` | selects one of 7 states |
| Throw-in y thirds | `342`, `556` | forward / centre / back |
| Near miss | speed ≥ 768, x ∈ [290, 381], z + 2 ≤ 25 | |
| Goal-attempt window | x ∈ [240, 431] | on target: [303, 367] |
| Foul contact | squared distance ≤ 32 | ≈ 5.7 units |
| Foul relevance | victim `ballDistance` ≤ 800 | ≈ 28 units |
| Tackle-from-behind | facing within ±1 octant | |
| Red card odds | 12.5 % normally, **87.5 %** as last man | `Rand() < 32` |
| `playerCardChances3min` | `4,4,4,4,5,5,5,5,6,6,6,7,7,8,9,10` | out of 16 |
| `playerCardChances10min` | `1,1,1,1,1,2,2,2,2,2,2,2,2,2,3,3` | |
| Injury base gate | `Rand() & 3` (25 %) | |
| `kTackleInjuryProbability` | `48, 28, 20, 14` by match length | doubled if already injured |
| `kInjuryLevels` | `42, 7, 5, 4, 3, 2, 1` | severity thresholds |
| `pitchBallSpeedFactor` | `−3, 4, 1, 0, 0, −1, −1` | friction offset, loose ball only |
| `ballSpeedBounceFactor` | `24, 80, 80, 72, 64, 40, 32` | by pitch type |
| `ballBounceFactor` | `88, 112, 104, 104, 96, 88, 80` | by pitch type |
| `kBallGroundConstant` / `kBallAirConstant` | 13 / 4 (PC), 16 / 10 (Amiga) | |
| Away-goals weight | first-leg goals × 2 | second leg, level aggregate |
| `stoppageEventTimer` | 100 (half) / 150 (full time) / 110 (extra) | |

---

## 12. Open questions

**Resolved by the Amiga oracle** (see §14):

- ~~Whether the `currentGameTick`-as-dice behaviour is original or a port artefact.~~
  **Split answer, and both halves matter.** Using the frame counter as a dice source
  is *original* and pervasive — three separate mechanics do it. But the **wall-clock
  coupling that makes it non-deterministic is the port's**: the Amiga advances its
  clock by a fixed decrement once per vertical blank and never by a variable frame
  delta. Under the original, tick-derived dice are perfectly reproducible.
- ~~Offside.~~ Confirmed absent in the Amiga match module too — a second independent
  sweep finding nothing. Record it as a confirmed absence.
- ~~The exact ball-physics constants.~~ All recovered; see [BALL.md](BALL.md) §12.

**Still open:**

- **The off-screen result model** (§10) — the largest single gap, and the one whose
  absence is most visible in a career mode. §14 narrows *where* it is not.
- Whether the free-kick sub-states (`LEFT1…RIGHT3`) differ in anything but the wall
  arrangement; the positions themselves live in the `ballOutOfPlayPositions` tables,
  which I have not extracted. (And there is no wall — [SETPIECES.md](SETPIECES.md) §8.)
- **How cards are gated.** §6.3's once-per-match strictness value compared against
  `currentGameTick` bits has no counterpart in the Amiga, which gates the card
  escalation on a global *and* on `Rand() & 3` (§14). Two different mechanisms for
  the same decision; at most one is right for either build.
- The substitution flow and the `g_waitForPlayerToGoInTimer` sequence
  ([LEGACY.md](LEGACY.md) §4) — touched here only where it intersects selection.
- Whether `prolongLastMinute` (§3.2) exists on the Amiga at all. The Amiga reads the
  end-of-period path as a flat 50-frame grace with no extension condition (§14).

---

## 13. Guidance for the reimplementation

- **One fixed-step tick, and derive everything from the tick count.** No
  `lastFrameTicks`. The clock, the referee, the display timers and the "dice" should
  all be functions of an integer tick counter, so the same inputs always produce the
  same match. This single change buys replay determinism, headless league
  simulation, and network play ([LEGACY.md](LEGACY.md) §8, §12).
- **Record inputs plus a seed, not coordinates.** SWOS could not do this because its
  clock was wall-clock-coupled; we can. Replays become tiny and re-simulatable.
- **Keep the two-level state machine.** `gameStatePl` (live / stopped) plus a named
  situation is a clean shape, and the restart-writes-four-things idiom (spot, camera,
  turn mask, mode) is worth keeping literally.
- **Keep the injury-time rule.** "Do not blow the whistle while the ball is in a box
  or an attack is live" is three lines and is a large part of why SWOS matches feel
  like matches.
- **Scale per-event probabilities by match length.** Cards and injuries both do this,
  so a 3-minute game has the same *number* of incidents as a 10-minute one. Any clone
  with a match-length option needs this and most forget it.
- **Roll the referee once per match**, not per foul.
- **Possession is a tick counter for `lastTeamPlayed`.** Do not "improve" it into
  time-in-control; the number the original shows is this one.
- **Pitch type belongs in the ball's friction and bounce**, applied only to a loose
  ball. Three small tables, seven surfaces.
- **Decide the CPU booking bias deliberately.** The original declines to book a CPU
  player who already has a card. That is a choice, not a law.
- **Treat off-screen results as a pluggable module.** It is genuinely separate from
  the match engine in the original too — confirmed twice, from two binaries (§14) —
  and we can improve it without betraying anything.
- **Preserve the player sweep order.** Players update in index order and each sees
  the already-updated positions of those before it and the stale positions of those
  after (§14). It is asymmetric, it is real, and it is the easiest place to
  introduce a divergence that only surfaces hundreds of ticks later in a trace.
- **Model the stoppage machine explicitly with the timer values in §14.** The 50/75
  split and the CPU's 350/600/750 timeouts are what make a CPU-versus-CPU match take
  the length it does, and they are cheap now and expensive to fit later.

---

## 14. Amiga cross-check

Traced independently through the Amiga original — [amiga/TIMING.md](amiga/TIMING.md).

### The clock: arithmetic confirmed, wall-clock coupling refuted

The Amiga's `UpdateTime` (asm:26101) is §3.1 without the frame delta:

```
secondsSwitchAccumulator -= timeDelta          ; 30 / 18 / 12 / 9
if secondsSwitchAccumulator < 0:
    secondsSwitchAccumulator += 49
    gameSeconds += 1                            ; <-- not += lastFrameTicks
```

`timeDelta` is picked once at `InitGame` from a four-entry table (asm:26526) holding
exactly §3.1's `30, 18, 12, 9`, and the reload is exactly the Amiga column's 49. So
the clock model is confirmed — **and the `+= swos.lastFrameTicks` in §3.1 has no
counterpart.** On the original, one vertical blank advances the accumulator by
exactly one step, the simulation is fixed-step end to end, and there is no path by
which a slow machine skips dice values.

That reframes §1.2 and §1.3 considerably. The determinism hazard is the *port's*
frame pacer, not SWOS's design. §13's first recommendation — one fixed-step tick,
everything derived from the tick count — is therefore not a departure from the
original at all. It is a restoration.

Working the four settings through at 50 Hz corroborates the frame rate itself:

| Setting | `timeDelta` | Frames per game-second | Frames per 45′ half | Full match |
|---|---|---|---|---|
| 0 | 30 | 1.633 | 4 410 | ≈ 3 min |
| 1 | 18 | 2.722 | 7 350 | ≈ 5 min |
| 2 | 12 | 4.083 | 11 025 | ≈ 7½ min |
| 3 | 9 | 5.444 | 14 700 | ≈ 10 min |

Four arbitrary constants plus an arbitrary reload of 49 landing on the exact four
options a player sees in the menu is strong evidence that both the 50 Hz assumption
and the reading of the clock are right.

### Minutes are packed decimal

New, and it explains something odd. `gameTime` is a **longword of four decimal
digits**, one per byte, incremented with manual carry at 10. That is why the
comparison points look the way they do in a raw listing:

| Comparison | Means |
|---|---|
| `gameTime == $405` | minute 45 |
| `gameTime == $900` | minute 90 |
| `gameTime == $10005` | minute 105 |
| `gameTime == $10200` | minute 120 |

The format exists so the scoreboard can render digits without dividing. §3.2's
period boundaries — 45, 90, 105, 120 — are exactly these. Store minutes as an
integer and format at the boundary, but keep the comparison points as named
constants; they are the real content.

### The frame order, and one determinism detail §1.1 is missing

The Amiga's per-frame chain matches §1.1's shape: clock, then input, then the ball,
then the players, then presentation. Input is latched **once**, into the team
structs, and nothing re-reads the hardware later in the frame. The ball moves before
the players, so every player reacts to a ball position that already includes this
frame's physics.

What §1.1 does not say: **players are swept in a fixed index order, 0 to 10, and
each writes its results immediately.** A player at index 3 therefore sees the
already-updated positions of players 0–2 and the stale positions of 4–10. The Amiga
document is emphatic that this asymmetry is real and must be preserved.

Nothing in the Amiga reading confirms or refutes §1.1's one-team-per-frame
alternation. It records a single `UpdatePlayersAndBall` per frame with a register
pointing at one team, which is consistent with alternation but is not evidence for
it. Still the highest-value open item across the whole corpus
([MOVEMENT.md](MOVEMENT.md) §13).

### Periods, injury time, and the away-goals rule

Confirmed exactly: the boundaries at 45 / 90 / 105 / 120, the end-of-period latch
setting `gameSeconds = −1` with a **50-frame** countdown (§11's Amiga column), and
the `halfPlayed` promotion 0 → 1 → 2 that feeds career appearances.

The away-goals rule is confirmed as §8 describes it, down to the implementation:
each side's away-goal tally is **doubled** and added to the other leg's total. Two
readings of an unusual arithmetic trick agreeing is worth noting.

**One gap.** The Amiga reads the 50 frames as the whole of injury time — *"a flat
fifty frames, not a computed allowance"* — and found no equivalent of §3.2's
`prolongLastMinute`. §3.2 is the richer and more specific reading and is probably
right for the port; whether the Amiga has the same extension condition and it was
simply not traced is open.

### The stoppage machine, in detail §2 does not have

`RunStoppageEventsAndSetAnimationTables` (asm:37250) stages the break on
`breakCameraMode`:

| Stage | Meaning | Duration |
|---|---|---|
| 0 | Play live | — |
| 1 | Ball at rest, break begins | 50 frames, or **75 after a goal** |
| 2 | Positions being taken up, ball placed | until players are set |
| −1 | Restart armed | until fire, or the CPU's timeout |

Stage 1 **will not begin until both of the ball's horizontal deltas are zero** — the
break waits for the ball to actually stop, it is not triggered by the whistle. The
one exception is a keeper holding the ball (`gameState` 3), which skips the wait.
Auto-replay and highlight-save hooks fire during stage 1, gated so that replays are
offered for goals and near-misses but not for throw-ins.

A human side ends the stoppage by pressing fire, from either joystick and either
coach slot. A CPU-versus-CPU match uses timers instead: **600 frames** on the
half-time and full-time result screens, **350** otherwise, and a CPU kick-off after
a goal will not proceed until **750 frames — 15 seconds** — have passed. §11's
`stoppageEventTimer` of 100 / 150 / 110 is a different set of timers for the period
transitions and does not conflict.

While stopped, players still move but at reduced speed through two ramps, and
outfielders use the *flatter* `playerSpeedsGameStopped` table
([MOVEMENT.md](MOVEMENT.md) §13) — everyone walks back at nearly the same pace
regardless of ability, which is why the fastest player has no advantage getting to a
throw-in.

### Out of play: confirmed, with one correction

The corner and goal-kick spots in §5.4 match the Amiga's placement table exactly,
turn masks included. The throw-in y thirds 342 / 556 match, and the Amiga adds that
they are the **same two thresholds the shot-on-goal test uses**
([SHOOTING.md](SHOOTING.md) §9) — one pitch division serving two purposes, worth
sharing as a named constant.

**§5.3's near miss is not "no gameplay effect".** The Amiga reads the same test —
speed ≥ $300 (768), X 290 … 381, Z < 25 — and finds it *also clears the
referee-whistle flag*, so a shot that comes back off the frame and out is not
whistled as an ordinary out-of-play. Three identical constants and one extra
consequence: the constants confirm the reading, and the consequence should be added
to §5.3.

Goal-mouth geometry differs by one unit as it does in [BALL.md](BALL.md) §12 —
§5.1's `x ∈ [303, 367]` against the Amiga's 302 … 366 — almost certainly an
inclusive/exclusive convention difference in the two transcriptions.

### Cards: two different mechanisms

§6.3's model is a per-match strictness value, drawn once, compared against
`(currentGameTick & 0x1E) >> 1`, with a last-man test flipping 12.5 % red to 87.5 %
red. The Amiga's card path (`sub_111388`, asm:36096) is gated on a global *and* on
`Rand() & 3` — roughly one in four, when a difficulty flag permits — with no
strictness table and no last-man test found.

These are not variations on one design; they are different designs. §6.3 is much
more specific and carries real tables, so it is likely the better reading of the
port. Whether the Amiga does the same thing and it was not traced, or the two builds
genuinely differ, is now an explicit question. It matters: §6.3's match-length
scaling is called out in §13 as an idea worth stealing.

Both readings agree on the important negative, though: **the foul decision itself
contains no randomness.** Only the card draw touches the RNG
([TACKLING.md](TACKLING.md) §12).

### §10, corroborated from the other side

The Amiga match module is a separate overlay (`swos4.dk1`/`dk2`/`dku`), and its
string table contains match statistics, tactics, substitutions, replay and disk I/O
— no division names, no fixture or league-table strings, no career vocabulary. A
sweep for routines calling `Rand` three or more times, the shape any goal-generating
simulation must have, returns exactly five, and each is identifiable: pitch and
weather selection, the kick-off coin flip, celebration length and restart camera,
the crowd-chant scheduler, and crowd reactions. **None produces a scoreline.**

So §10's structural claim — that off-screen results live outside the match engine —
is now confirmed from two independent binaries. The closest relative *inside* the
match code is the goal/save resolution ([AI.md](AI.md) §10), a skill-difference
lookup against a sixteen-entry chance table. Nothing in either match binary
constrains a result simulator, which means §13's "treat it as a pluggable module" is
not a compromise; it is the correct architecture.

One caveat for §1.3: the Amiga reads a **single** `Rand` stream, where §1.3 and
[AI.md](AI.md) §6 find two (`Rand`/`Rand2`). §10 relies on `Rand2` keeping result
generation off the match engine's stream. If the second stream is a port addition,
that isolation does not exist in the original — worth checking before depending on
it.
