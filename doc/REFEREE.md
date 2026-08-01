# REFEREE.md

The referee as an on-pitch actor: a six-state machine that walks on from off
screen, waits, shows a card, and walks off again — plus the blinking shirt-number
sprite that tells you who was booked, and the sending-off that follows a red.
Traced through the reference DOS port in
[../reference/swos-port/](../reference/swos-port/).

The **decision** to award a foul or a card is not here — that is
[TACKLING.md](TACKLING.md) §5 (the foul test) and
[SIMULATION.md](SIMULATION.md) §6 (cards, injuries, the last-man rule). This
document is what happens on screen afterwards. The restart the referee's whistle
produces is [SETPIECES.md](SETPIECES.md) §4.

> **Provenance.** [referee.cpp](../reference/swos-port/src/game/referee.cpp) is
> modern C++ with every constant named and present, so this is a high-confidence
> document. It also contains a **deliberately preserved original bug** (§4) that the
> porters reproduce under test — rare and valuable evidence about the original
> binary's data layout. Read to understand the design; write our own code.

---

## 0. One-paragraph version

The referee is a `Sprite` that lives at a **hiding place** off the pitch and is
summoned by `activateReferee()`. He enters from the top or bottom edge of the
*current camera view* — not the pitch — at a randomised x offset, walks to a point
five units below and twenty-eight to the right of the foul spot, turns to face
left, and waits. When a card is due he switches to the matching card animation and
the booked player's **shirt number blinks above his head** on a fixed 30-entry
schedule; when that schedule hits its terminator the referee turns and walks off
toward the *opposite* end of the pitch from the foul, and a red card sends the
player walking to `(-20, 449)`, off the left edge of the world. While any of this
is happening `refereeActive()` blocks the bench ([BENCH.md](BENCH.md) §3) and
`cardHandingInProgress()` takes over the camera ([CAMERA.md](CAMERA.md) §2).

---

## 1. States

```
enum RefereeState {
    kRefOffScreen      = 0,   // not drawn
    kRefIncoming       = 1,   // walking to the foul
    kRefWaitingPlayer  = 2,   // standing, facing left
    kRefAboutToGiveCard= 3,   // one-tick transition
    kRefBooking        = 4,   // showing the card, number blinking
    kRefLeaving        = 5,   // walking off
};

enum CardHanding {
    kNoCard = 0, kYellowCard = 1, kRedCard = 2, kSecondYellowCard = 3,
};
```

