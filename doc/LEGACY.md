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

> **A second oracle now exists.** The documents in [amiga/](amiga/) were traced
> through a 68000 disassembly of the **Amiga SWOS 96/97 match module** — the build
> the DOS port was ported *from*. Its gameplay data tables carry descriptive labels
> and literal values, so most of §15's physics and tuning unknowns now have a
> **candidate number**. Where the two oracles agree, a claim is settled; where they
> disagree, that disagreement is itself a finding and is recorded rather than
> quietly resolved. The full ledger of what changed is
> [AMIGA_CHANGES.md](AMIGA_CHANGES.md).
>
> The rule above stands unchanged. A recovered value is a *measurement of the
> original*, not a specification we are obliged to match, and the interpretation
> wrapped around it can still be wrong.

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

`[DATA]` Stored as 4-bit values 0 to 15, but the effective scale is 0 to 7: the
high bit is redundant, so 8 behaves as 0, 9 as 1, up to 15 as 7. Whatever the high
bit originally meant, it is not skill magnitude. Preserve it in your importer rather
than masking it away; it may turn out to be a flag.

> **Confirmed, and with the mechanism.** This was a `[COMMUNITY]` claim; the Amiga
> original settles it. `AdjustPlayerSkills` (asm:102092) loads the packed longword
> and masks it with **`$07777777`** — three bits per nibble — before unpacking seven
> consecutive bytes and clamping each to 7. A stored 8 becomes 0 and a stored 15
> becomes 7 because **the high bit is literally masked off on load**, which is
> exactly what the community observed from the outside. The eighth nibble is masked
> away entirely, confirming there are seven skills and not eight.
>
> Two consequences. First, the range really is **0–7**, and every attribute-indexed
> table in the engine has exactly eight entries — so the "are these tables
> undersized?" worry that [DATA.md](DATA.md) §3 raised, and that
> [HEADING.md](HEADING.md) §6 appeared to confirm, is void
> ([HEADING.md](HEADING.md) §10). Second, the high bit carries no information the
> engine reads, so preserving it in an importer is a precaution rather than a
> requirement. See [amiga/PLAYERS.md](amiga/PLAYERS.md) §1.

> **The stored nibbles are not the final ratings.** Before unpacking, a factor
> derived from the player's transfer value (`× 100 / value`, with a conditional −12
> and a flat override under a competition flag) is computed and applied to every
> nibble through a per-skill transform. Tuning fitted against raw extracted team data
> will therefore carry a systematic bias unless that transform is modelled — even as
> a stub. [amiga/PLAYERS.md](amiga/PLAYERS.md) §3.

`[DATA]` **Goalkeepers have no skill values at all.** Keeper quality is derived
from the player's transfer value. If you want a better keeper model, that is a place
where deviating from the original is clearly an improvement rather than a betrayal.

> **Now with the formula.** `goalieSkill = clamp((value + 3) / 7 + b, 0, 7)` where
> `b` is 1 or 2 from a global bit, plus two competition-context ±1 adjustments — so
> the same keeper can be up to two points better or worse depending on the fixture,
> which on a scale of eight is a swing of 12.5 percentage points of goal probability
> ([AI.md](AI.md) §10). Non-keepers get 0. Value *is* the rating, unambiguously.

`[COMMUNITY]` The three letters displayed beside a player's name are simply his three
highest attributes, computed for display. Not a stored field.

~~`[UNKNOWN]` How each attribute maps to a numeric modifier in the simulation.~~
**Answered for six of the seven**, from the Amiga original. Every one is a linear
eight-entry table:

| Attribute | What it indexes | 0 → 7 |
|---|---|---|
| **Passing** | *no in-match reader found* | — |
| **Velocity** | Long-shot launch speed bonus | 1824 → 2592 (+42 %) |
| **Heading** | Jumping-header speed bonus | −336 → 0 (**handicap only**) |
| **Tackling** | Contest odds (averaged with Control); recovery time | recovery 30 → 9 ticks |
| **Control** | Contest odds; dribble touch size; touches before loss | 4 → 21 touches |
| **Speed** | Running speed | 928 → 1250 (+35 %) |
| **Finishing** | Close-range shot bonus; the goal-vs-save roll | 1920 → 2816; 6 % → 94 % goal odds |

