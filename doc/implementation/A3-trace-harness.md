# A3 — Trace harness

The measuring instrument. A per-tick trace record format, a pure serialiser in
`src/core/`, an instrumented build of the reference that emits the same records and the
inputs that produced them, a differ that reports the first divergence tick, a viewer
that shows it, and the input corpus everything from B1 to B10 is judged against. It
excludes the golden-trace regression and CI that consume it (A6), the engine physics it
measures (B1–B10), and the sprites that make a divergence visible (A4).

Depends on: A2   Blocks: A6, and the acceptance criterion of every B part   Wave: 1

---

## 0. One-paragraph version

[../PLAN.md](../PLAN.md) §9 is blunt about why this comes first: it is the difference
between a number and an opinion. The work is mostly plumbing, with three decisions that
are not. **The oracle is a `SWOS_TEST` build in Amiga mode** — `SWOS_TEST` because the
port's default kernel is knowingly unfaithful
([A2](A2-determinism-primitives.md) §2.4), Amiga because 50 Hz gives tick-for-tick
correspondence (§2.4a); neither is optional and both are one-line switches. **The
reference's own replay format is not the trace format** — `HIL2` records what was
*drawn*, not what was *simulated*, and records no input at all, so it cannot drive our
engine; it is still worth having as a free viewer smoke-test and an independent
positional cross-check. **The divergence tolerance is zero.** [../PLAN.md](../PLAN.md)
§8 lists "where the tolerance line sits" as undecided and expects a band; with
fixed-point arithmetic and a pinned kernel there is no legitimate source of ±1 drift,
so a band would hide bugs rather than absorb noise. The number A3 reports is the tick
at which exact equality first breaks, and a drift profile describing how bad it gets
afterwards.

---

## 1. Scope

**In:**

- The trace record: layout, header, versioning, endianness, and the padding-free
  guarantee it inherits from [A2](A2-determinism-primitives.md) §2.6.
- `SerializeTrace` / `DeserializeTrace` in `src/core/` — pure, no I/O, so Rule 1 holds.
- The reference-side instrumentation, as a **patch committed to this repo** and applied
  to a local reference checkout. Emits our record format plus a separate input log.
- `src/tools/tracegen/` — runs our engine headless from an input log and writes a trace.
- `src/tools/tracediff/` — the differ and its CLI, usable with no window.
- `src/tools/trace_viewer/` — two traces overlaid, scrubbing, divergence reporting.
- The input corpus, its storage policy, and how it is regenerated.
- The numerical divergence definition (§2.6), which [../PLAN.md](../PLAN.md) §8 requires
  before Wave 2 starts.

**Out:**

| Excluded | Owner |
|---|---|
| Golden-trace regression, CI on both platforms, the headless season runner | A6 |
| `MatchState`'s contents — A3 fixes the *encoding*, not the fields | B1 |
| Any engine behaviour the traces measure | B1–B10 |
| Sprites that make a divergence visible rather than merely counted | A4 |
| Career/season traces | E layer, and they are a different instrument |

A3 owns the instrument and the corpus. It owns no gameplay, and a change to A3 must
never change a tick of simulation.

---

## 2. Design

### 2.1 The oracle build

Two switches, both required, neither default:

| Switch | Why | Source |
|---|---|---|
| `SWOS_TEST` | The port's default kernel computes `sin * speed >> 8`; the original computes `(sin >> 8) * speed`. The default is a better number and a worse oracle. | [A2](A2-determinism-primitives.md) §2.4 |
| Amiga mode | 50 Hz, so reference tick *N* is our tick *N* and the diff is an equality test rather than a resampling argument. | [A2](A2-determinism-primitives.md) §2.4a |

`SWOS_TEST` pays a second dividend: the port's test configuration mocks SDL through
`SDL_DYNAMIC_API` (`docs/tests.md`), so an instrumented build has no vsync and no
wall-clock in its loop. A recorder that waits on a display is not a recorder.

