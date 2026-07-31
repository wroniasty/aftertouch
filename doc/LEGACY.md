# LEGACY.md

Everything known about the original game's mechanics, gathered as a reference for
reimplementation. Companion to `PLAN.md`.

## How to read this document

Every claim carries a confidence tag. This matters more than the claims themselves,
because the tags tell you what you can code against and what you have to measure.

| Tag | Meaning |
|---|---|
| `[MANUAL]` | Stated in the official game manual. Authoritative for intent, not necessarily for exact numbers. |
| `[DATA]` | Established from file format reverse engineering. Reliable. |
| `[COMMUNITY]` | Consensus among long-term players and modders. Usually right, occasionally folklore. |
| `[DISPUTED]` | Community disagrees. Do not code against this. Measure it. |
| `[UNKNOWN]` | Nobody has published it. Must come out of the trace harness. |

**The `[UNKNOWN]` entries are the actual work.** Section 15 collects them into a
checklist. Everything above that section is scaffolding that tells you what to
measure and roughly what answer to expect.

A rule for the whole document: no numeric constant here should be pasted into the
engine as fact. Every one of them is a starting guess for a parameter that gets
fitted against reference traces.

---

## 1. Which original are you cloning?

`[COMMUNITY]` The Amiga and DOS builds of 96/97 do **not** play identically. The
DOS release is a port by Wave Software, not the same binary, and the physics
diverge. SWOS 2020 ships both as user-selectable modes precisely because the
community would not accept one.

Competitive players overwhelmingly treat the **Amiga** build as canonical. Tournaments
are played on it. If you are going to match one, match that one.

`[DATA]` Version lineage relevant to mechanics:

- Sensible Soccer (1992): the base engine
- SWOS (1994): tactics grid, career mode, the full database
- SWOS 95/96 Edition: added standing headers and low passes with curl, carried forward
- SWOS 96/97: final release. Amiga on two floppies, PC on CD. Data update plus tweaks.
- Sensible Soccer '98: different engine. Ignore entirely.

**Decision to record here before you write physics code:**

```
Reference build: ____________  (Amiga 96/97 recommended)
Reference binary/checksum: ____________
```

---

## 2. Presentation

`[DATA]` Amiga: 320x256 PAL, planar bitmap playfield, hardware scrolling, blitter
bobs with cookie-cut masks, locked to the 50 Hz vertical blank.

`[DATA]` DOS: 320x200, 256-colour VGA, DOS/4GW protected mode, compiled with Watcom
C, with smooth scrolling.

`[COMMUNITY]` Player sprites are roughly 16 pixels tall. The small scale is a
deliberate design choice, not a memory constraint: it puts a large slice of pitch on
screen so passing lanes are visible. **This is the single most important constraint
in the whole game.**

`[DATA]` Player sprites are **layered**, not flat. The extracted asset tree from the
DOS port splits `player/`, `goalkeeper/` and `bench/` and ships a layer-processing
step. Kit rendering is a body layer composited with separately tinted shirt, shorts
and sock layers, not a palette swap over a single flat sprite.

`[MANUAL]` Kit editing offers a primary shirt colour and a secondary colour for
sleeves or stripes, plus separate shorts and socks colours.

`[COMMUNITY]` Camera is orthographic top-down with the pitch oriented vertically. The
apparent tilt is purely in the sprite art. Camera follows the ball.

`[UNKNOWN]` Camera follow behaviour: dead zone size, lag/lead, and clamping at pitch
edges. Measure from traces.

---

## 3. Controls

This is the heart of the game and the part you cannot get approximately right.

### Input model

`[MANUAL]` **One button.** Eight digital directions. Nothing else.

`[COMMUNITY]` **No sprint button.** Players always run at full pace. This is
load-bearing: adding a sprint changes the entire risk model of dribbling. Sociable
Soccer added one and the community's most common complaint was exactly this.

`[COMMUNITY]` Aftertouch exists partly *because* movement is limited to eight
directions. It is the mechanism that gives you continuous aim on top of discrete
input. Analog input would make aftertouch redundant and break the game.

`[MANUAL]` Player selection is automatic. You cannot cycle manually.

`[COMMUNITY]` The goalkeeper cannot be controlled directly, including at penalties.

### With the ball

`[MANUAL]` Move in any of eight directions to run with the ball. The ball is not
attached to the player's feet; the faster you run, the harder it is to keep control.

