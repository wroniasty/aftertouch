# BENCH.md

In-match management: how you summon the dugout without a pause menu, the five
states it can be in, how a substitution actually executes, how tactics are changed
mid-game, and the interlocks that stop any of it happening at the wrong moment.
Traced through the reference DOS port in
[../reference/swos-port/](../reference/swos-port/).

[LEGACY.md](LEGACY.md) §4 sketches substitutions from the manual's point of view;
this is the mechanism. The camera's bench modes are [CAMERA.md](CAMERA.md) §6, and
the referee interlock is [REFEREE.md](REFEREE.md).

> **Provenance.** [bench/](../reference/swos-port/src/game/bench/) is modern C++
> rather than decompilation, so the state machine and constants are read directly.
> The **UI layout and rendering** in `drawBench.cpp` is largely the porters' work
> and is deliberately not covered here — this document is the model, not the view.
> Read to understand the design; write our own code.

---

## 0. One-paragraph version

SWOS has no pause menu. You reach the bench by **double-tapping a direction** while
play is stopped — two taps of the *same* direction within 15 ticks, with a release
required between them. That drops you into a five-state machine
(`kInitial`, `kAboutToSubstitute`, `kFormationMenu`, `kMarkingPlayers`,
`kOpponentsBench`) driven by the same eight-way stick and one fire button as the
match itself. A substitution runs in **two phases separated by the outgoing player
walking off**: `initiateSubstitution` increments the sub count and sends him to
`(39, 449)` on the touchline, and only when he arrives does `substitutePlayer`
swap the two player records, reapply tactics and start a 100-tick delay while the
replacement walks on. Changing tactics is the same shape but instant. Five
independent conditions block the bench and four more make it unavailable — the
interlocking is the majority of the logic.

---

## 1. State

```
enum class BenchState {
    kInitial,             // browsing the bench, arrow over a player
    kAboutToSubstitute,   // picked who comes on, choosing who goes off
    kFormationMenu,       // changing tactics
    kMarkingPlayers,      // assigning man-marking
    kOpponentsBench,      // viewing the other team's bench
};
```

| Variable | Meaning |
|---|---|
| `m_state` | The above |
| `m_arrowPlayerIndex` | Cursor position on the bench |
| `m_playerToEnterGameIndex` | Substitute coming on |
| `m_playerToBeSubstitutedPos` / `Ord` | Player going off — position slot and ordinal |
| `m_selectedFormationEntry` | Cursor in the tactics menu |
| `m_shirtNumberTable[2][11]` | Per-team shirt-number permutation, identity at kick-off |
| `m_goToBenchTimer` | Entry delay |
| `m_blockFire`, `m_blockDirections` | Input latches to prevent double-actions |
| `m_pl1TapState`, `m_pl2TapState` | Per-player double-tap detectors (§2) |

Global interlock flags live in `swos`: `g_substituteInProgress`,
`g_waitForPlayerToGoInTimer`, `g_cameraLeavingSubsTimer`, `substitutedPlSprite`,
`teamThatSubstitutes`.

---

## 2. Getting to the bench: the double tap