The disassembly's own annotation of `refState`
([swos.asm](../reference/swos-port/swos/swos.asm)) reads: *"0 = dont draw referee,
1 = referee incoming, 2 = waiting player, 3 = very short, when red card, 4 =
booking player and showing card, 5 = leaving"*. Note the comment says state 3 is
*"when red card"*, but
[updateRefereeState()](../reference/swos-port/src/game/referee.cpp#L192) enters it
for **all three** card kinds. Trust the code.

Two sprites, both constructed with `teamNumber = 3` — the source explains why:
*"set team number so they show up in replays"*.

| Sprite | Purpose |
|---|---|
| `m_refereeSprite` | The referee himself |
| `m_bookedPlayerNumberSprite` | The blinking shirt number above the booked player |

Two globals drive it: `swos.refState` and `swos.whichCard`, plus
`swos.bookedPlayer`, `swos.lastTeamBooked` and `swos.refTimer`.

---

## 2. Arrival

[activateReferee()](../reference/swos-port/src/game/referee.cpp#L50):

```
destX = foulXCoordinate + 28
destY = foulYCoordinate + 5

xOffset = rand() / 8
if (foulXCoordinate >= 336) xOffset = -xOffset      // approach from the inside

refStartY = cameraY - 20                             // enter from the top
if (foulYCoordinate <= 449) refStartY = cameraY + 215   // enter from the bottom

x     = destX + xOffset
y     = refStartY
speed = kRefereeSpeed                                // 1024
show(); initDisplaySprites()
animation = refComingAnimTable
refState  = kRefIncoming
```

Three things worth pulling out:

- **He enters relative to the camera, not the pitch.** `cameraY - 20` and
  `cameraY + 215` are screen-space edges. The referee materialises just outside the
  visible area and walks in, so he is never seen popping into existence regardless
  of where on the pitch the foul was.
- **The entry edge is chosen by pitch half**, so he always walks *toward* the centre
  rather than away — a foul in the top half is approached from below.
- **`rand() / 8` randomises his approach line**, negated in the right half so the
  offset always points inward. Two identical fouls do not produce identical
  referee paths. This consumes from the match RNG stream
  ([AI.md](AI.md) §6).

`kRefereeSpeed = 1024` is deliberately below a player's 1250 running maximum
([MOVEMENT.md](MOVEMENT.md) §10) — the referee is the slowest mover on the pitch.

---

## 3. Walking, and the update gate

[updateReferee()](../reference/swos-port/src/game/referee.cpp#L87):

```
if (refereeActive() && visible) {
    if (onScreen)                              { updateSpriteAnimation(); updateRefereeState(); }
    else if (gameStatePl == kInProgress)       { moveSprite(); }          // move only
    else                                       { updateRefereeState(); }
}
```

**An off-screen referee during live play is moved but not state-updated.** He
keeps walking toward his destination but cannot change state until he is either
visible or play has stopped. This is how the engine avoids the referee completing
his whole booking sequence out of shot.

[updateRefereeState()](../reference/swos-port/src/game/referee.cpp#L192) handles
`kRefIncoming` and `kRefLeaving` with **shared code via a deliberate fall-through**:

```
case kRefLeaving:
    if (!onScreen) { refState = kRefOffScreen; hide(); initDisplaySprites(); break; }
    // fall-through
case kRefIncoming:
    speed = kRefereeSpeed
    oldDirection = direction
    updateSpriteDirectionAndDeltas()
    if (oldDirection != direction) initRefereeAnimationTable(refComingAnimTable)
    moveSprite()
    if (stationary()) {
        refState  = kRefWaitingPlayer
        direction = kFacingLeft
        animation = refWaitingAnimTable
    }
```

So leaving and arriving are the same walk. The animation table is **re-initialised
whenever the direction changes**, which is what makes the referee's sprite turn as
he curves toward his destination ([PLAYER_SPRITES.md](PLAYER_SPRITES.md) §6).

Note the `stationary()` transition applies on the leaving path too — if a departing
referee reaches his destination while still on screen, he flips to
`kRefWaitingPlayer` and stands there facing left. Presumably unreachable in
practice, since his leaving destination is off the pitch.

---

## 4. The card and the blinking number

[updateBookedPlayerNumberSprite()](../reference/swos-port/src/game/referee.cpp#L101).
Runs only while `whichCard != 0`, `refState == kRefBooking` and the booked player
is in state `kBooked`.

```
kPlayerNumberBlinkTable[30] = {
    0, 9, 0, 9, 0, 9, 0, 9, 0, 9, 0, 9, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1,
};

refTimer += lastFrameTicks
action = kPlayerNumberBlinkTable[refTimer >> 3]

if (action > 0) {
    imageIndex = kSmallDigit1 + player.shirtNumber - 1
    numberSprite.setImage(imageIndex)
    numberSprite.x, y = bookedPlayer.x, y
    numberSprite.z    = kPlayerNumberOffset          // 20 — floats above his head
} else if (action < 0) {
    putRefereeToLeavingState()
    if (whichCard & kRedCard) sendPlayerAway()
    whichCard = 0; bookedPlayer.reset()
}
```

The table is a **tiny animation bytecode**, the same idea as the frame stepper in
[PLAYER_SPRITES.md](PLAYER_SPRITES.md) §7: `0` = draw nothing, `9` = draw the
number, `-1` = sequence over. Read across, it blinks **six times** (12 entries
alternating), then holds blank for 17 entries, then terminates. Timing is
`refTimer >> 3` where `refTimer` accumulates `lastFrameTicks` — so it is
wall-clock driven, not tick-driven.

The `z = 20` lift uses the same height mechanism as a jumping player
([PLAYER_SPRITES.md](PLAYER_SPRITES.md) §10) to float the digit above the head.

### The preserved bug

[:117-125](../reference/swos-port/src/game/referee.cpp#L117-L125):

```cpp
// emulate SWOS bug: the table for second yellow card is 3 bytes short,
// and accessing it randomly brings in the bytes from the array that follows it
constexpr int kFaultySize = 27;
if (whichCard != kRedCard && whichCard != kYellowCard && index >= kFaultySize) {
    static const std::array<int8_t, 3> kJunkBytes = { 96, 3, -98, };
    action = kJunkBytes[index - kFaultySize];
}
```

**The original's second-yellow-card blink table is three bytes short**, so the last
three reads run off the end into whatever data followed — `96, 3, -98`. Since
`-98 < 0`, the sequence still terminates, one entry earlier than intended, and a
stray `96 > 0` draws the number for one extra frame. The porters identified the
overrun bytes and reproduce them under `SWOS_TEST`.

This is worth recording for two reasons. It belongs in [LEGACY.md](LEGACY.md) §14
as a genuine original-binary bug. And it is direct evidence about the original's
data-segment layout — the three bytes immediately following that table are known.

---

## 5. Leaving, and the red card

[putRefereeToLeavingState()](../reference/swos-port/src/game/referee.cpp#L247):

```
xOffset = rand() / 4 - 32              // -32..31
destX   = x.whole() + xOffset
destY   = (foulYCoordinate > 449) ? 129 : 770
animation = refComingAnimTable
refState  = kRefLeaving
```

**The referee leaves toward the far end of the pitch.** A foul in the lower half
sends him to `y = 129` (the top goal line); a foul in the upper half sends him to
`y = 770`. He walks the length of the pitch rather than to the nearest touchline.
A second RNG draw randomises his exit line by ±32.

[sendPlayerAway()](../reference/swos-port/src/game/referee.cpp#L260), on a red:

```
if (gameTeam->markedPlayer == player.playerOrdinal - 1)
    gameTeam->markedPlayer = -1               // clear man-marking assignment

player.cards    = -1
player.sentAway = 1
player.destX    = -20                          // kSentOffPlayerX
player.destY    = 449                          // kSentOffPlayerY
```

- **`cards = -1`** is the sent-off sentinel, distinct from any positive card count.
- **Destination `(-20, 449)`** is outside the pitch entirely — negative x. The
  player walks off the left edge of the world under normal movement
  ([MOVEMENT.md](MOVEMENT.md) §7) and simply keeps going.
- **The man-marking assignment is cleared** if the sent-off player was the marker,
  the same fix-up `maintainMarkedPlayer()` performs for substitutions
  ([BENCH.md](BENCH.md) §5).

Note `sentAway` and `cards` are both cleared again if that player is later
substituted ([BENCH.md](BENCH.md) §5) — which cannot legitimately happen, but the
code does not prevent it.

---

## 6. The hiding place

[removeReferee()](../reference/swos-port/src/game/referee.cpp#L163) does not destroy
the sprite; it **parks** it:

```
refState = kRefOffScreen; hide()
x, y     = (276, 439)                  // kRefereeHidingPlace
z        = 0
destX/Y  = same
speed    = 0
playerDownTimer = 0
frameIndex = -1;  cycleFramesTimer = 1
clearImage(); direction = kFacingTop; onScreen = 1
animation = refWaitingAnimTable
```

`(276, 439)` is just left of and above the centre spot `(336, 449)` — inside the
pitch, but the sprite is hidden and imageless. A single persistent sprite reused
for every booking, fully reset on retirement. Note `onScreen = 1` on a hidden
sprite, which matters for the `updateReferee` gate in §3.

---

## 7. Constants quick reference

| Constant | Value | Meaning |
|---|---|---|
| `kRefereeSpeed` | 1024 | Below a player's 1250 max |
| `kRefereeHidingPlaceX/Y` | 276, 439 | Parked position when inactive |
| `kRefereeLeavingTopDestY` | 129 | Exit toward top goal line |
| `kRefereeLeavingBottomDestY` | 770 | Exit toward bottom goal line |
| `kSentOffPlayerX/Y` | −20, 449 | Off the left edge of the world |
| `kPlayerNumberOffset` | 20 | `z` lift of the blinking digit |
| Arrival offset from foul | `+28, +5` | |
| Arrival x jitter | `rand() / 8`, inward | |
| Entry y | `cameraY − 20` or `cameraY + 215` | Camera-relative |
| Departure x jitter | `rand() / 4 − 32` | −32..31 |
| `kPlayerNumberBlinkTable` | 30 entries, 6 blinks then hold then `-1` | |
| Blink timing | `refTimer >> 3`, `refTimer += lastFrameTicks` | Wall-clock |
| Second-yellow table overrun | `96, 3, -98` | **Original bug** |
| Sprite `teamNumber` | 3 | So the referee appears in replays |

---

## 8. What this resolves, and what still needs measurement

**Confirmed as structure:**

- Six referee states; one persistent sprite parked at a hiding place rather than
  created and destroyed. ✓
- Entry is **camera-relative**, from whichever edge faces the foul, with a
  randomised inward x offset. ✓
- Referee speed 1024, slower than any player. ✓
- Incoming and leaving share one walk implementation via fall-through; the animation
  table is re-initialised on every direction change. ✓
- Off-screen referee during live play is moved but not state-advanced. ✓
- The shirt number is a separate sprite lifted by `z = 20`, animated by a 30-entry
  bytecode: six blinks, hold, terminate. ✓
- Blink timing is wall-clock (`lastFrameTicks`), not simulation ticks. ✓
- The second-yellow blink table overruns by three bytes in the original —
  `96, 3, -98`. ✓
- Referee exits toward the **far** end of the pitch from the foul. ✓
- A sent-off player walks to `(−20, 449)`, gets `cards = -1`, and has any marking
  assignment cleared. ✓
- Both sprites carry `teamNumber = 3` so replays render them. ✓

**Open (measurement targets, [LEGACY.md](LEGACY.md) §15):**

- **Who calls `activateReferee()`, and when.** The trigger — presumably from the
  foul path in [SIMULATION.md](SIMULATION.md) §6 — is not in this file. In
  particular, whether the referee appears for every foul or only for card-worthy
  ones.
- **What sets `whichCard` and `bookedPlayer`**, and how `kSecondYellowCard` is
  distinguished from a straight red at the decision site.
- **`kRefWaitingPlayer` → `kRefAboutToGiveCard`** — the transition out of waiting is
  not in `updateRefereeState()`. Something external advances it; presumably it waits
  for the booked player to arrive, which would explain the state's name.
- Whether the blink table being wall-clock driven makes booking duration
  **frame-rate dependent** — and if so, whether that differs between the Amiga and
  DOS builds' tick rates ([BALL.md](BALL.md) §9).
- The referee animation tables (`refComingAnimTable`, `refWaitingAnimTable`,
  `refYellowCardAnimTable`, `refRedCardAnimTable`, `refSecondYellowAnimTable`) —
  contents not read.
- `initDisplaySprites()` — called on both arrival and departure, purpose unclear.
- Whether the referee has any influence on play at all (collision, obstruction), or
  is purely decorative. Nothing here suggests he does.
- The `stationary()` transition on the leaving path (§3) — reachable or not?

---

## 9. Guidance for the reimplementation

- **Park the sprite, don't allocate it.** One referee, fully reset by a single
  `removeReferee()`. It removes a whole class of lifetime bugs and it is what the
  reference does.
- **Enter relative to the camera, not the pitch.** This is the detail that makes the
  referee feel like he was always there. It costs two lines and it is invisible
  when done right.
- **Keep the blink table as data.** A tiny bytecode driving a sprite is the same
  pattern as [PLAYER_SPRITES.md](PLAYER_SPRITES.md) §7 — use one shared stepper for
  both rather than writing a bespoke blink timer.
- **Do not reproduce the table overrun** (§4), but **do** record it in
  [LEGACY.md](LEGACY.md) §14. It is a bug with no gameplay meaning, unlike most SWOS
  quirks. The porters only emulate it under test, and that is the right call.
- **Decide whether booking duration should be wall-clock or tick-driven.** The
  reference uses wall-clock, which means it is not deterministic across frame rates
  and therefore cannot live inside `at_core`. Keeping the referee entirely outside
  the deterministic tick — as [CAMERA.md](CAMERA.md) §11 recommends for the camera —
  is the clean answer, with the two RNG draws hoisted into core.
- **Route marking-assignment cleanup through one function.** Both this document §5
  and [BENCH.md](BENCH.md) §5 clear `markedPlayer`; they should not be two
  implementations.
- **Make the referee purely presentational** unless a trace proves otherwise. Giving
  him collision is a change, not a port.