`[MANUAL]` **Tap fire** = pass. The player attempts a pass to the nearest team-mate
in the general direction he is facing.

`[MANUAL]` **Hold fire** = kick straight in the facing direction. Longer hold means
harder kick.

`[MANUAL]` **Keep holding fire after the kick** = the ball passes through your own
players instead of being collected by them. This is a distinct mechanic and easy to
miss when reimplementing.

### Aftertouch

`[MANUAL]` Move the stick **after** the kick to bend, lob or drive. The sooner you
move after the kick, the stronger the effect.

`[MANUAL]` When attacking the goal at the top of the screen:

| Stick after kick | Result |
|---|---|
| Up | Ball stays low along the ground |
| Down | Ball goes high in the air |
| Vertically centred | Lob |
| Left or right (combined with the above) | Curve in that direction |

Reversed when attacking the bottom goal. Aftertouch applies to passes as well as
shots.

`[UNKNOWN]` The aftertouch window length in ticks, the decay curve of effect strength
against elapsed ticks, and the magnitude of lateral acceleration applied. This is
probably the single highest-value thing to extract from traces, because it is what
players mean when they say a clone "feels wrong".

### Without the ball

`[MANUAL]` **Press fire without possession** = sliding tackle.

`[MANUAL]` **Press fire when the ball is in the air nearby** = jump and attempt a
header. Deflection and travel-distance rules match the sliding tackle case.

`[COMMUNITY]` **Running tackle**: run into an opponent without pressing anything. The
outcome is influenced by the tackler's Tackling and the carrier's Control. Community
advice is that this is the safer way to win the ball, since a missed slide leaves you
out of the play.

`[UNKNOWN]` Exact contest resolution: what is compared, with what weights, and how
much randomness is involved.

### Non-play controls

`[MANUAL]` Bring up the manager's bench by tapping the **same direction three times
in quick succession** while the ball is out of play. Six figures appear: the manager
at the top and five substitutes below.

`[MANUAL]` Keyboard functions during a match:

| Key | Function |
|---|---|
| `P` | Pause / unpause |
| `R` | Replay the last few seconds |
| `R` during replay | Toggle slow motion |
| `Space` | Save the last few seconds as a highlight for the end of the match |
| `H` | Watch saved highlights (only at the full-time score screen) |
| `S` | In-match statistics (only while the ball is out of play) |
| `F9` | Toggle the spinning logo (A1200 version only) |
| `F10` | Toggle crowd chants |
| `Esc` | Abandon the match |

`[MANUAL]` Abandoning at 0 minutes lets you replay the match. Abandoning later is an
automatic loss.

---

## 4. Substitutions and in-match management

`[MANUAL]` Default allowance: two outfield substitutions plus one goalkeeper change.

`[MANUAL]` If the competition was set up with the "two substitutes" option, you get
two changes total, including any goalkeeper change.

`[MANUAL]` In any competition other than a two-substitute one, you must name a
goalkeeper on the bench at kickoff.

`[MANUAL]` Tactics can be changed as many times as you like during a match, via the
manager figure on the bench.

`[COMMUNITY]` If you have no goalkeeper available, an outfield player can go in goal.
Some players deliberately never bench a keeper for this reason.

---

## 5. Ball physics

`[COMMUNITY]` The ball has a height coordinate and a full 3D velocity. It is drawn
offset vertically by its height with the shadow left at ground position. Players also
have height, for jumps and headers.

`[UNKNOWN]` Everything numeric:

- Fixed-point format and the scale of pitch coordinates in internal units
- Gravity constant
- Restitution (bounce) and rolling friction, per pitch type
- Air resistance, if any
- Kick power curve: mapping from fire-hold duration in ticks to launch velocity
- Launch elevation angle as a function of hold duration and context
- Curl: magnitude and decay of lateral acceleration during flight
- Capture radius: how close a player must be to take possession
- Dribble kick distance: how far ahead of a running player the ball is placed
- How Control affects that distance, which is the mechanical meaning of "the ball is
  not glued to his feet"

Every item above is a fitting target for the trace harness.

---

## 6. Pitch types

`[COMMUNITY]` Seven physical types plus two selection modes:

**Frozen, Muddy, Wet, Soft, Normal, Dry, Hard**, plus **Seasonal** and **Random**.

`[DATA]` The match data holds **two separate fields**: one for the pitch's appearance
and one for its physics. They can differ. Model them as independent from the start;
retrofitting that split is annoying.