Ranked by in-match consequence: **Finishing** dominates (it appears twice and each
point is worth 6.25 points of goal probability), then **Control** (three consumers,
with an accelerating curve), then **Tackling** (two-sided: the odds *and* the cost of
failure), then **Speed** (a flat 35 % band, the least differentiating), then
**Velocity** (only outside the box), then **Heading** (pure handicap, no upside).

`[UNKNOWN]` **Does anything read Passing?** A sweep of the Amiga match module's
documented routines found no read site — `DoPass` targets by geometry alone. If that
holds, Passing is a **career-only attribute** that affects transfer value and nothing
on the pitch. It is a surprising claim, it is checkable against the DOS port, and it
should be checked before we ship an attribute UI: showing a rating that does nothing
is worse than not showing it. [amiga/PLAYERS.md](amiga/PLAYERS.md) §2.

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

> **The Amiga oracle closed most of this list.** Items struck through below now have
> a **candidate value** recovered from the Amiga original's data segment, where the
> physics and tuning constants are named literals rather than opaque addresses. That
> changes what the trace corpus is *for*: these items become **confirmation
> scenarios** — check our number against the original's — rather than searches. The
> per-subsystem derivations are in [amiga/](amiga/); the full ledger of what moved is
> [AMIGA_CHANGES.md](AMIGA_CHANGES.md).
>
> A value being present does not make the surrounding interpretation right. Each
> chain from "this word is 4608" to "therefore gravity is 0.0703 px/frame²" passes
> through assumptions about fixed-point format and frame rate, and those are what a
> trace should attack.

**Timing and integration**
- [x] ~~Confirm simulation tick rate on the reference build~~ — **50 Hz** on the
      Amiga, derived and corroborated from the four match-length settings
      ([SIMULATION.md](SIMULATION.md) §14). The PC build's rate is still inferred.
- [x] ~~Fixed-point format and pitch coordinate scale in internal units~~ — 16.16
      for position and velocity; `speed` is a separate int16 in units of **~1/512
      px/frame**, so 512 ≈ 1 px/frame ([STATE.md](STATE.md) §11).
- [x] ~~Integration order per tick~~ — clock, input (latched once), ball, players in
      **fixed index order seeing partial results**, presentation
      ([SIMULATION.md](SIMULATION.md) §14).
- [ ] Rounding and truncation behaviour in fixed-point multiply and divide
- [ ] **Whether team decisions really alternate one team per frame.** Now the single
      highest-value open item: it halves each side's decision rate and changes input
      latency ([MOVEMENT.md](MOVEMENT.md) §13).

**Player movement**
- [x] ~~Max speed per Speed attribute value (all 8 levels)~~ — `928, 974, 1020, 1066,
      1112, 1158, 1204, 1250` in play; `1136 … 1248` when stopped. Linear, +46 per
      point, 35 % spread.
- [x] ~~Acceleration and deceleration ramps~~ — **there are none.** Full speed or
      nothing, same tick ([MOVEMENT.md](MOVEMENT.md) §3.3).
- [x] ~~Turning behaviour, including whether turns cost speed~~ — no turn-rate limit
      and no cost; restriction is applied by masking *input*, walking outward from
      the requested octant ([MOVEMENT.md](MOVEMENT.md) §13).
- [x] ~~Animation frame timing relative to movement~~ — `max(1280 − speed, 0)/128 + 6`,
      computed inside the simulation. Confirmed identically by both oracles.

**Ball**
- [x] ~~Gravity~~ — `4608` in 16.16 = 0.0703 px/frame² = **176 px/s²**.
- [x] ~~Restitution and rolling friction per pitch type (7 sets)~~ — all three tables,
      all seven surfaces, confirmed element-for-element by both oracles
      ([BALL.md](BALL.md) §12).
- [x] ~~Kick power curve: hold duration to launch speed~~ — **there is no curve for
      kicks**: a flat `2208` for everyone, with skill entering only as a bonus. There
      *is* one for passes: a ramp `$600 … $8AA` by hold duration
      ([SHOOTING.md](SHOOTING.md) §9).