**The instrumentation lives in this repo as a patch, not as a vendored copy.**
`tools/reference/` holds `instrument.patch` and an `apply.py` that applies it to a
reference checkout given by path, refusing to run if the checkout is dirty or at an
unexpected commit. That keeps the instrumentation versioned and reviewable without
putting a line of the reference in our history — the same rule
[../PLAN.md](../PLAN.md) §9 states for code and §10 states for assets.

### 2.2 `HIL2` is not the trace format

The reference already records whole matches. `docs/highlights.txt` documents `HIL2`: a
3510-byte header (both `TeamGame` structures, competition name, score, pitch type) then
per-frame records of camera x/y, score, a BCD clock, and an array of
`(picture index, x, y)` with **full 32-bit coordinates including the fractional part** —
the format notes this is new, the original stored whole coordinates only.

It is tempting and it is insufficient, for one structural reason and one fatal one:

- **Structural:** it records the *drawn* scene, not the simulation. There is no
  velocity, no `speed`, no `playerState`, no RNG state; `z` is baked into screen y by
  the draw path, and positions have already been through the sprite centre-point offset
  ([../RENDERING.md](../RENDERING.md) §3). Every one of those is a field a physics diff
  needs.
- **Fatal:** it records no input. A replay is a record of outputs, and our engine cannot
  be driven to reproduce outputs — it needs the inputs that produced them.

So `HIL2` earns two real jobs and not the main one: a **viewer smoke-test** before any
instrumentation exists (the viewer can load and scrub a real match on day one), and an
**independent positional cross-check** on our own recorder, since a bug in our
instrumentation that quietly corrupts positions would otherwise be invisible. Both are
worth having; neither is the format.

### 2.3 The record

Fixed-width binary, one record per tick, little-endian, explicitly padded. Fixed width
is what makes `tick → file offset` arithmetic rather than a scan, and it is what makes
the differ a loop over `memcmp`.

```
Header (once)
    magic "ATTR", format version, engine build id
    platform profile (Amiga / PC), seed, tick rate
    scenario name, corpus entry id
    record stride, record count

Record (per tick)
    tick, phase
    MatchInput for both teams          <- what was pressed
    RNG stream states                  <- all three, all bytes (A2 section 2.5)
    ball:    pos xyz, vel xyz, speed, direction, fullDirection, dest xy, flags
    players: the same, plus playerState, animation cursor, flags   x22
    payload hash
```

**The record is built field by field, not by copying the object representation.** That
costs a few lines and buys three things: the format does not depend on compiler padding,
it does not depend on host endianness, and it survives B1 reshaping `MatchState`. A
`memcpy` would be shorter and would silently encode whatever the compiler left in the
gaps.

**The hash covers the serialised payload, not the state object.** This is a deliberate
departure from what [A2](A2-determinism-primitives.md) §2.6 provides. Hashing the object
requires `MatchState` to be padding-free — a guarantee A2 work item 7 has yet to deliver
— whereas hashing the payload is padding-independent by construction and can be verified
by a reader that never reconstructs the state at all. It also means A3 is not blocked on
A2 item 7, which it otherwise would be.

**The input is in the record, not only in a sidecar.** A trace that does not carry its
own inputs cannot be replayed by anyone who has only the trace, and the first time that
matters is the first bug report. The separate input log exists so a scenario can be
*driven*; the in-record copy exists so a trace is self-describing, and the two
disagreeing is itself a detectable error.

**The hash is in the record** even though it is derived. A whole-record `memcmp`
answers "identical"; the hash answers it in one word, which is what lets the viewer scan
a hundred thousand ticks to find the first divergence without decoding every field. The
differ verifies the hash matches the payload on load, so a stale hash is caught rather
than trusted.

**Serialisation is pure and lives in core.** `SerializeTrace(const MatchState&,
std::span<uint8_t>)` and its inverse do no I/O; the harness supplies the bytes and does
the writing. That is what keeps Rule 1 intact while still putting the format next to the
state it encodes. It is also, not coincidentally, B1's "done when": full match state
serialises to a trace record and back losslessly.