`[COMMUNITY]` Physics behaviour, from community testing:

| Type | Ball behaviour | Notes |
|---|---|---|
| Muddy | Slowest, least bounce | Ball control degraded |
| Soft | Reportedly as Normal | See DISPUTED below |
| Normal | Benchmark | Roughly 30% of Random rolls |
| Dry | Reportedly as Normal | Different look only |
| Hard | Reportedly as Normal, higher bounce | Higher injury risk than Dry/Normal |
| Wet | Faster than Normal | |
| Frozen | Fastest, ball slides and bounces furthest | Worst ball control, highest injury risk, long shots favoured |

`[DISPUTED]` Whether Soft, Dry and Hard genuinely share Normal's physics or only look
similar is contested within the community. Treat all seven as separate parameter sets
in your data model even if some end up identical after measurement.

`[COMMUNITY]` Seasonal progression, roughly in descending likelihood:

| Season | Types |
|---|---|
| Spring | Normal, Soft, Dry, Hard |
| Summer | Hard, Dry, Normal, Soft |
| Autumn | Soft, Muddy, Wet, Frozen |
| Winter | Frozen, Wet, Muddy, Soft |

`[COMMUNITY]` Frozen favours shooting from distance, so the Velocity attribute pays
off disproportionately there. A good sanity check once your attribute modifiers are
wired: long-shot conversion should visibly rise on Frozen.

---

## 7. Tactics: the off-ball AI

The most interesting piece of engineering in the original, and pure data rather than
code.

`[MANUAL]` Ten preset tactics covering common formations, plus six custom slots named
User A through User F.

`[MANUAL]` `[DATA]` The grid: **35 areas the ball can be in**, and **240 positions a
player can be in**. 35 is a 5-wide by 7-deep ball grid. 240 is a 15 by 16 player grid.

`[DATA]` The `.tac` file layout:

```
offset  size  content
0       8     tactic name
8       1     unused (00)
9       35    outfield player 1 target per ball zone
44      35    outfield player 2
...           ... ten outfield players total, goalkeeper excluded
359     10    flip flags, one per player (FF = off)
369     1     base tactic identifier
```

`[DATA]` Each of the 35 bytes per player encodes a target position with **X in the
high nibble and Y in the low nibble**. A value of `0x7F` means column 7, row 15.

`[COMMUNITY]` Player slot order in the file corresponds roughly to RB, D, D/M, LB,
RW, D/M, M/A, LW, M/A, A, though which slot is which depends on the base tactic.

`[DATA]` Older analyses give an indexing formula of `(player * 35) - 26 + (zone - 1)`
for locating a byte. Verify against your own parsing rather than trusting it; the
off-by-one conventions differ between write-ups.

`[COMMUNITY]` Trailing bytes appear to be a fixed magic sequence used for file
validation.

### The algorithm

`[COMMUNITY]` Reduces to: determine which of 35 zones the ball is in, read one byte
per outfield player, steer that player toward the decoded point, mirror for the team
playing the other direction. No pathfinding, no utility scoring, no state machines.
The entire off-ball behaviour of a team is roughly 350 bytes.

`[UNKNOWN]` What "steer toward" actually means mechanically: whether players move at
full speed toward the target, whether there is an arrival radius, how the target
interacts with ball-chasing, and what overrides positional play (nearest-to-ball,
marking, offside line).

### Green ticks and red crosses

`[DISPUTED]` **This one matters and the community does not agree.**

One position: the tick is purely a career-mode indicator meaning the player will gain
value in that position, with no effect on match performance.

The other position: tick coverage affects in-match performance, with players in
poorly-ticked positions being slower and having worse ball control, and a threshold
around 25 to 27 ticked zones out of 35 for a player to reach full effectiveness.

These cannot both be true. Resolve it by measurement: run identical inputs with a
player at high and low tick coverage and diff the traces. Do not implement either
version until you have.

---

## 8. The on-ball AI

The least documented part of the entire game, and the part with the most work in it.

The tactics grid is documented because it is user-editable and lives in a file
format. The on-ball decision layer is pure code with no data file, so essentially
nothing has been published about it. What follows is one structural finding, two
empirical fingerprints, and a method for extracting the rest.

### The structural finding: the AI is a virtual joystick