- [x] ~~Launch elevation as a function of hold duration~~ — flat `$14000`; elevation
      is changed only by aftertouch at tick 4.
- [x] ~~Capture radius and dribble kick distance~~ — the aim point is placed ±1000
      units ahead; capture is gated by five exact height bands (4/8/12/17) with a
      hard ceiling at z = 17 ([CONTROL.md](CONTROL.md) §8).
- [x] ~~How Control modifies dribble kick distance~~ — `130, 116, … 32` by Control,
      **inverted** (low Control pushes the ball further), fired on 2 frames in 4.
- [ ] The three **planar** proximity thresholds. Single-sourced from the DOS port
      (`≤ 32 / 72 / 2450` squared); the Amiga could not isolate them.
- [ ] Which pitch index is which named surface in the Amiga binary. The tables match
      the DOS port's exactly, so the naming almost certainly carries over, but the
      index arrives from outside the match module.

**Aftertouch**  
*(engine-side status: structure fixed in [B6a](implementation/B6a-kick-fidelity.md);
the values below now have candidates from the Amiga and should be re-tagged from
`[PROVISIONAL]` to `[CANDIDATE]` as they are confirmed against traces.)*
- [x] ~~Window length in ticks~~ — **10**, counted 0–9, `−1` inactive.
- [x] ~~Effect strength decay against elapsed ticks~~ — `5, 4, 3, 2, 2, 2, 2, 1, 1, 1`,
      summing to 23, with **more than half the curl in the first three ticks**.
- [x] ~~Lateral acceleration magnitude for curl~~ — a 32-entry table of aim-point
      offsets, magnitudes 0 / 23 / 32, halved for passes
      ([AFTERTOUCH.md](AFTERTOUCH.md) §11).
- [x] ~~Vertical effect magnitude for low/high/lob~~ — lob `$20000` at speed 2688,
      drive `$16000` at 2560, against a launch default of `$14000` / 2208. Both
      branches *raise* the ball; nothing makes it fly lower than it left the foot.
- [x] ~~Whether aftertouch differs between passes and shots~~ — yes, three ways: half
      -strength curl, indexed on the ball's *current* heading rather than the launch
      direction, and **no loft at all** for passes, only a one-shot `+1/8` on speed.
- [ ] **Which way the side-latch subtraction runs.** The two oracles transcribe it
      with the operands reversed, which swaps the curl direction in every non-trivial
      case ([AFTERTOUCH.md](AFTERTOUCH.md) §11). One line, total effect on feel.

**Contests**
- [x] ~~Slide tackle: reach, duration, recovery time~~ — launch `1792`, friction 96
      per tick (≈ 17 units, 19 ticks), recovery `30 … 9` by Tackling, or a flat 3 on
      the deflecting path.
- [x] ~~Running tackle resolution: inputs, weights, randomness~~ — one
      `Rand() & 31` against `16 … 23` of 32, indexed by the difference of the two
      players' *(Tackling + Control)/2*. Exactly 50/50 when level; 71.9 % at maximum
      advantage. There is no separate standing-tackle path.
- [x] ~~Header: trigger height range, jump arc, resulting ball velocity~~ — jump
      launch 2048 for **50 frames**, ball at `player.speed × 5/4`, rise `$A000` —
      exactly half a kick's. Heading contributes a **handicap only**, `−336 … 0`.
- [ ] Deflection rules on intercepted balls — still open for a *non-tackling*
      player. The tackle case is documented ([TACKLING.md](TACKLING.md) §12).
- [ ] **Which way round the foul-from-behind test goes.** The two oracles read it as
      exact complements ([TACKLING.md](TACKLING.md) §12). This inverts the refereeing
      of every challenge and is the highest-value contest item.
- [ ] What marks a slide as a *deflecting* tackle rather than a possession attempt.

**AI: instrumentation (do this first)**
- [ ] Locate where the reference writes per-player synthesized controller values
- [ ] Extend the trace format to carry inputs as well as state
- [ ] Confirm two-layer diffing works on a known-good scenario