### 2.4 Alignment: which team moves first

[../MOVEMENT.md](../MOVEMENT.md) §1.1 — `UpdateAndApplyTeamControls` handles **one team
per frame**, alternating on `++m_teamSwitchCounter & 1`. So a trace comparison is only
meaningful if both sides agree on the phase of that alternation at tick 0. If our engine
starts on the bottom team and the reference on the top, every decision is one tick out
of step and the diff reports divergence at tick 1 for a reason that has nothing to do
with physics.

The counter's initial value is therefore part of the trace header and part of the
scenario definition, and the instrumentation records it rather than assuming it. This is
the cheapest possible bug to prevent and an expensive one to diagnose.

### 2.5 The tools

Three binaries under `src/tools/`, deliberately separate:

| Tool | Links | Why separate |
|---|---|---|
| `tracegen` | `at_core` only | Runs our engine headless from an input log. No SDL, so it runs in CI and on a machine with no graphics stack. |
| `tracediff` | `at_core` only | The differ and its report. Usable from a script, a test, and A6's regression job. The GUI must not be the only way to get the number. |
| `trace_viewer` | `at_core`, SDL, ImGui | Two traces overlaid, scrub, per-field breakdown, jump-to-divergence. |

The split matters: **the number must be obtainable without a window.** A harness whose
only interface is a GUI cannot be run by A6, cannot fail a build, and quietly becomes a
thing people look at rather than a thing that gates work.

`src/tools/` is outside Wall 1 by design ([../PLAN.md](../PLAN.md) §3 draws the line at
`src/core/`), so the viewer may use SDL and ImGui freely. A3 adds one wall rule
regardless: `tracegen` and `tracediff` must link no SDL, checked the same way
`core_tests` is.

### 2.6 What divergence means — the number

[../PLAN.md](../PLAN.md) §8 lists this as undecided and warns that every B part will
argue about it separately if it is not settled first. Settled:

**Tolerance is zero. Every field must match exactly.** The reported number is
`first_divergence_tick`: the lowest tick at which any field of any entity differs.

The reasoning is not strictness for its own sake. The engine is fixed-point, the kernel
is pinned bit-for-bit, the tables are exact ([A2](A2-determinism-primitives.md) §2.3)
and the tick rate matches. **There is no mechanism by which a correct implementation
drifts by one raw unit.** A tolerance band would therefore never absorb noise — there is
no noise — it would only postpone the tick at which a real bug is reported, which is the
one thing this instrument exists to prevent. The classic failure mode of trace-diffing
projects is a tolerance that grows every time it is inconvenient; a tolerance of zero
cannot grow without someone noticing.

Two things soften that without weakening it:

- **A drift profile, not a pass/fail.** After the first divergence the differ keeps
  going and reports per-field L1 distance over time. "Diverges at 340, drifts to 3 units
  by 600" and "diverges at 340, explodes by 350" are different bugs and the tick alone
  does not distinguish them.
- **Field classes in the report.** A divergence in `playerState` is a different kind of
  problem from one in the low bits of `x`, and the report says which class broke first
  even though both count.

B parts state their acceptance as a tick count against a named corpus entry — B4's "no
divergence in the first 200 ticks" is exactly this number — and no B part gets to define
its own tolerance.

### 2.7 The corpus, and how it is stored

Scenarios, each an input log plus the reference trace it produced:

| Entry | Exercises | First needed by |
|---|---|---|
| `kickoff` | The spine, alternation phase, restart state | B2 |
| `run` | Eight-way movement, boundaries | B4 |
| `pass_short` | Tap fire, receiver selection | B5, B6 |
| `shot_curl` | Aftertouch — the part the project is named after | B6 |
| `dribble_turn` | Possession, the ball's own aim point | B5 |
| `tackle_slide` | Contests, fouls | B7 |
| `header_jump` | Height windows, the driven/lob switch | B7 |
| `throw_in`, `goal_kick` | Restarts | B8 |
| `keeper_claim` | The claim rule and the landing predictor | B3, B9 |