`[COMMUNITY]` The computer team uses the **same tactics system** as the human team.
Community efforts to improve the CPU consist of writing better tactics and patching
them into the executable, which is where the CPU's tactic set lives. There is no
separate positional AI.

`[COMMUNITY]` The one behaviour anyone suspects of cheating is shooting: the theory
is that the CPU can trigger a shot without holding fire for the required number of
frames, in the way that CPU characters in fighting games of the era skipped input
motions. Note what that implies. It is described as an *exception*, which is evidence
that everything else goes through the normal input path.

**Model every AI player as emitting the same `(direction, fire_state)` a human
joystick would emit, and nothing else.** No direct writes to ball velocity, no
privileged actions, no separate physics path. The AI is 21 synthetic controllers
plus whatever the human is holding.

Three consequences worth acting on:

1. **The AI lives inside `at_core`, not in the shell.** It is part of the simulation.
   Determinism requires it and headless league simulation is impossible without it.
2. **The engine's internal input representation is per-player**, 22 `PlayerInput`
   values per tick, of which one or two come from real hardware. The `MatchInput`
   struct in `PLAN.md` is the external API; internally it expands.
3. **Any AI behaviour you cannot reproduce with a human input sequence is a bug**
   in your reimplementation, and that is a testable invariant.

### Empirical fingerprint 1: the CPU scores from rebounds

`[COMMUNITY]` A long-standing observation is that a large majority of computer goals
come from shots on rebounds, in a way human players essentially never replicate.

This is diagnostic, not trivia. It is what you would see if the shoot condition is
evaluated **every tick** for whichever CPU player is nearest a loose ball, with no
reaction delay. A human needs perception and reaction time; a per-tick predicate does
not. The CPU fires the instant a rebound enters its shooting envelope.

If your reimplementation does not show this bias, your shoot trigger has a delay,
hysteresis or cooldown the original does not have.

### Empirical fingerprint 2: the CPU does not close down

`[COMMUNITY]` A common complaint is that the CPU retreats to its tactical position
instead of pressing a nearby attacker. Players describe defenders falling back when
an opponent is in front of them.

This says the tactic target **dominates**, and that the only meaningful override is
whatever selects the ball chaser. There is little or no dynamic marking, pressing or
closing-down layer on top of the grid. That is a much simpler AI than most people
assume, and it is good news for reimplementation.

`[UNKNOWN]` Whether there is any override at all beyond ball pursuit. Test by putting
an attacker next to a stationary defender whose tactic target is elsewhere, and
watching whether the defender moves.

### Ball pursuit

`[UNKNOWN]` Everything. Specifically:

- **How many players chase.** One, or the nearest few, or role-dependent.
- **Chase target.** Current ball position (naive pursuit) or a predicted intercept
  point. These look completely different in motion, and predicted intercept is the
  more likely answer given how the game reads, but nobody has confirmed it.
- **Selection metric.** Euclidean distance, or estimated time-to-ball accounting for
  the player's Speed and current velocity.
- **Hysteresis.** Whether chaser selection is re-evaluated every tick or sticky. It
  must be sticky or damped in some way: pure per-tick nearest-player selection makes
  two equidistant players dither visibly, and the original does not dither.
- **Whether the goalkeeper is in the pool** and under what conditions he leaves his
  line.
- **Human team selection.** Which of your players the game gives you and when it
  switches. Same problem, and probably the same code.

### The shoot decision

`[COMMUNITY]` For the human, shoot versus pass is decided by how long fire is held.
The community estimate is a threshold around 4 frames, with anything shorter being a
pass. **Treat the number as a guess.** It is exactly the kind of constant that reads
plausibly and is wrong.

`[UNKNOWN]` For the CPU, the candidate inputs to a shoot predicate are:

- Distance to the opposing goal
- Angle to goal, or whether the goal is inside a facing cone
- Distance to the nearest opponent (pressure)
- Ball height and whether it is controllable
- Whether the player is inside the penalty area, which matters because Finishing and
  Velocity apply to different zones
- Possession state, including loose-ball proximity, per fingerprint 1

`[UNKNOWN]` Whether shot power is chosen at all, or whether CPU shots use a fixed
power, or power derived from distance.

### The pass decision

`[MANUAL]` The human pass rule is: nearest team-mate in the general direction the
player is facing.