**AI: off-ball**
- [x] ~~What "steer toward tactic target" means mechanically~~ — a pure table lookup:
      the ball's **predicted landing point** selects one of 35 zones, a 35-byte row
      per player yields one nibble-packed cell on a 15 × 16 lattice, plus a centred
      sub-zone nudge. No steering, no collision avoidance ([AI.md](AI.md) §10).
- [x] ~~Arrival radius, if any~~ — none; the destination is reached and snapped
      per-axis ([MOVEMENT.md](MOVEMENT.md) §1.3).
- [x] ~~Whether any override exists beyond ball pursuit (marking, pressing, offside)~~
      — **none, and there is no offside rule at all**, confirmed by independent
      sweeps of both binaries.

**AI: ball pursuit**
- [ ] How many players chase the ball
- [ ] Chase target: current ball position or predicted intercept point
- [ ] Selection metric: distance, or time-to-ball accounting for Speed
- [ ] Hysteresis or damping in chaser selection (there must be some)
- [ ] Whether the goalkeeper is in the pool, and when he leaves his line
- [ ] Human-team auto-selection rule and switch conditions

**AI: shooting**
- [ ] Human fire-hold threshold in ticks for shot versus pass (community guess: ~4).
      Still open on **both** oracles — one of the few gameplay constants neither
      binary gives up easily.
- [ ] Whether the CPU bypasses the hold requirement
- [ ] Shoot predicate inputs: goal distance, facing cone, pressure, area, ball height
- [ ] Whether the predicate is evaluated per tick (rebound fingerprint)
- [ ] Whether CPU shot power is chosen, fixed, or distance-derived

**AI: passing**
- [ ] Whether the CPU uses the same nearest-in-cone rule as the human
- [ ] If not: cone half-angle, distance weighting, lane blocking, tie-breaks
- [ ] Goalkeeper distribution logic

**RNG**
- [x] ~~Whether the simulation contains any randomness at all~~ — yes, but far less
      than expected. **The two most consequential rolls in the game do not use it**:
      the goal-versus-save resolution and the goalmouth rebound scatter both read the
      frame counter instead ([AI.md](AI.md) §10, [BALL.md](BALL.md) §12).
- [x] ~~Which generator, exact algorithm and word size~~ — a 256-byte table walked by
      an 8-bit position counter, XORed with a key refreshed on wrap. Returns a byte;
      callers mask it (`& 31`, `& $18`, `& 3`, `& 1`).
- [x] ~~How it is seeded~~ — **it is not seeded at all.** Three bytes of state, period
      65 536 draws, fully deterministic from cold. Which means the *call order* is
      part of the simulation state: one speculative draw for a cosmetic effect
      desynchronises every gameplay roll after it.
- [ ] Verify: same state replayed twice produces bit-identical traces
- [ ] Whether the second stream (`Rand2`) is original or a port addition. §10's
      isolation of career result generation from the match stream depends on it
      ([SIMULATION.md](SIMULATION.md) §14).
- [ ] Extract the 256-byte table verbatim (asm:32590).

**Attributes**
- [x] ~~Numeric modifier per attribute per point, for all seven attributes~~ — §9.
- [x] ~~Whether modifiers are linear~~ — all of them are, with even steps.
- [x] ~~What the redundant high bit in the skill nibble means~~ — **nothing.** It is
      masked off on load by `$07777777`, which is precisely why 8 reads as 0 (§9).
- [ ] Whether **Passing** has any in-match reader (§9).
- [ ] The value→skill transform applied during unpack, which stands between a team
      file's numbers and the engine's (§9).

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
- **A 68000 disassembly of the Amiga SWOS 96/97 match module**, held in the sibling
  repository `original-amiga-swos`. IDA output with a substantial hand-annotation
  layer, including named labels on the gameplay data tables. This is the source
  behind the [amiga/](amiga/) documents and behind most of the values now filled in
  above. Control flow is read directly and is reliable; symbol *names* are a human's
  interpretation and are wrong in at least two places that matter
  ([amiga/STATE.md](amiga/STATE.md) §5). Cited throughout as `asm:NNNNN`, meaning a
  line number in that file. Like the DOS port, it is used **as an oracle, never as a
  source of code**.
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