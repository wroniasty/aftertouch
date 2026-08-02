# B12 — Performance rating

A 1–10 rating per player per match, derived from match events and per-player
volume counters. Pure computation over what the engine already emits; **changes
no gameplay**.

Depends on: B2   Blocks: B11, E2   Wave: 3

Sources: [SIMULATION.md](../SIMULATION.md) §7; PLAN.md B12.

---

## 0. One-paragraph version

Append-only chronicle + per-player `PlayerMatchStats` feed `ComputePlayerRating`.
One primary score 1–10 plus a `RatingBreakdown` for E2. Position-band weight
tables (GK / DEF / MID / ATT) score the same inputs differently. Same function
rates engine matches and table-synthesised sheets. Removing the compute calls
must not change any tick (`HashState` after compute == before).

---

## 1. Locked shape

| Field | Role |
|---|---|
| `rating` (1–10) | Career / HUD primary |
| `RatingBreakdown` | goals, cards, injury, minutes proxy, passes, tackles, headers, carry, saves, fouls_conceded |

Volume stats live on `MatchSide::match_stats[16]` (not the 32-slot chronicle).

---

## 2. Inputs

### Chronicle

Goal, Yellow, Red, Injury (corners exist but do not move the rating).

### `PlayerMatchStats` (wired in Step emit sites)

| Counter | Site |
|---|---|
| `passes_attempted` | pass kick path (`shooting.hpp`) |
| `passes_completed` | capture while `pass_in_progress`, credit kicker (`possession.hpp`) |
| `tackles` | `BeginSlide` (`tackling.hpp`) |
| `headers` | jump / static header begin (`heading.hpp`) |
| `carry_distance` | carrier planar step after dribble (`movement.hpp`), cap 60000 |
| `saves` | GK catch / claim (`goalkeeper.hpp`) |
| `fouls_conceded` | foul path, offending tackler (`set_pieces.hpp`) |

Team `TeamStats::fouls_conceded` remains the team tally (bumped once in tackle foul path).

### Position

`SquadPlayer.position` (`data::Position`: GK=0 … A=7), filled by `ApplyKickoff`.

---

## 3. Position bands + weights

| Band | Positions |
|---|---|
| GK | GK |
| Def | RB, LB, D |
| Mid | RW, LW, M |
| Att | A |

`BandWeights` (goals, pass_comp, pass_att, tackles, headers, carry, saves, fouls, clean_sheet):

| Band | g | pc | pa | t | h | c | sv | foul | cs |
|---|---|---|---|---|---|---|---|---|---|
| GK | 0 | 1 | 0 | 0 | 0 | 0 | 2 | −1 | 2 |
| Def | 1 | 1 | 0 | 2 | 2 | 1 | 0 | −2 | 0 |
| Mid | 2 | 2 | 1 | 1 | 1 | 1 | 0 | −1 | 0 |
| Att | 3 | 1 | 0 | 0 | 1 | 2 | 0 | −1 | 0 |

Yellow / red / injury penalties are global (−1 / −3 / −1), not band-weighted.

---

## 4. Formula

Integer only, then clamp:

```text
base = played ? 5 : 4
raw  = goals*W.g + completed*W.pc/3 + attempted*W.pa/8
     + tackles*W.t + headers*W.h + carry*W.c/200
     + saves*W.saves + fouls*W.fouls
     + (GK && clean sheet) * W.cs
raw  = clamp(raw, -4, +5)
rating = clamp(base + raw - yellows - 3*reds - injured, 1, 10)
```

Typical matches stay near 5–7.

---

## 5. B11 table / scripted synth

`BuildSyntheticStateForRatings` invents plausible `PlayerMatchStats` from band +
RNG (mids more passes, defenders more tackles/fouls, GK more saves when little
conceded, attackers more carry). Table Resolve then overlays real squad
positions from the league and re-rolls volume stats.

---

## 6. Done when

- Unit pins: same stats → different ratings by band; MID moves more on passes;
  GK on saves; DEF hurt more by fouls than ATT
- `HashState` unchanged when rating runs after the match
- XI rated in fixed-seed CPU / synthesised matches
- Golden / corpus re-pinned after `match_stats` landed in ATTR