`[UNKNOWN]` Whether the CPU uses the same rule. **This is worth testing early because
it collapses the problem.** If the CPU is subject to the same nearest-in-cone
resolution, then its only real decision is *which direction to face*, and passing
reduces from target selection to direction selection. That is a much smaller thing
to fit.

Test: log every CPU pass and check whether the receiver was ever someone other than
the nearest team-mate within the facing cone. A single counterexample kills the
hypothesis.

`[UNKNOWN]` If the CPU does search for targets: cone half-angle, distance weighting,
whether opponents blocking the lane are considered, and tie-breaking.

### Method: fit the decisions, do not guess them

You are not going to derive these rules by reasoning about football. Extract them.

**Step 1: instrument the reference to dump synthesized inputs, not just state.**
This is harder than dumping positions, because you have to locate where the AI writes
its per-player controller values rather than just reading a state struct. It is worth
the effort, and it is the highest-return instrumentation work in the project.

**Step 2: diff in two layers.**

| Inputs | Physics | Diagnosis |
|---|---|---|
| match | match | Correct |
| match | diverge | Physics bug. AI is fine. |
| diverge | (irrelevant) | AI bug. Stop looking at physics. |

Without this split, a divergence at tick 340 tells you nothing about which subsystem
is wrong, and you will burn days on the wrong one. With it, localization is instant.
Build the input layer into the trace format from the start rather than adding it
later.

**Step 3: log a feature vector per player per tick.**

Distance to ball, time-to-intercept, distance to each goal, angle to opposing goal,
distance and bearing to nearest opponent, ball height, possession flag, tactic target,
offset from tactic target, and the emitted action.

**Step 4: scatter-plot the action against each feature.**

This was written in 68000 assembly and Watcom C in the early nineties. The decisions
are almost certainly integer comparisons against hard-coded thresholds, not anything
fitted or weighted. **Hard thresholds appear as crisp boundaries in a scatter plot.**
Find the boundary, read off the constant, move on.

If a boundary comes out fuzzy rather than crisp, that is not noise. It means a random
term is involved, which leads to the next point.

### The RNG is a prerequisite

`[UNKNOWN]` Whether the AI decisions involve randomness at all, and if so, which
generator and how it is seeded.

If there is any random component anywhere in the simulation, you must reproduce the
**exact** generator and its seeding to get traces that diff cleanly. An
almost-right PRNG produces traces that agree for a few dozen ticks and then diverge
irrecoverably, which looks exactly like a physics bug and is not one.

Find it early. Era-typical candidates are a linear congruential generator or a small
shift-and-xor sequence, usually 16 or 32 bit, often visible as a suspicious multiply
and add near decision code. Confirm by replaying the same match twice from the same
state and checking whether it is bit-identical: if it is, either there is no RNG or
it is seeded deterministically, and both are good news.

Whatever it turns out to be, put it in `at_core` behind an explicit seed that the
caller supplies. Never call a platform RNG from the simulation.

---

## 9. Player attributes

`[DATA]` `[MANUAL]` Seven skills, stored in this order in the team file:

| Letter | Name | Meaning |
|---|---|---|
| P | Passing | Precision and speed of passes |
| V | Velocity | Shot power from **outside** the penalty area |
| H | Heading | Precision and power of headers |
| T | Tackling | Winning the ball, both slides and running contact |
| C | Control | Dribbling and turning with the ball |
| S | Speed | Movement speed |
| F | Finishing | Precision and power of shots from **inside** the area |

`[COMMUNITY]` Stored as 4-bit values 0 to 15, but the effective scale is 0 to 7: the
high bit is redundant, so 8 behaves as 0, 9 as 1, up to 15 as 7. Whatever the high
bit originally meant, it is not skill magnitude. Preserve it in your importer rather
than masking it away; it may turn out to be a flag.

`[COMMUNITY]` **Goalkeepers have no skill values at all.** Keeper quality is derived
from the player's transfer value. If you want a better keeper model, that is a place
where deviating from the original is clearly an improvement rather than a betrayal.

`[COMMUNITY]` The three letters displayed beside a player's name are simply his three
highest attributes, computed for display. Not a stored field.

`[UNKNOWN]` How each attribute maps to a numeric modifier in the simulation. Speed
presumably scales max velocity, but by how much per point, and linearly or not, is
unpublished. Same for every other attribute. This is a large block of measurement
work and it is unavoidable.

### Other player fields

`[DATA]` Nationality, shirt number (1 to 16), name, a combined hair and skin colour
code, position, the seven skills, and transfer value (roughly £25k to £15m+).

