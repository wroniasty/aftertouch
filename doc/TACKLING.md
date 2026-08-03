# TACKLING.md

The tackle, end to end: how a slide starts, what it costs, how contact with the
ball is resolved, how possession is decided when two players arrive together, and
what happens to the player who gets hit. Traced through the reference DOS port in
[../reference/swos-port/](../reference/swos-port/).

This document closes the promise made in [MOVEMENT.md](MOVEMENT.md) §10 — *"full
resolution belongs in a tackle document, not here"* — and the open item in
[CONTROL.md](CONTROL.md) §6. The **consequences** of a foul (cards, injuries,
free-kick placement, the last-man test) belong to [SIMULATION.md](SIMULATION.md) §6
and are only referenced here; this document is the contest itself. Heading, the
other contest, is [HEADING.md](HEADING.md).

> **Provenance.** The tackle routines survive as register-level decompilation and
> the control flow below is read straight from it. **The constants are real**: the
> tables are visible in the annotated disassembly
> ([swos.asm:245813](../reference/swos-port/swos/swos.asm#L245813),
> [:245843](../reference/swos-port/swos/swos.asm#L245843)) with literal values, not
> as data-segment addresses. Two dead stores in §2 were confirmed against the
> original assembly, not just the decompiled C++. Read to understand the design;
> write our own code.

> **Second oracle.** [amiga/CONTEST.md](amiga/CONTEST.md) traces the same system
> through the Amiga original (`CalculateIfPlayerWinsBall` asm:35144, the foul test
> `sub_113122` asm:39106). Every table in §9 is confirmed, and the attribute-range
> worry in §10 turns out to be unfounded. But the **foul-from-behind test in §5 comes
> out the opposite way round**, and the flat-3 recovery table in §6 has a completely
> different explanation. See §12.

---

## 0. One-paragraph version

A tackle is a **state, not an event**. `playerBeginTackling` puts the player in
`kTackling`, aims him one step ahead in his facing octant, and launches him at
`kPlayerTacklingSpeed = 1792` — faster than any player can run. He then decelerates
under normal ground friction until he stops, at which point a **downtime** keyed to
his Tackling attribute pins him in place. While sliding, if he reaches the ball,
`playersTackledTheBallStrong` fires: the ball is kicked away at **125 % of his
current speed** in his input direction, his own speed is halved, and a
`tackleState` flag records that he touched it. Separately, if he ends up within a
small radius of the opponent's controlled player, `playerTacklingTestFoul` runs:
the victim is knocked down, the tackler is cut to a quarter speed, and whether it
is a **foul** is decided entirely by `tackleState` plus relative facing. Possession
itself, when contested, is one `rand() & 31` roll against a threshold table indexed
by the **difference of the two players' (Tackling + Control) / 2 averages** — a
50 % coin flip between equals, rising to only 72 % for the largest gap.

---

## 1. State

| Field | Offset | Meaning |
|---|---|---|
| `Sprite.state` | +12 | `kTackling` (1) while sliding, `kTackled` (3) for the victim |
| `Sprite.playerDownTimer` | +13 | Counts down; at 0 the player returns to `kNormal` |
| `Sprite.tacklingTimer` | +106 | Ticks since the slide began; **goes negative to mean "ending"** |
| `Sprite.tackleState` | +96 | 0 = never touched ball, 1 = touched it, 2 = `TS_GOOD_TACKLE` |
| `Sprite.speed` | +44 | Slide speed, decaying |
| `TeamGeneralInfo.headerOrTackle` | +52 | Set to 1 when either contest is triggered |
| `TeamGeneralInfo.lastHeadingTacklingPlayer` | +72 | Who last contested |
| `TeamGeneralInfo.wonTheBallTimer` | +138 | Set to 12 for the contest winner |
| `PlayerGameHeader.tackling` | +72 | Tackling attribute |
| `PlayerGameHeader.ballControl` | +73 | Control attribute |
| `PlayerGameHeader.fasterTackle` | +50 | **Has no effect — see §2** |

`tackleState` is the spine of the whole system. It is the only thing that
distinguishes a clean challenge from a foul, and it is set by physical contact with
the ball, not by intent.

---

## 2. Starting the slide

[playerBeginTackling()](../reference/swos-port/src/game/updatePlayers/updatePlayers.cpp#L14839):

```
tackleState             = 0                       // has not touched the ball
team.controlledPlDirection = D0                   // input direction, locked
sprite.direction        = D0
animation table         = plTacklingAnimTable
state                   = kTackling
destX, destY            = pos + kDefaultDestinations[direction]
speed                   = kPlayerTacklingSpeed    // 1792
tacklingTimer           = 0
```

`kDefaultDestinations` is the same eight-entry octant offset table
[MOVEMENT.md](MOVEMENT.md) §3 describes — the tackle aims one step ahead and
never re-aims. **Direction is locked at launch.** Once committed, a slide cannot be
steered.

**`kPlayerTacklingSpeed = 1792`** against a maximum running speed of 1250
([MOVEMENT.md](MOVEMENT.md) §10) — the slide is 43 % faster than a sprint. That
differential is what makes the tackle a real option rather than a desperation move.

### Two dead stores

Three separate instructions write `playerDownTimer` in this function, and the
**last one unconditionally overwrites the other two**
([:14852-14893](../reference/swos-port/src/game/updatePlayers/updatePlayers.cpp#L14852-L14893)):

```
playerDownTimer = m_playerDownTacklingInterval    // 55 (PC) / 50 (Amiga)
if (PlayerGameHeader.fasterTackle)
    playerDownTimer = 25
// @@no_faster_tackle:
playerDownTimer = -1                              // always
```

This is not a decompilation artefact. The original assembly has the same shape —
the `fasterTackle` branch writes 25 and then falls straight through the
`@@no_faster_tackle` label into the `-1`
([swos.asm](../reference/swos-port/swos/swos.asm#L106330)). Two consequences:

- **The `fasterTackle` player attribute does nothing.** It is read, tested,
  branched on, and its effect is discarded one instruction later.
- **The Amiga/PC tackle-downtime split does nothing either.** `amigaModeActive()`
  sets `m_playerDownTacklingInterval` to 50 vs 55
  ([amigaMode.cpp:30,48](../reference/swos-port/src/game/amigaMode.cpp#L30)), and
  that value is likewise overwritten.

`playerDownTimer = -1` means the countdown in §3 does not terminate the slide.
Deceleration does. Both belong in [LEGACY.md](LEGACY.md) §14.

---

## 3. The slide, per tick

[updatePlayers.cpp:6375-6493](../reference/swos-port/src/game/updatePlayers/updatePlayers.cpp#L6375-L6493):

```
if (tacklingTimer >= 0) tacklingTimer += 1
playerDownTimer -= 1
if (playerDownTimer == 0) { state = kNormal; stop; }

// --- human early release ---
if (tacklingTimer >= 0 && team.playerNumber != 0 && !team.firePressed) {
    tacklingTimer = -tacklingTimer
    if (tacklingTimer >= -2) tacklingTimer = -1
}

// --- deceleration ---
if (speed != 0) {
    speed -= kPlayerGroundConstant
    if (speed <= 0) { speed = 0; setPlayerDowntimeAfterTackle(); }
}
```

**Releasing fire cuts the tackle short.** The sign of `tacklingTimer` is the
"ending" flag: a human who lets go of the button flips it negative, and if the
slide was two ticks old or less it is pinned to exactly `-1`. This is a real,
undocumented control nuance — the player has some authority over slide length after
committing to it, even though he has none over its direction.

That `-1` then selects a different recovery table (§6), so an early release is
rewarded twice: shorter slide, and near-instant recovery.

---

## 4. Winning the ball

[playersTackledTheBallStrong()](../reference/swos-port/src/game/updatePlayers/updatePlayers.cpp#L14960),
which fires when a sliding player reaches the ball:

```
dir = team.currentAllowedDirection
if (dir < 0) dir = sprite.direction        // fall back to facing

ball.direction  = dir                       // deflected tackles: input, not facing
ball.destX/Y    = far away along dir
ball.speed      = player.speed + (player.speed >> 2)   // 125 %
player.speed  >>= 1                                    // 50 %
tackleState     = 1
```

**The ball leaves in the direction the player is *holding*, not the direction he is
sliding.** `currentAllowedDirection` is the live input; the slide direction was
frozen at launch. A player can therefore slide north and deflect the ball
north-east by holding the stick over. The function's own comment calls these
"deflected tackles" — it is a deliberate feature, and the single most skill-rewarding
detail in the tackle system.

Ball speed is **125 % of the tackler's current speed**, so an early contact
(still near 1792) blasts the ball away while a late one barely moves it. The
routine's comment notes CPU players get 100 % rather than 125 %.

### The good-tackle test

[:15147-15237](../reference/swos-port/src/game/updatePlayers/updatePlayers.cpp#L15147-L15237).
`tackleState` is promoted from 1 to `TS_GOOD_TACKLE` (2) only if **all** hold:

| Test | Meaning |
|---|---|
| opponent has a controlled player | there is someone to have fouled |
| `opponent.ballDistance >= 9` | the opponent was **not** close to the ball |
| `dx² + dy² > 32` (whole units) | the two players are **not** close together |

That is: *you took the ball while nobody was near enough for it to look like a
challenge.* It plays `PlayGoodTackleComment()` and, per §5, immunises the tackler
against the foul test.

Both paths end with `PlayKickSample()` and **`resetBothTeamSpinTimers()`** — winning
the ball in a tackle kills any live aftertouch, the same abort described in
[AFTERTOUCH.md](AFTERTOUCH.md) §3.

---

## 5. Contact with the opponent, and the foul test

[playerTacklingTestFoul()](../reference/swos-port/src/game/updatePlayers/updatePlayers.cpp#L12708).
Note it tests only against the **opponent's controlled player** — a slide cannot
foul an AI-driven off-ball opponent at all.

**Step 1 — proximity.** `dx² + dy² <= 32` in whole pitch units
([:12775-12785](../reference/swos-port/src/game/updatePlayers/updatePlayers.cpp#L12775-L12785)).
Roughly 5.6 units of separation.

**Step 2 — the goalkeeper exemption**
([:12794-12817](../reference/swos-port/src/game/updatePlayers/updatePlayers.cpp#L12794-L12817)).
If the victim is a keeper (`playerOrdinal == 1`), the tackler's speed is cut to a
quarter (`speed >>= 1` twice, then `|= 1`) and the function returns. **No knockdown,
no foul, ever.** Keepers absorb tackles.

**Step 3 — the victim must be in the pitch proper**
([:12847-12897](../reference/swos-port/src/game/updatePlayers/updatePlayers.cpp#L12847-L12897)):
`x ∈ [81, 590]`, `y ∈ [129, 769]`. Outside that box nothing happens at all —
challenges near the touchline and in the goalmouth simply do not register.

**Step 4 — knockdown.** Tackler cut to quarter speed, then `playerTackled()` on the
victim (§7).

**Step 5 — the foul decision**
([:12932-13001](../reference/swos-port/src/game/updatePlayers/updatePlayers.cpp#L12932-L13001)):

```
if (victim.ballDistance > 800)  → no foul      // victim wasn't playing the ball
if (tackleState == 0)           → FOUL         // never touched the ball
if (tackleState == TS_GOOD_TACKLE) {
    PlayDangerousPlayComment(); → no foul      // clean, but commented on
}
if (|tackler.direction - victim.direction| <= 1) → FOUL    // from behind
else                                             → no foul
```

Two refinements over [SIMULATION.md](SIMULATION.md) §6, which describes the same
ladder from the consequence side:

- The `ballDistance > 800` gate comes **first**. A player nowhere near the ball
  cannot be fouled, regardless of how he was hit.
- `TS_GOOD_TACKLE` does not merely skip the foul — it plays a **dangerous-play
  commentary line**. The referee waves it on and the commentator remarks on it.

**"From behind" is `|dirA − dirB| ≤ 1`** — both players facing within one octant of
each other. It is a facing test, not a geometric one, so a tackle that is
physically from the side but where both players happen to face the same way is
judged a foul. Cheap to compute, occasionally absurd, and load-bearing for how
SWOS's refereeing feels.

---

## 6. Recovery

[setPlayerDowntimeAfterTackle()](../reference/swos-port/src/game/player.cpp#L3132),
called when the slide decelerates to zero:

```
table = (tacklingTimer == -1) ? kComputerTacklingDownTime : kPlayerTacklingDownTime
playerDownTimer = table[tackling]
```

| Table | Values, indexed by Tackling 0–7 |
|---|---|
| `kPlayerTacklingDownTime` | `30, 27, 24, 21, 18, 15, 12, 9` |
| `kComputerTacklingDownTime` | `3, 3, 3, 3, 3, 3, 3, 3` |

The Tackling attribute's **only surviving mechanical effect on the slide itself is
recovery time** — a 0-Tackling player is grounded for 30 ticks, a 7-Tackling player
for 9. (Tackling also feeds the possession contest in §8.) Given `fasterTackle` is
dead (§2), this table is the whole of "some players tackle better".

`kComputerTacklingDownTime` is a flat **3 ticks regardless of attribute**, selected
by `tacklingTimer == -1` — the state a human sets by releasing fire early (§3). The
table's name says "computer", and it is presumably also the CPU's path, but the
condition that selects it is reachable by human input. **A human who taps and
releases recovers in 3 ticks instead of 9–30.** Whether that is intended is
unknown; it is certainly exploitable.

---

## 7. The victim

[playerTackled()](../reference/swos-port/src/game/updatePlayers/updatePlayers.cpp#L14410).
Sets the victim to `kTackled` and rolls for injury. The gating, before any injury
roll happens:

- **Training games**: `Rand() & 3` — a 3-in-4 chance to skip injury entirely
  ([:14419-14428](../reference/swos-port/src/game/updatePlayers/updatePlayers.cpp#L14419-L14428)).
- **`team1NumAllowedInjuries` / `team2NumAllowedInjuries`** must be non-zero. A team
  that has used its allowance cannot suffer another injury.

The injury probability itself (`kTackleInjuryProbability = 48, 28, 20, 14` by match
length, doubled if already injured) and the card/booking consequences are
[SIMULATION.md](SIMULATION.md) §6.

Movement while `kTackled` — the victim keeps sliding, decelerating at
`kPlayerGroundConstant`, with an extra −25 % if inside the goal area — is
[MOVEMENT.md](MOVEMENT.md) §9.

---

## 8. The possession contest

[player.cpp:360-440](../reference/swos-port/src/game/player.cpp#L360-L440). This is
what [CONTROL.md](CONTROL.md) §6 lists as open, and it is smaller than expected:

```
avgA = (A.tackling + A.ballControl) >> 1
avgB = (B.tackling + B.ballControl) >> 1
diff = avgA - avgB

favoured = (diff >= 0) ? A's team : B's team
diff     = |diff|

if ((Rand() & 31) < kPlAvgTacklingBallControlDiffChance[diff])
     winner = favoured
else winner = the other team

winner.wonTheBallTimer = 12
ball.speed = 0
ball.destX, ball.destY = ball.x, ball.y      // ball stops dead
```

`kPlAvgTacklingBallControlDiffChance = 16, 17, 18, 19, 20, 21, 22, 23`
([swos.asm:245813](../reference/swos-port/swos/swos.asm#L245813)), out of 32:

| Attribute-average gap | Chance the better player wins |
|---|---|
| 0 | 16/32 = **50.0 %** |
| 1 | 17/32 = 53.1 % |
| 2 | 18/32 = 56.3 % |
| 3 | 19/32 = 59.4 % |
| 4 | 20/32 = 62.5 % |
| 5 | 21/32 = 65.6 % |
| 6 | 22/32 = 68.8 % |
| 7 | 23/32 = **71.9 %** |

**Attributes barely matter.** The maximum possible edge is 72/28, and that requires
the largest gap the table can express. A one-point advantage is worth three
percentage points. SWOS's contests are deliberately close to coin flips, and this
single table is the clearest statement of the game's design philosophy anywhere in
the codebase: *the better player usually wins, and often doesn't.*

**The ball stops dead** on resolution — speed zeroed, destination collapsed to its
own position. It is not awarded to the winner's feet. `wonTheBallTimer = 12` gives
the winning team 12 ticks of some form of privileged access; exactly what it gates
is not established here.

---

## 9. Constants quick reference

| Constant | Value | Meaning |
|---|---|---|
| `kPlayerTacklingSpeed` | `1792` | Slide launch speed (vs 1250 max run) |
| `kPlayerTacklingDownTime` | `30, 27, 24, 21, 18, 15, 12, 9` | Recovery ticks by Tackling 0–7 |
| `kComputerTacklingDownTime` | `3` × 8 | Recovery when `tacklingTimer == -1` |
| `kPlAvgTacklingBallControlDiffChance` | `16..23` | Contest threshold vs `rand() & 31` |
| `wonTheBallTimer` | `12` | Ticks granted to the contest winner |
| Ball speed after tackle | `speed × 1.25` | 100 % for CPU players |
| Tackler speed after ball contact | `speed × 0.5` | |
| Tackler speed after player contact | `speed × 0.25`, min 1 | |
| Foul proximity | `dx² + dy² ≤ 32` | Whole pitch units |
| Good-tackle separation | `dx² + dy² > 32` **and** `ballDistance ≥ 9` | |
| Victim-was-playing-ball gate | `ballDistance ≤ 800` | |
| Foul-from-behind | `\|dirA − dirB\| ≤ 1` | Octants |
| Pitch-proper box | `x ∈ [81, 590]`, `y ∈ [129, 769]` | Outside → no contact at all |
| `m_playerDownTacklingInterval` | 55 PC / 50 Amiga | **Dead — overwritten** |
| `PlayerGameHeader.fasterTackle` | — | **Dead — overwritten** |

---

## 10. What this resolves, and what still needs measurement

**Confirmed as structure:**

- Tackle is a state with a locked direction, launched at 1792 and ended by
  deceleration, not by a timer. ✓
- `tackleState` (0 / 1 / `TS_GOOD_TACKLE`) is the sole discriminator between a clean
  challenge and a foul. ✓
- Ball leaves at 125 % of the tackler's current speed, in the **held input**
  direction — deflected tackles are deliberate. ✓
- Winning the ball resets both spin timers, killing aftertouch. ✓
- Keepers cannot be fouled by a tackle; contact merely slows the tackler. ✓
- Fouls only register against the opponent's **controlled** player, inside
  `[81,590] × [129,769]`. ✓
- Recovery time is the Tackling attribute's only effect on the slide. ✓
- The possession contest is one `rand() & 31` roll, 50 %–72 % across the whole
  attribute range. ✓
- `fasterTackle` and the Amiga/PC downtime split are dead stores, confirmed against
  the original assembly. ✓

**Resolved by the Amiga oracle** (see §12):

- ~~`kPlAvgTacklingBallControlDiffChance` has 8 entries but the gap could exceed 7 if
  attributes range 0–15.~~ **No bug.** Attributes are masked to `7` per nibble and
  clamped to 7 explicitly on load ([amiga/PLAYERS.md](amiga/PLAYERS.md) §1), so the
  maximum gap is exactly 7 and every eight-entry table in the engine is the right
  size. §11's "fix or reproduce the table bounds" instruction is moot.
- ~~What `wonTheBallTimer = 12` gates.~~ A **possession lock**: while it runs, the
  contest cannot re-run. It is also loaded with 8 from the dribble touch-count
  overflow ([CONTROL.md](CONTROL.md) §8). Whether the two reload values are
  distinguished by any consumer is still open on both oracles.
- ~~Running tackles.~~ Confirmed: there is no separate standing-tackle path. All
  dispossession goes through the same routine, which despite its name runs on every
  controlled contact — it is the dribble touch *and* the contest.

**Open (measurement targets, [LEGACY.md](LEGACY.md) §15):**

- **Slide reach.** How close the tackler must get for `playersTackledTheBallStrong`
  to fire — the call site condition was not traced, only the routine. The Amiga
  enters the contest from `player_tackling` (asm:44014) and is no more specific.
- **Where the possession contest is invoked from.** [player.cpp:360](../reference/swos-port/src/game/player.cpp#L360)
  is inside a larger routine whose entry conditions are unread. Does it run on every
  simultaneous arrival, or only in specific states?
- Whether the CPU's 100 %-vs-125 % ball speed is a separate code path or a flag;
  only the routine's comment asserts it. The Amiga's deflected-tackle path uses
  **150 %** in a similar shape (§12), which suggests at least three multipliers
  exist and we have not separated the cases cleanly.
- **What marks a tackle as a *deflection*.** The Amiga gates its whole alternate
  path on `Sprite` +$6A = −1 and cannot say what writes it; §3's early-release
  mechanic is a strong candidate (§12). Settling this settles §6 as well.
- Ball deflection off a player who is **not** tackling ([LEGACY.md](LEGACY.md) §15
  "deflection rules on intercepted balls") remains open; see [BALL.md](BALL.md) §10.

---

## 11. Guidance for the reimplementation

- **Model the tackle as a state with a frozen direction and a live input read.**
  The split between locked slide direction and live `currentAllowedDirection` for
  ball deflection is the mechanic. Collapse them and you lose the skill ceiling.
- **Make `tackleState` the foul discriminator.** Do not attempt a geometric or
  physical model of "did he get the ball". A three-valued flag set on contact is
  sufficient, deterministic, and matches the reference exactly.
- **Keep contests near coin flips.** The 50 %–72 % band in §8 is a design decision,
  not a limitation. Widening it to reward attributes properly will make the game
  feel less like SWOS, quickly.
- **End the slide on deceleration, not a timer**, and apply recovery from the
  Tackling table afterwards. This gives fast tackles a natural rhythm without a
  second state variable.
- **Implement the early-release shortening** (§3). It is invisible in any manual,
  it is real, and skilled play depends on it.
- **Do not reproduce the dead stores** (§2), but *do* decide consciously what
  `fasterTackle` should mean — the attribute exists in the data and currently does
  nothing. Reviving it is a balance change, so make it deliberately or not at all.
- **Model attributes as 0–7.** The table bounds worry in §8 is answered: eight
  entries is exactly right, the maximum gap is 7, and nothing is read out of range.
- **Run the whole contest inside the deterministic tick**, with the RNG drawn from
  the match stream ([AI.md](AI.md) §6), so replays stay bit-exact.
- **Charge the recovery cost.** Tackling is balanced by its downside, not by the
  odds — the odds barely move. If our tackles are cheap the whole risk model breaks.
- **Keep fouls deterministic.** Only the card draw touches the RNG (§12). Resisting
  the urge to randomise the foul itself keeps replays and traces stable and matches
  the original.

---

## 12. Amiga cross-check

Traced independently through the Amiga original — [amiga/CONTEST.md](amiga/CONTEST.md).

### Confirmed, exactly

| §9 constant | Amiga symbol | Line | Agreement |
|---|---|---|---|
| `kPlAvgTacklingBallControlDiffChance` | same name | asm:34774 | 16 … 23, out of 32 |
| `kPlayerTacklingDownTime` | `unk_1106B2` | asm:34747 | 30 … 9 by Tackling |
| `kPlayerTacklingSpeed` 1792 | `playerTacklingSpeed` | asm:30706 | $700 |
| `wonTheBallTimer` 12 | `wonBallTimer` | asm:35194 | 12 frames |
| Contest RNG | — | asm:35187 | `Rand() & 31` |
| Foul proximity `≤ 32` squared | — | asm:39118 | ≈ 5.7 units |
| Victim-was-playing-ball gate `≤ 800` | — | asm:39190 | ≈ 28 units |
| Pitch-proper box | — | — | X 81 … 590, Y 129 … 769 |
| Ground friction 96 | `playerGroundConstant` | asm:30584 | same |

The whole of §8 is confirmed structurally: the average of **Tackling and Ball
Control** on *both* sides, the absolute difference, one `Rand() & 31` against the
table, and a symmetric read — the table is "probability the better player wins",
applied to whichever side is favoured. An even contest is **exactly 50/50** and the
maximum edge is **71.9 %**, which the Amiga document calls the same design statement
§8 does. A slide launched at 1792 against friction 96 covers about 17 units and
stops in 19 frames.

The foul chain also matches §5 step for step: proximity gate, keeper exemption
(keepers cannot be fouled, ever — the tackler is merely slowed), the in-pitch
requirement, the tackler cut to a quarter speed, the `ballDistance` gate, and a
final facing comparison. Both readings agree the foul decision contains **no
randomness at all** — the only `Rand` in the whole system is the card draw
(`Rand() & 3`, asm:36100, roughly one in four and only when a difficulty flag
permits).

### Two disagreements, and they matter

1. **The from-behind test is inverted.** §5 fouls when
   `|tackler.direction − victim.direction| ≤ 1` — the two players facing within one
   octant of each other. The Amiga (asm:39178) fouls when the two octants differ by
   **more than one**, and describes the intent plainly: *"a tackle from behind or
   across is a foul; one from alongside going the same way is not."* These are exact
   complements. The Amiga reading is the one that matches football and the one that
   makes §5's own commentary ("occasionally absurd") unnecessary — a tackle where
   both players are running the same way is precisely the *clean* case. **This is
   the highest-value single item in this document to settle**, because it inverts
   the refereeing of every challenge.

2. **The flat-3 recovery table is the *deflecting tackle's*, not the CPU's.** §6
   selects `kComputerTacklingDownTime` on `tacklingTimer == -1` and notes the name
   says "computer" while the condition is reachable by human input. The Amiga has
   the identical eight-entry table of 3s (`unk_1106C2`, asm:34763) and reaches it
   from `sub_110CD8` on the **deflected-tackle** path — a tackle flagged as a
   deflection rather than a possession attempt, marked by `Sprite` +$6A = −1. Such
   a tackle skips the skill contest entirely, always disturbs the ball, never wins
   it outright, and costs a flat 3 frames for anybody.

   **These two readings fit together.** The Amiga cannot say what sets +$6A to −1
   and flags it as player-facing and important; this document has a mechanism —
   releasing fire early — that produces exactly a `−1` sentinel and selects exactly
   the flat-3 table. If they are the same thing, then §3's "early release" *is* the
   deflecting tackle: you commit to disturbing the ball rather than winning it, you
   forfeit the contest, and you get up almost immediately. That would make it a
   deliberate risk/reward choice rather than the exploit §6 suspects. **Worth one
   targeted trace; it changes how the mechanic should be presented to players.**

### The deflecting tackle, in full

Since it is absent from this document entirely (`sub_110C04`, asm:34920 region):

- picks an octant one step from the carrier's, toward the tackler;
- aims the ball there;
- halves the carrier's speed, then gives the ball `carrier.speed × 3/2`;
- sets a contact-quality flag to 1, or to **2** if the tackler was more than ~5 units
  from the ball, marking how clean the deflection was;
- runs **no skill contest**.

Compare §4: the ball leaves at `tackler.speed × 5/4` and the *tackler* is halved.
Same shape, different subject and different multiplier. Either they are two distinct
paths that both documents partially saw, or one of the two transcriptions has the
players swapped.

### `tackleState` and the deflection flag are the same offset

§1 puts `Sprite.tackleState` at **+96** with values 0 / 1 / `TS_GOOD_TACKLE` (2).
The Amiga has `field_60` — offset $60 = **96** — also 0 / 1 / 2, also a
contact-quality marker, but read on the **ball** sprite and meaning "how clean was
the deflection" rather than "did the tackler touch the ball". One offset, two
semantics, and the foul test consumes it in both readings. Resolve this alongside
disagreement (1); they are the same few instructions.

### New from the Amiga

- **The penalty-area test.** After a foul is awarded, if the victim is inside
  X 193 … 478 and Y ≤ 216 (top box) or Y ≥ 682 (bottom box), it becomes a **penalty**
  (asm:39197–39232). That box is 285 × 87 units, centred on the goal, and it is
  wider than the Finishing zone the shot classifier uses
  ([SHOOTING.md](SHOOTING.md) §9). Two different "boxes" exist; do not merge them.
- **The free-kick taker is picked by position, not role** — the fouled side's
  outfielders are scanned for whoever is nearest the opponent's goal
  (asm:39248–39292).
- **Cards escalate through a separate routine** (`sub_111388`, asm:36096) gated on
  a global *and* on `Rand() & 3`. Which offence yields yellow versus red, and
  whether a second yellow is tracked, is open on both oracles.