[benchInvoked()](../reference/swos-port/src/game/bench/updateBench.cpp#L440). There
is a dedicated `kGameEventBench` control in the port, but the original's way in is
a **double tap of any direction**:

```
if (no direction held) {
    blockWhileHoldingDirection = false
    if (++tapTimeoutCounter == 15) reset()          // kTapTimeoutTicks
} else if (!blockWhileHoldingDirection) {
    if (gotLastTapDirection()) {
        tapTimeoutCounter = 0
        if (holdingSameDirectionAsLastTap()) {
            if (++tapCount >= 2) return true        // kNumTapsForBench
            else blockWhileHoldingDirection = true
        } else {
            previousDirection = none; tapCount = 0  // different direction: start over
        }
    } else {
        previousDirection = current
        blockWhileHoldingDirection = true
    }
}
```

Three details make this work as an input gesture rather than a nuisance:

- **`blockWhileHoldingDirection`** forces a release between taps. Holding a
  direction registers once, not once per tick.
- **The second tap must be the same direction.** Tapping up then left resets the
  counter rather than counting as two taps.
- **The 15-tick timeout only runs while no direction is held.** A slow double tap
  with the stick centred between taps still works if the gap is under 15 ticks.

Each team has its own `TapCounterState`, so both players can reach their own bench
independently.

---

## 3. The interlocks

This is most of the code, and it is worth reading as a list of everything that must
*not* be happening.

**[benchUnavailable()](../reference/swos-port/src/game/bench/updateBench.cpp#L?)** —
also resets both tap detectors and clears the pending bench calls:

```
gameStatePl == kInProgress          // play is live
|| cardHandingInProgress()          // referee is booking someone
|| playingPenalties
|| gameState ∈ [kStartingGame, kGameEnded]
```

**[benchBlocked()](../reference/swos-port/src/game/bench/updateBench.cpp#L?)** — and
note the first two *decrement as a side effect of being tested*:

```
if (g_waitForPlayerToGoInTimer)  { g_waitForPlayerToGoInTimer--;  return true }
if (g_cameraLeavingSubsTimer)    { g_cameraLeavingSubsTimer--;    return true }
return g_substituteInProgress || refereeActive() || statsTimer
```

**Two countdown timers are driven from inside a predicate.** `benchBlocked()` is
not a pure query — calling it advances the state machine. That is fragile (calling
it twice in a tick would double-decrement) and it is exactly the kind of thing to
*not* copy, while being aware the reference behaves this way.

The net effect: the bench is reachable only while play is stopped, no card is being
shown, no substitution is mid-flight, the referee is off screen, and the stats
overlay is down.

---

## 4. Substitution, phase one

[initiateSubstitution()](../reference/swos-port/src/game/bench/updateBench.cpp#L?):

```
setSubstituteInProgress()
numSubs++                                  // team1NumSubs or team2NumSubs
m_blockFire = true
m_state = kInitial

substitutedPlSprite   = m_team->players[m_playerToBeSubstitutedPos]
teamThatSubstitutes   = m_team
substitutedPlSprite->cards = 0
substitutedPlDestX/Y  = (39, kPitchCenterY)     // 39, 449 — the touchline
plSubstitutedX/Y      = (39, kPitchCenterY)
```

The source comments the key point: *"must set this sprite since the game waits for
him to arrive at the destination before continuing"*. The substitution is **not
atomic**. The outgoing player is given a destination on the touchline and walks
there under normal movement ([MOVEMENT.md](MOVEMENT.md) §7), and the rest of the
substitution is deferred until he arrives.

`cards = 0` is cleared here — a booked player who is substituted takes his card
off the pitch with him.

While this is running, [BALL.md](BALL.md) §2 step 8 has a special case:
`ST_KEEPER_HOLDS_BALL` combined with `g_substituteInProgress` and a matching
`teamThatSubstitutes` drops the ball to the ground. Substituting while the keeper
holds the ball is explicitly handled.

---

## 5. Substitution, phase two

[substitutePlayer()](../reference/swos-port/src/game/bench/updateBench.cpp#L?),
called from `benchCheckControls` once `newPlayerAboutToGoIn()`:

```
substitutedPlSprite->injuryLevel = 0
substitutedPlSprite->sentAway    = 0

maintainMarkedPlayer()
swapPlayerShirtNumbers(m_playerToEnterGameIndex, m_playerToBeSubstitutedPos)

teamGame->players[outPos].position = PlayerPosition::kSubstituted
std::swap(teamGame->players[inIdx], teamGame->players[outPos])

initializePlayerSpriteFrameIndices()
ApplyTeamTactics()

g_waitForPlayerToGoInTimer = 100          // kPlayerGoingInDelay
m_blockFire = true
enqueueSubstituteSample()
leaveBench()
```

Points worth extracting:

- **The two players are swapped in the array**, and the outgoing one is marked
  `kSubstituted` *before* the swap — so the marker travels with him into the bench
  slot. The squad list is a permutation, not a list with holes.
- **`injuryLevel` and `sentAway` are cleared on the outgoing player.** An injured
  player who is substituted stops being injured. Since he cannot return, this is
  bookkeeping rather than a loophole — but it means injury state does not persist on
  the record, which matters if the match result feeds a career.
- **`swapPlayerShirtNumbers`** maintains `m_shirtNumberTable`, initialised to the
  identity at kick-off. Shirt numbers follow the *slot*, not the player.
- **`ApplyTeamTactics()` is re-run** — the incoming player is slotted into the
  existing formation rather than the formation adapting to him.
- **`maintainMarkedPlayer()`** fixes up man-marking assignments so a marker who
  leaves the pitch does not strand his target.
- A **100-tick delay** covers the replacement walking on.

`kMaxSubstitutes = 5`, though `gameMaxSubstitutes` is consulted separately at
[:436](../reference/swos-port/src/game/bench/updateBench.cpp#L436) with a special
case for index 11 — suggesting the bench can hold more than five in some modes.

---

## 6. Tactics and marking

[changeTactics()](../reference/swos-port/src/game/bench/updateBench.cpp#L?):

```
pl1Tactics or pl2Tactics = newTactics
m_team->tactics          = newTactics
ApplyTeamTactics()
enqueueTacticsChangedSample()
leaveBenchFromMenu()
```

Instant, unlike a substitution — no walk-off, no timer, no cost. Tactics are stored
in **two places** (the per-player global and the team struct) which must be kept in
sync. `kNumFormationEntries = 18` is the size of the formation menu.

The tactics themselves — the grid, the 35 cells, how `ApplyTeamTactics` turns a
tactic into per-player destinations — are [AI.md](AI.md) §3.

`kMarkingPlayers` is a separate state for assigning man-marking, entered from the
formation menu and handled by
[markPlayersMenuHandler()](../reference/swos-port/src/game/bench/updateBench.cpp#L409).
Its interaction with `m_playerToBeSubstitutedOrd < 0` and `== 1` (the goalkeeper)
at [:439-448](../reference/swos-port/src/game/bench/updateBench.cpp#L439-L448)
suggests keepers cannot be assigned as markers, but the logic was not fully traced.

---

## 7. Geometry

From [bench.cpp](../reference/swos-port/src/game/bench/bench.cpp):

| Constant | Value | Meaning |
|---|---|---|
| `kBenchX` | 27 | Bench camera x — matches [CAMERA.md](CAMERA.md) §6's slide to `x = 0` |
| `kTopBenchY` | 389 | |
| `kBottomBenchY` | 485 | |
| `kTrainingPitchBenchY` | 456 | |
| `kPlayerGoingInX/Y` | 26, 449 | Where the substitute waits |
| Substituted player dest | 39, 449 | Where the outgoing player walks |

Both benches sit either side of the halfway line (449), 96 units apart, which is
why [CAMERA.md](CAMERA.md) §6's slide band is `y ∈ [339, 359]` — the camera must be
near the halfway line before it is allowed to pan off the pitch to the dugout.

`swapBenchWithOpponent()` and `kOpponentsBench` let you inspect the other team's
bench — a scouting feature, read-only.

---

## 8. Constants quick reference

| Constant | Value | Meaning |
|---|---|---|
| `kNumTapsForBench` | 2 | Taps to open the bench |
| `kTapTimeoutTicks` | 15 | Tap window, counted only while stick is centred |
| `kEnterBenchDelay` | 15 | |
| `kPlayerGoingInDelay` | 100 | Ticks while the substitute walks on |
| `kLeavingSubsDelay` | 55 | |
| `kSubstituteFireTicks` | 8 | |
| `kMaxSubstitutes` | 5 | |
| `kNumFormationEntries` | 18 | Formation menu size |
| `kBenchX` | 27 | |
| `kTopBenchY` / `kBottomBenchY` | 389 / 485 | |
| Substituted player destination | `(39, 449)` | |
| Substitute waiting spot | `(26, 449)` | |

---

## 9. What this resolves, and what still needs measurement

**Confirmed as structure:**

- The bench is entered by a double-tap of the same direction, with a required
  release between taps and a 15-tick window that only runs while centred. ✓
- Five bench states, driven by the same stick-and-fire as the match. ✓
- Substitution is two-phase, separated by the outgoing player physically walking to
  `(39, 449)`. ✓
- The squad array is a permutation; the outgoing player is marked `kSubstituted`
  and swapped into the bench slot. ✓
- Injury, sent-away and card flags are cleared on the outgoing player. ✓
- Shirt numbers follow the slot via a permutation table. ✓
- `ApplyTeamTactics()` re-runs after both substitution and tactics change. ✓
- Tactics changes are instant and free; substitutions are not. ✓
- Nine separate conditions gate bench access. ✓
- Two of those conditions decrement timers as a side effect of being tested. ✓

**Open (measurement targets, [LEGACY.md](LEGACY.md) §15):**

- **Does the CPU use the bench at all?** Nothing in this file suggests an AI path
  for substitutions or tactics changes. If the CPU never substitutes, that is a
  significant behaviour to know about; if it does, the code is elsewhere.
- **`gameMaxSubstitutes` vs `kMaxSubstitutes = 5`** — the special case at
  [:436](../reference/swos-port/src/game/bench/updateBench.cpp#L436) implies a
  larger bench in some modes. Which, and how large?
- **`maintainMarkedPlayer()`** was not traced — what happens to a marking assignment
  when either the marker or the target leaves.
- Whether an injured player is *forced* off, or merely can be. [SIMULATION.md](SIMULATION.md)
  §6 has the injury path; the connection to this code is not established.
- The `kMarkingPlayers` goalkeeper special-cases in §6.
- What `m_playerToEnterGameIndex == 11` means (index 11 in an 11-player array).
- Whether the double-tap gesture is the original's or the porters' — it is
  implemented in modern C++ here, and `kGameEventBench` exists alongside it.
  [LEGACY.md](LEGACY.md) §3 should confirm against the manual.
- The bench UI layout and rendering, deliberately excluded here.

---

## 10. Guidance for the reimplementation

- **Keep the two-phase substitution.** The walk-off is not decoration — it costs
  real time, it is visible, and it makes substituting during a dangerous moment a
  genuine decision. An atomic swap removes a tactical consideration.
- **Make the squad a permutation, not a list with holes.** Swapping records and
  marking position `kSubstituted` keeps indices stable and avoids every
  null-check that a sparse list would need.
- **Model the interlocks as one predicate over explicit state**, not as a chain of
  side-effecting tests. Specifically: **do not decrement timers inside
  `benchBlocked()`**. Advance timers in the tick, query them in the predicate. The
  reference's approach works only because it is called exactly once per tick.
- **Keep the double-tap gesture.** No-pause-menu in-match management is part of what
  SWOS is, and the release-between-taps rule is what makes it usable rather than
  accidental. Also expose a dedicated button, as the port does.
- **Re-apply tactics after any squad change**, from one function, so formation state
  can never drift from the squad it describes.
- **Decide deliberately what clearing `injuryLevel` should mean** if match results
  feed a career. The reference clears it because nothing downstream reads it; that
  will not be true for us.
- **Separate model from view now.** This document covers only the model, because the
  port's bench rendering is its own invention. Ours will be too — so keep the state
  machine free of layout.