`[COMMUNITY]` Positions: G, D, DM (shown as D/M), M, AM (shown as M/A), A, with
lateral variants for full backs and wingers.

---

## 10. Team and competition data

`[DATA]` Teams live in `team.###` files, one per country, numbered by nation
(`team.000` Albania, `team.008` England, and so on). File layout: a header giving the
team count, then per team a header with name and kit colours, then the player records.

`[COMMUNITY]` 96/97 ships roughly 1500 clubs and 131 national sides.

`[COMMUNITY]` A squad is 16 players. A career start includes 2 reserves and 8
trialists with generated names.

---

## 11. Match flow and rules

`[COMMUNITY]` Match length in career mode is fixed and not user-adjustable. This is
one of the most common complaints about the original and an obvious candidate for
improvement.

`[COMMUNITY]` Injuries occur, most often from sliding tackles, and can cost a player
a long spell. Injury likelihood rises on Hard and especially Frozen pitches.

`[UNKNOWN]` Foul, card and injury probabilities and their inputs (tackle angle,
relative speed, Tackling attribute, pitch type).

`[UNKNOWN]` Offside implementation, or whether there is one.

`[UNKNOWN]` Set piece handling: corners, free kicks, throw-ins, penalties, and how
much of each is scripted versus simulated.

---

## 12. Replays and highlights

`[MANUAL]` `R` replays the last few seconds live, with a slow-motion toggle. `Space`
commits the last few seconds to a highlights reel viewable at full time with `H`.

`[COMMUNITY]` Saved replay files are small, which means the game records **simulation
state**, not video. That is only possible because the simulation is deterministic at
a fixed timestep.

**Implementation note:** this confirms the architecture in `PLAN.md`. A ring buffer of
`MatchState` snapshots gives you replays, highlights and trace dumping from one
mechanism. Build the ring buffer early; it is also your debugging tool.

---

## 13. Rendering order and depth

`[COMMUNITY]` Draw order is shadows first, then entities sorted by pitch Y so that
entities further up the screen render behind, then the ball composited last when
airborne.

`[COMMUNITY]` Goals and nets are drawn with faked perspective as part of the pitch
graphics, and players are layered in front of or behind the goal frame at the ends.

`[UNKNOWN]` Tie-breaking in the Y sort, which matters for visual determinism when two
players share a Y coordinate.

---

## 14. Things the original got wrong

Recorded here so you can decide deliberately whether to reproduce them. Faithfulness
to the match engine does not oblige you to reproduce management bugs.

`[COMMUNITY]` **The transfer market ignores club stature.** Elite players will sign
for tiny clubs if the money is there. The canonical example is a top international
joining a lower-league English side. Fixing this needs club attractiveness and player
ambition, neither of which the original models.

`[COMMUNITY]` **Goalkeepers have no attributes.** Value is the only differentiator.

`[COMMUNITY]` **Fixed match length in career mode**, with no option to lengthen.

`[COMMUNITY]` **Limited league coverage** outside Europe. Three Asian leagues total,
which is the gap that motivates most community data projects.

These four are your stated project goals restated as bugs. That is a good sign: the
match engine is the thing to preserve and the management layer is the thing to fix,
which is exactly the split the architecture assumes.

---

## 15. The measurement checklist

Everything tagged `[UNKNOWN]` or `[DISPUTED]`, collected. This is the Phase 0 backlog.
Each item becomes a scenario in the trace corpus.

**Timing and integration**
- [ ] Confirm simulation tick rate on the reference build
- [ ] Fixed-point format and pitch coordinate scale in internal units
- [ ] Integration order per tick (input, AI, movement, collision, ball)
- [ ] Rounding and truncation behaviour in fixed-point multiply and divide

**Player movement**
- [ ] Max speed per Speed attribute value (all 8 levels)
- [ ] Acceleration and deceleration ramps
- [ ] Turning behaviour between the 8 directions, including whether turns cost speed
- [ ] Animation frame timing relative to movement

**Ball**
- [ ] Gravity
- [ ] Restitution and rolling friction per pitch type (7 sets)
- [ ] Kick power curve: hold duration in ticks to launch speed
- [ ] Launch elevation as a function of hold duration
- [ ] Capture radius and dribble kick distance
- [ ] How Control modifies dribble kick distance

