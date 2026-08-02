# B11 — Result simulation

*View result* for fixtures nobody plays: `IResultSimulator::Resolve(League,
Fixture, Seed) -> MatchResult`. Three backends behind one seam.

Depends on: B9, A5, B12   Blocks: D3   Wave: 3

Sources: [SIMULATION.md](../SIMULATION.md) §10; PLAN.md B11 contract.

---

## 0. One-paragraph version

Scripted, Table, and Engine backends fill one `MatchResult` (score, scorers,
cards, injuries, TeamStats, B12 ratings, fidelity). Resolve uses a **local**
RNG from the seed — never `MatchState::resolve_rng`. Table is calibrated
against Engine under fixed seeds.

---

## 1. Types

- `Fixture`, `MatchResult`, `ResultFidelity` in `core/match_result.hpp`
- `IResultSimulator` + backends in `data/result_simulator.hpp` (needs `League`)

---

## 2. Backends

| Backend | Fidelity | Notes |
|---|---|---|
| Scripted | Synthesised | Fixed score table for tests / D3 |
| Table | Synthesised | Strength index → goal table → scorers → synth stats → B12 |
| Engine | ExactEngine | Headless CPU vs CPU to FullTime |

---

## 3. Table model

Strength = mean outfield (finishing+passing+speed)/3 → index 0..5. Home +1 step.
Goals sampled from 6×8 table with local RNG. Scorers by position weights; ~3%
unattributed. Then synthesise chronicle-equivalent state and call B12.

---

## 4. Done when

Bit-identical table resolve; engine fills ratings; fixed-seed envelope Table vs
Engine within bands; resolve does not perturb a later match HashState.
