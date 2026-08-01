# A6 — Test infrastructure

The harness every later part is verified with: golden-trace regression, shared
fixtures, a headless season runner, offscreen capture helpers, a null UI backend,
fuzz entry points for the binary decoders, and CI on Windows and macOS. It owns
no gameplay and no presentation — only the instruments that make "done when"
checkable in a commit rather than a demo.

Depends on: A3   Blocks: every wave gate (§6 rule 5)   Wave: 1

---

## 0. One-paragraph version

[PLAN.md](PLAN.md) §7 and standing rule 5 demand that a part is not done until a
test says so on both platforms. A6 is that rule made concrete: `tracekit` turns an
input scenario into a byte-identical trace and diffs two traces to a first-divergence
tick; a committed golden for the `kickoff` scenario fails the moment anyone changes
what `MatchEngine::Step` writes; `NullUiBackend` lets D-layer flows run with no
window; a tiny season runner and an RGBA capture helper are the E and C hooks; fuzz
entry points hammer A4/A5 validators; GitHub Actions runs `ctest` on Windows and
macOS. Core tests still link nothing but `at_core` and doctest — CI enforces that.

---

## 1. Scope

**In:**

- `src/tools/tracekit/` — generate + diff (+ file I/O for tools/tests). Links `at_core` only.
- `src/tools/tracegen/`, `src/tools/tracediff/` — thin CLIs over tracekit.
- Committed golden `tests/golden/kickoff.attr` and the regen path.
- `NullUiBackend` in `src/app/ui/`.
- Headless season runner and offscreen surface-hash helpers under `tests/harness/`.
- Fuzz-style mutation tests for `ATAP` / `ATGD` validators.
- `.github/workflows/ci.yml` — both platforms, submodules, `ctest`.
- CMake guard: `core_tests` must not link SDL.

**Out:**

| Excluded | Owner |
|---|---|
| Trace record layout | A3 (`core/trace.hpp`) |
| Reference corpus / viewer | A3 items still open |
| Real kit/pitch golden images | C1 / C3 |
| SQLite career persistence | D2 |
| Full B11 result backends | B11 |

---

## 2. Design

### 2.1 Golden traces vs reference diffs

[A3](A3-trace-harness.md) already separates the questions. A6 wires the first one into
every build:

| Question | Mechanism |
|---|---|
| Did *we* change? | Regenerate from the scenario, `Diff` against `tests/golden/*.attr` |
| Are we *faithful*? | Reference corpus via A3 (when present) |

Fixtures are generated (`tracegen`), not hand-hexed. Regenerating after an intentional
engine change is `tracegen … > tests/golden/kickoff.attr` and a deliberate commit.

### 2.2 The acceptance wedge

A harness that has never failed is worthless. A6 therefore ships two tests together:

1. Regenerated `kickoff` matches the committed golden.
2. The same scenario with a one-raw-unit post-step mutation of `ball.pos.x` **does not**.

That is the "deliberately introduced physics change fails a golden trace" criterion
without leaving a buggy binary in the tree.

### 2.3 Null UI

`NullUiBackend` implements `IUiBackend`, ignores window/renderer/events, and returns
intents from a scripted queue. D1's screen flows become ordinary doctest cases.

### 2.4 Season runner and offscreen

Both are thin so B11/C1 can grow into them:

- **Season runner** — round-robin pairs from an A5 `League`, `ApplyKickoff`, step N
  ticks, collect scores. No graphics.
- **Offscreen** — an RGBA buffer + FNV hash. Capture-from-SDL lands with C1; the hash
  contract is fixed here so C tests have somewhere to land.

### 2.5 Fuzz entry points

Deterministic mutation suites over valid `ATAP`/`ATGD` bytes (truncate, flip, grow).
Optional `LLVMFuzzerTestOneInput` stubs behind `AT_FUZZ` for local libFuzzer runs;
CI runs the deterministic suite only.

### 2.6 CI

Two jobs, Windows and macOS. Recursive submodules, configure via presets, build,
`ctest --output-on-failure`. A red job blocks the wave — not a notification.

---

## 3. Interfaces

| Path | Role |
|---|---|
| `at_tracekit` | `Generate`, `Diff`, file helpers |
| `tracegen` / `tracediff` | CLI for humans and regen |
| `NullUiBackend` | Scripted `IUiBackend` |
| `tests/harness/*` | Season runner, offscreen hash |
| `.github/workflows/ci.yml` | Both platforms |

Wall: tracekit and core_tests link no SDL. Null UI lives beside `IUiBackend`, not under
`ui_imgui/`.

---

## 4. Work items

1. **tracekit + CLIs** — generate, diff, file I/O. → unit tests on synthetic traces.
2. **Golden `kickoff`** — commit + regen docs. → `test_golden_trace.cpp`.
3. **Physics-failure wedge** — mutate post-step, assert golden fails.
4. **NullUiBackend** + scripted intent test.
5. **Season runner + offscreen helpers** + smoke tests.
6. **Fuzz mutation suites** for A4/A5 validators.
7. **CI workflow** + CMake SDL-link guard on `core_tests`.

---

## 5. Tests and acceptance

| Test | Pins |
|---|---|
| `test_golden_trace.cpp` | Kickoff golden matches; one-raw-unit physics mutate diverges. |
| `test_tracekit.cpp` | Self-diff identical; header mismatch / truncate are errors. |
| `test_null_ui.cpp` | Scripted intents drain in order; no SDL. |
| `test_season_runner.cpp` | Fictional league plays every pair for N ticks. |
| `test_offscreen.cpp` | Stable hash for a solid surface. |
| `fuzz_containers.cpp` | Mutated packs/leagues never crash; invalid → reject. |
| CI | `ctest` green on `windows-latest` and `macos-latest`. |

**Done when:** `ctest` runs the whole suite on both platforms in CI, core tests link no
SDL, and a deliberately introduced physics change fails a golden trace.

---

## 6. Open questions

- **libFuzzer in CI** — deferred; deterministic mutation coverage is enough until A6's
  CI time budget is measured.
- **Golden growth** — one kickoff scenario until B2; add named goldens per B part rather
  than one megafile.