**Aftertouch**
- [ ] Window length in ticks
- [ ] Effect strength decay against elapsed ticks
- [ ] Lateral acceleration magnitude for curl
- [ ] Vertical effect magnitude for low/high/lob
- [ ] Whether aftertouch differs between passes and shots

**Contests**
- [ ] Slide tackle: reach, duration, recovery time
- [ ] Running tackle resolution: inputs, weights, randomness
- [ ] Header: trigger height range, jump arc, resulting ball velocity
- [ ] Deflection rules on intercepted balls

**AI: instrumentation (do this first)**
- [ ] Locate where the reference writes per-player synthesized controller values
- [ ] Extend the trace format to carry inputs as well as state
- [ ] Confirm two-layer diffing works on a known-good scenario

**AI: off-ball**
- [ ] What "steer toward tactic target" means mechanically
- [ ] Arrival radius, if any
- [ ] Whether any override exists beyond ball pursuit (marking, pressing, offside)

**AI: ball pursuit**
- [ ] How many players chase the ball
- [ ] Chase target: current ball position or predicted intercept point
- [ ] Selection metric: distance, or time-to-ball accounting for Speed
- [ ] Hysteresis or damping in chaser selection (there must be some)
- [ ] Whether the goalkeeper is in the pool, and when he leaves his line
- [ ] Human-team auto-selection rule and switch conditions

**AI: shooting**
- [ ] Human fire-hold threshold in ticks for shot versus pass (community guess: ~4)
- [ ] Whether the CPU bypasses the hold requirement
- [ ] Shoot predicate inputs: goal distance, facing cone, pressure, area, ball height
- [ ] Whether the predicate is evaluated per tick (rebound fingerprint)
- [ ] Whether CPU shot power is chosen, fixed, or distance-derived

**AI: passing**
- [ ] Whether the CPU uses the same nearest-in-cone rule as the human
- [ ] If not: cone half-angle, distance weighting, lane blocking, tie-breaks
- [ ] Goalkeeper distribution logic

**RNG**
- [ ] Whether the simulation contains any randomness at all
- [ ] Which generator, exact algorithm and word size
- [ ] How it is seeded, and whether it is per-match or persistent
- [ ] Verify: same state replayed twice produces bit-identical traces

**Attributes**
- [ ] Numeric modifier per attribute per point, for all seven attributes
- [ ] Whether modifiers are linear
- [ ] What the redundant high bit in the skill nibble means

**Contested**
- [ ] Green tick semantics: career-only value growth, or in-match performance effect

**Camera**
- [ ] Dead zone dimensions
- [ ] Follow lag or lead
- [ ] Edge clamping behaviour

---

## 16. Sources

Primary and secondary sources used to compile this. Where two sources conflicted, the
conflict is recorded above rather than resolved.

- Original SWOS manual, hosted at `worldofstuart.excellentcontent.com/swos/` and
  `abandonwaredos.com`. Authoritative for controls, aftertouch, substitutions and the
  tactics grid dimensions.
- `bigcalm.tripod.com/swos/tactics-analysis.htm`, `.tac` file structure and the
  nibble encoding of target positions.
- SWOS United / sensiblesoccer.de forums, particularly threads on player attributes,
  pitch types and tactics. The pitch types PDF guide is the best single source on
  surface behaviour.
- `github.com/zlatkok/swos-port` asset scripts, for sprite layering and asset
  structure.
- Community player databases (`swos.gazchap.com`, `database.swos.info`) for the shape
  of the player data model.
- Community FAQs on GameFAQs and Neoseeker, for control specifics and scoring
  technique.
- SWOS United threads on CPU shooting behaviour and on improving CPU difficulty, for
  the rebound-goal observation, the fire-hold threshold estimate, and the finding
  that CPU tactics are stored in the executable.

**On using any of these:** they are documentation. Read them, then write your own
code and your own importers. Disassembly-derived source and extracted game artwork
are derivative works of a copyrighted binary regardless of their age.

---

## 17. The one thing to remember

`[COMMUNITY]` Jon Hare, who designed the original, spent over a decade building a
spiritual successor and the community's verdict was that it did not capture the feel.
The designer could not reproduce it from memory.

That is not an argument against this project. It is an argument that memory,
intuition and "playing it until it feels right" are insufficient methods, and that
the trace harness in `PLAN.md` section 9 is not optional infrastructure.

Measure everything. Tune nothing by feel until the numbers already agree.