**Storage policy, because the arithmetic forces a decision.** A record is roughly 40
bytes per entity, so 23 entities is about 1 KB per tick and a 500-tick scenario is
around 500 KB. Committing ten of those is five megabytes of binary that changes whenever
the format does.

So: **input logs are committed** (they are hundreds of bytes and they are the part that
cannot be regenerated), together with the header and a hash chain over the reference
trace. The reference traces themselves are **regenerated on demand** from a local
reference checkout by `tools/reference/record_corpus.py`, and the committed hash chain
proves the regenerated trace is the one the corpus was defined against. Exactly two
scenarios — `kickoff` and `shot_curl` — additionally commit their full reference trace,
so that a clone with no reference checkout can still run a meaningful diff and so the
format has a permanent worked example. This follows
[../PLAN.md](../PLAN.md) §7: fixtures are generated, not hand-written, and a committed
40 KB trace is fine while a hand-maintained one is not.

---

## 3. Interfaces

Added to `src/core/include/core/`: `trace.hpp` — the record layout, `SerializeTrace`,
`DeserializeTrace`, and the format version constant. Nothing else in core changes.

New under `src/tools/`: `tracegen/`, `tracediff/`, `trace_viewer/`.
New under `tools/reference/`: `instrument.patch`, `apply.py`, `record_corpus.py`.
New under `tests/corpus/`: input logs, hash chains, two full traces.

What other parts see:

- **B1–B10** see one thing: a tick number from `tracediff`, against a named corpus
  entry. They do not read the format and they do not get to reinterpret it.
- **A6** consumes `tracegen` and `tracediff` as the regression mechanism, and adds the
  golden traces and the CI wiring.
- **A4** is the other half of the same instrument: A3 says a divergence started at tick
  340, A4 is what lets you see it. The dependency runs the other way round from how it
  looks — neither blocks the other, and both are wanted before Wave 2.

The wall: `trace.hpp` is core, so it does no I/O and no allocation. `tracegen` and
`tracediff` link no SDL.

---

## 4. Work items

1. **The record format and the core serialiser.** `trace.hpp`, `SerializeTrace`,
   round-trip. Blocked on nothing; `MatchState` is still the A1 stub, and encoding a
   stub is the right first move because it forces the format to be versioned before
   there is anything to lose. → `test_trace_format.cpp`.
2. **`tracediff` and the divergence definition.** The differ, the drift profile, the
   field classes, the CLI. Testable entirely on synthetic traces, so it lands before any
   real data exists. → `test_tracediff.cpp`.
3. **`tracegen`.** Headless run of our engine from an input log. → an end-to-end test
   that generates a trace and diffs it against itself.
4. **The reference patch.** `SWOS_TEST` + Amiga build, alternation-counter capture,
   input logging, per-tick emission in our format. The largest unknown in this part; see
   §6.1.
5. **The corpus.** `record_corpus.py`, the scenario definitions, the hash chains, the two
   committed traces.
6. **`trace_viewer`.** Overlay, scrub, jump-to-divergence, field breakdown. Last
   deliberately: the number is useful without it, and it is the piece most likely to
   absorb unbounded time.
7. **`HIL2` reader.** Optional, and worth it: a viewer smoke-test and an independent
   positional cross-check (§2.2).

Items 1–3 are unblocked today and are the ones B1 needs. Item 4 is where the schedule
risk sits.

---

## 5. Tests and acceptance

[../PLAN.md](../PLAN.md) §7 puts A3 in service of the B layer's technique — golden
traces and reference diffs — so A3's own tests are about the instrument being
trustworthy, which is a different question from the engine being right.

| Test | What it pins |
|---|---|
| `test_trace_format.cpp` | Round-trip of a fully-populated state; stride matches the header; a version mismatch is rejected rather than misread; the hash in the record matches the payload; endianness is explicit and asserted, not inherited. |
| `test_tracediff.cpp` | Self-diff of any trace reports no divergence; a trace perturbed at tick *k* in field *f* reports exactly *k* and names *f*'s class; a one-raw-unit perturbation **is** a divergence (the zero-tolerance rule, asserted rather than described); truncated and length-mismatched traces are errors, not silent short diffs. |
| `test_tracegen.cpp` | The same input log produces byte-identical traces twice in one process and across two runs — the [A2](A2-determinism-primitives.md) gate, re-asserted at the level the corpus actually uses. |
| `test_corpus.py` | Every committed input log parses; every hash chain matches its regenerated trace when a reference checkout is present, and is skipped with a clear message when it is not. |
| `test_hil2.cpp` | The `HIL2` reader against a recorded match; frame count and scene offsets agree with the header. |

**Invariants added to the always-on set:** a trace record's hash equals the hash of the
state it was serialised from; `DeserializeTrace(SerializeTrace(s)) == s` every tick under
a test build — which is the [../PLAN.md](../PLAN.md) §7 engine invariant "state
serialises and round-trips losslessly every tick", and A3 is what makes it checkable.

**Golden data:** the two committed reference traces, plus synthetic traces generated in
the tests themselves. Regenerated by `tools/reference/record_corpus.py`.

**The demonstration that closes the part**: a reference trace and an engine trace of the
recorded `kickoff` scenario load side by side in the viewer, and it reports a first
divergence tick. Note what that does *not* require — it does not require the tick to be
large. At the end of Wave 1 the engine is still a stub and the honest expected result is
divergence at tick 1. The instrument working is the deliverable; the number getting
bigger is Waves 2 and 3.

---

## 6. Open questions

**6.1 — How much of the reference has to be instrumented, and how invasive is it?**
The single largest unknown here, and the one item that could overrun. Emitting our record
means reaching `Sprite` and both `TeamGeneralInfo` blocks each tick, which
[../STATE.md](../STATE.md) maps completely, so the *what* is known. The *how* is not: the
port addresses globals through a flat VM memory image (`g_memByte[523118]`), and whether
a clean hook point exists in the game loop or whether the patch ends up scattered is
unknown until tried. **Resolved by:** doing item 4 early enough that a bad answer does not
surprise Wave 2 — it is scheduled fourth, not last, for that reason. If it turns out
invasive, the fallback is to emit at the same point `HIL2` recording already hooks, which
is known to exist and known to see every frame.

**6.2 — Does the reference's input path admit a deterministic replay?**
We need the reference driven by a recorded input sequence, not by a human. Whether its
control layer can be fed synthetically without fighting `filterOverlappedEvents`
([../MOVEMENT.md](../MOVEMENT.md) §3.1) is unverified. **Resolved by:** item 4. If it
cannot, the corpus becomes "record a human playing, capturing input and state together"
— which still works, and merely removes the ability to re-run a scenario after changing
the reference.

**6.3 — Is the presentation RNG stream in the record?**
Inherited from [A2](A2-determinism-primitives.md) §6.2, which deferred it here on the
grounds that A3 decides what a record contains. The record above includes all three
streams. If the reference draws for commentary at moments we do not reproduce, that
choice turns cosmetic differences into reported divergences. **Resolved by:** the first
real reference trace — the question is empirical and one recording answers it. Until
then the stream is recorded but excluded from the hash, which is the reversible choice.

**6.4 — What the seed is, and where it comes from.**
[A2](A2-determinism-primitives.md) §6.3 deferred this to the record format. It is in the
header. What remains is whether the reference's seed at kickoff is reachable and
settable, or merely observable. **Resolved by:** item 4.

Unmeasured constants encountered here go to [../LEGACY.md](../LEGACY.md) §15. None were:
A3 measures rather than tunes.
