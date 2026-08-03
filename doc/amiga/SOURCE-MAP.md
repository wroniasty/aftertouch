# SOURCE-MAP.md

Where everything is in `original-amiga-swos.asm`. This document is a lookup table,
not an explanation — every entry links onward to the document that explains it.

The file is 334 132 lines and roughly 80 % data. The match engine occupies two
bands: **lines 6 000 – 47 000** (interrupts, main loop, simulation, AI) and
**lines 78 000 – 140 000** (menus, in-match UI, tactics, skills). Everything above
line 140 000 is graphics, sprite tables, animation frames and team data.

---

## 0. Orientation

| Band | Lines | Contents |
|---|---|---|
| Structs and enums | 1 – 305 | `Sprite`, `PlayerGame`, `TeamGeneralInfo`, `sField_18`, `PL_*`, `ST_*` |
| System | 6 100 – 20 600 | Interrupt handlers, keyboard, joystick, blitter, audio |
| Maths and ball | 20 660 – 22 240 | Trig, `SetBallPosition`, `UpdateBall`, ball quadrant |
| Keeper sprites, init | 22 234 – 26 100 | `DoGoalKeeperSprites`, `InitGame` |
| Clock | 26 101 – 26 530 | `UpdateTime` and the clock variables |
| Input | 28 189 – 28 550 | `ReadJoy0`, `ReadJoy1` |
| **Gameplay constants A** | **30 500 – 30 800** | Physics, pitch tables, spin tables |
| Main loop | 30 817 – 31 800 | `main`, `maingame`, `Game_Starting_Wait` |
| RNG | 32 600 – 32 640 | `Rand` |
| **Gameplay constants B** | **34 700 – 34 870** | Player speeds, contest tables, shot bonus tables |
| Kicking and contest | 34 859 – 35 550 | `DoPass`, `PlayerKickingBall`, `CalculateIfPlayerWinsBall`, `UpdatePlayerSpeed` |
| Zonal grid | 35 886 – 36 110 | Quadrant limits and coordinates, `SetPlayerWithNoBallDestination` |
| Set-piece tables | 36 496 – 36 620 | Eight direction-delta tables |
| Stoppage machine | 37 250 – 37 800 | `RunStoppageEventsAndSetAnimationTables` |
| Headers | 38 018 – 38 420 | `PlayerDoingHeader`, `DoStaticHeader` |
| Keeper logic | 38 560 – 39 700 | `ShouldGoalkeeperDive`, `GoalkeeperCaughtTheBall`, `GoalkeeperJumping` |
| Aftertouch | 40 089 – 40 360 | `ApplyBallAfterTouch`, `ResetLeftAndRightSpinTimers` |
| Period transitions | 40 715 – 41 160 | `EndFirstHalf`, `EndOfGame`, `PrepareForInitialKick` |
| Restart classification | 41 156 – 41 620 | `GameSetup` |
| **The per-player pipeline** | **41 900 – 45 300** | `UpdatePlayersAndBall` — one routine, ~3 400 lines |
| CPU brain | 45 296 – 46 900 | `DoAI`, `AISetDirectionAllowed`, `AIHeader` |
| Skills | 102 057 – 102 200 | `AdjustPlayerSkills` |
| Animation tables | 59 488 – 66 300 | Sprite frame index tables |
| Sprite instances | 71 726 – 77 000 | `ballSprite`, `ballShadowSprite`, keeper and team sprite tables |

---

## 1. Routines, alphabetically

| Routine | Line | Documented in |
|---|---|---|
| `AdjustPlayerSkills` | 102057 | [PLAYERS.md](PLAYERS.md) §3 |
| `AIHeader` | 46576 | [AI.md](AI.md) §4 |
| `AISetDirectionAllowed` | 46524 | [AI.md](AI.md) §4 |
| `ApplyBallAfterTouch` | 40089 | [AFTERTOUCH.md](AFTERTOUCH.md) §2 |
| `CalculateBallNextXYPositions` | 39041 | [BALL.md](BALL.md) §7 |
| `CalculateDeltaXAndY` | 20661 | [MOVEMENT.md](MOVEMENT.md) §1 |
| `CalculateIfPlayerWinsBall` | 35144 | [CONTEST.md](CONTEST.md) §3 |
| `CalculateNextBallPosition` | 35665 | [BALL.md](BALL.md) §7 |
| `DoAI` | 45296 | [AI.md](AI.md) §3 |
| `DoGoalKeeperSprites` | 22234 | [GOALKEEPER.md](GOALKEEPER.md) §1 |
| `DoPass` | 34859 | [PASSING.md](PASSING.md) §1–§4 |
| `DoStaticHeader` | 38285 | [KICKING.md](KICKING.md) §5 |
| `EndFirstHalf` | 40756 | [TIMING.md](TIMING.md) §3 |
| `EndOfGame` | 40806 | [TIMING.md](TIMING.md) §3 |
| `FindClosestPlayerToBallFacing` | 46754 | [AI.md](AI.md) §4 |
| `GameSetup` | 41156 | [SETPIECES.md](SETPIECES.md) §1 |
| `GetBallDestCoordinatesTable` | 39793 | [SETPIECES.md](SETPIECES.md) §3 |
| `GetClosestNonControlledPlayerInDirection` | 39859 | [PASSING.md](PASSING.md) §1 |
| `GetFramesKeeperNeedsToReachBall` | 38672 | [GOALKEEPER.md](GOALKEEPER.md) §3 |
| `GetPlayerPointerFromShirtNumber` | 35640 | [STATE.md](STATE.md) §4 |
| `GoalkeeperCaughtTheBall` | 39290 | [GOALKEEPER.md](GOALKEEPER.md) §5 |
| `GoalkeeperClaimedTheBall` | 40627 | [GOALKEEPER.md](GOALKEEPER.md) §5 |
| `GoalkeeperJumping` | 39480 | [GOALKEEPER.md](GOALKEEPER.md) §5 |
| `InitGame` | 23679 | [TIMING.md](TIMING.md) §1, [BALL.md](BALL.md) §5 |
| `Joystick_Wait` | 37794 | [TIMING.md](TIMING.md) §1 |
| `maingame` | 30970 | [TIMING.md](TIMING.md) §1 |
| `PlayerDoingHeader` | 38018 | [KICKING.md](KICKING.md) §5 |
| `PlayerHeading` | 39688 | [KICKING.md](KICKING.md) §5 |
| `PlayerKickingBall` | 35033 | [KICKING.md](KICKING.md) §2 |
| `PrepareForInitialKick` | 40989 | [SETPIECES.md](SETPIECES.md) §2 |
| `Rand` | 32600 | [AI.md](AI.md) §5 |
| `ResetLeftAndRightSpinTimers` | 40348 | [AFTERTOUCH.md](AFTERTOUCH.md) §2 |
| `RunStoppageEventsAndSetAnimationTables` | 37250 | [TIMING.md](TIMING.md) §4 |
| `SetAnimationTable` | 21104 | — (presentation) |
| `SetBallPosition` | 21138 | [SETPIECES.md](SETPIECES.md) §2 |
| `SetPlayerWithNoBallDestination` | 35973 | [AI.md](AI.md) §2 |
| `SetPlayersHalfPlayed` | 26481 | [TIMING.md](TIMING.md) §3 |
| `ShouldGoalkeeperDive` | 38560 | [GOALKEEPER.md](GOALKEEPER.md) §3 |
| `UpdateBall` | 21593 | [BALL.md](BALL.md) §2–6 |
| `UpdateBallVariables` | 38711 | [CONTEST.md](CONTEST.md) §1 |
| `UpdateBallWithControllingGoalkeeper` | 39965 | [GOALKEEPER.md](GOALKEEPER.md) §5 |
| `UpdatePlayerSpeed` | 35391 | [MOVEMENT.md](MOVEMENT.md) §3 |
| `UpdatePlayersAndBall` | ~41900 | everywhere; see §3 below |
| `UpdateTeamOfs24Table` | 35548 | [AI.md](AI.md) §2 |
| `UpdateTime` | 26101 | [TIMING.md](TIMING.md) §2 |

Unnamed helpers worth knowing:

| Label | Line | What it does |
|---|---|---|
| `sub_109A54` | 22228 | Mirror ball `destX` about current `x` (post rebound) |
| `sub_109A66` | 22240 | Mirror ball `destY` about current `y` (net / barrier rebound) |
| `sub_110C04` | 34920* | Deflected-tackle ball redirect |
| `sub_110CD8` | 35000* | Set post-tackle recovery timer from Tackling |
| `sub_113122` | 39106 | Tackle collision, foul test, card decision |
| `sub_111388` | 36096 | Card / booking escalation |
| `sub_11381A` | 39905* | Snap the ball to a controlling player's feet |
| **`sub_111B98`** | **36982** | **Player selection.** Recomputes every `ballDistance`, then assigns `controlledPlayerSprite` to the nearest eligible player. Called from `Joystick_Wait`. [PASSING.md](PASSING.md) §6 |
| `sub_118290` | 44631* | CPU receiver's first-time pass under pressure. [PASSING.md](PASSING.md) §7 |

\* approximate — these sit inside larger listings; grep the label to land exactly.

---

## 2. Data tables

Two dense blocks hold nearly every tuning value. Both are worth reading top to
bottom once.

### Block A — asm:30500 – 30800

| Symbol | Line | Value(s) |
|---|---|---|
| `keeperPenaltySaveDistanceFar` | 30556 | 20 |
| `keeperPenaltySaveDistanceNear` | 30557 | 12 |
| `substitutedPlSpeed` | 30574 | 1536 |
| `goalkeeperSpeedWhenGameStopped` | 30578 | 1024 |
| `ballGroundConstant` | 30582 | 16 |
| `ballAirConstant` | 30583 | 10 |
| `playerGroundConstant` | 30584 | 96 |
| `ballKickingDeltaZ_2` (header rise) | 30587 | $A000 |
| `ballSpeedBounceFactorTable` | 30594 | 24, 80, 80, 72, 64, 40, 32 |
| `ballBounceFactorTable` | 30601 | 88, 112, 104, 104, 96, 88, 80 |
| `pitchBallSpeedInfluence` | 30608 | −2, 2, 3, 0, 0, −1, −1 |
| `gravityConstant` | 30615 | 4608 |
| `goalkeeperGameSpeed` | 30705 | $400 |
| `playerTacklingSpeed` | 30706 | $700 |
| `jumpHeaderSpeed` | 30707 | 2048 |
| `ballKickingDeltaZ` | 30729 | $14000 |
| `ballKickingSpeed` | 30730 | 2208 |
| `spinMultiplierFactor` | 30735 | 5,4,3,2,2,2,2,1,1,1 |
| `kickSpinFactor` | 30746 | 8 octants × 2 sides × (dx,dy) |
| `passingSpinFactor` | 30778 | same shape, roughly half magnitude |

### Block B — asm:34700 – 34870

| Symbol | Line | Value(s) |
|---|---|---|
| `playerSpeedsGameInProgress` | 34726 | 928, 974, 1020, 1066, 1112, 1158, 1204, 1250 |
| `playerSpeedsGameStopped` | 34734 | 1136, 1152, 1168, 1184, 1200, 1216, 1232, 1248 |
| `unk_1106B2` (tackle recovery) | 34747 | 30, 27, 24, 21, 18, 15, 12, 9 |
| `unk_1106C2` (deflected recovery) | 34763 | 3 × 8 |
| `plAvgTacklingBallControlDiffChance` | 34774 | 16, 17, 18, 19, 20, 21, 22, 23 |
| `ballSpeedDeltaWhenControlled` | 34783 | 130, 116, 102, 88, 74, 60, 46, 32 |
| `unk_1106EA` (dribble touch interval) | 34791 | 4, 5, 6, 8, 11, 14, 17, 21 |
| `unk_110670` (pass power bonus) | 34701 | 0, 48, 96, 144, 192, 256, 320, 384 |
| Pass base speeds | 34680 | $600, $680, $700, $755, $7AA, $800, $855, $8AA |
| `dseg_17E286` (bad-pass chance) | 34807 | 6, 4, 3, 2, 1, 0, 0, 0 |
| `goalScoredChances` | 34815 | 1…15, 0 (16 entries) |
| `keeperSaveDistance` | 34835 | 24 |
| `ballSpeedKicking` | 34836 | −384, −270, −162, −54, 54, 162, 270, 384 |
| `ballSpeedFinishing` | 34844 | −288, −160, −32, 96, 224, 352, 480, 608 |
| `playerStrongHeaderSpeedIncrease` | 34852 | −336, −288, −240, −192, −144, −96, −48, 0 |

### Elsewhere

| Symbol | Line | What |
|---|---|---|
| `kSineCosineTable` | 20761 | 256-entry Q15 sine, full circle |
| `kAngleCoeficients` | 20774 | 32 × 32 arctangent lookup |
| `unk_10B826` (`timeDelta` options) | 26526 | 30, 18, 12, 9 |
| `injuriesSpeedPenalty` | 35536 | 0, −96, −128, −160, −192, −224, −256, −288 |
| `ballXQuadrantLimits` | 35896 | 183, 285, 387, 489 |
| `ballYQuadrantLimits` | 35901 | 220, 312, 403, 495, 586, 678 |
| `playerXQuadrantsCoordinates` | 35907 | 15 columns, 98 → 574 step 34 |
| `playerYQuadrantCoordinates` | 35937 | 16 rows, 149 → 749 step 40 |
| `defaultPlayerDestinations` | 36496 | 8 octants × (dx, dy) of ±1000 |
| `unk_11169A` | 36368 | 32 directions × (dx, dy) of ±1000 — receiver intercept offsets |
| `leftThrowInBallDestDelta` … | 36505–36616 | seven restart-specific overrides |
| `unk_113864` | 39905 | 8 octants × (dx, dy) of ±1, ball-at-feet offset |

---

## 3. Navigating `UpdatePlayersAndBall`

The per-player pipeline is one routine of about 3 400 lines with no internal
function boundaries, dispatching on `Sprite.playerState`. IDA's local labels are
descriptive and are the practical index into it. In rough order of execution:

| Label | Line | Phase |
|---|---|---|
| `update_goalkeeper_saved_timer` | 41995 | Per-team timers decay |
| `update_pass_kick_timer` | 42007 | " |
| `update_player_switch_timer` | 42021 | " — triggers `ApplyBallAfterTouch` |
| `players_loop` | 42087 | Start of the 11-player sweep |
| `very_close_to_ball` … `ball_too_high` | 42131 – 42179 | Proximity and height banding ([CONTEST.md](CONTEST.md) §1) |
| `check_player_state` | 42182 | Dispatch on `PL_*` |
| `player_goalkeeper` | 42218 | Keeper branch |
| `goal_attempt` | 42489 | Shot resolution begins |
| `shot_at_goal` | 42569 | Save-or-goal decision |
| `goal_scored` | 42613 | Goal accepted |
| `its_controlled_player` | 43318 | Human-controlled player input |
| `find_acceptable_turn_flags_loop` | 43359 | Turn restriction |
| `firing` | 43539 | Fire button held → kick or pass |
| `its_a_header` | 43723 | Header dispatch |
| `player_throwing_in` | 43786 | Throw-in |
| `player_tackling` | 44014 | `PL_TACKLING` |
| `player_normal2` | 44152 | `PL_NORMAL2` (post-header settle) |
| `player_heading` | 44195 | `PL_HEADING` |
| `player_injured` | 44320 | `PL_INJURED` |
| `player_tackled` | 44331 | `PL_TACKLED` / `PL_DOWN` |
| `player_expecting_pass` | 44377 | Pass receiver — the whole receiver state machine ([PASSING.md](PASSING.md) §7–§9) |
| `not_controlled_player` | 44714 | AI-driven teammate |
| `set_player_with_no_ball_destination` | 45095 | Zonal positioning call |
| `update_player_speed` | 45137 | Speed resolution |
| `set_player_coordinates` | 45195 | Integrate position |
| `next_player` | 45234 | Loop |

---

## 4. Conventions in the listing

- **Duplicate labels.** IDA reuses short local names (`out:`, `game_in_progress:`,
  `next_player:`) inside different routines. Always disambiguate by line number,
  never by name alone.
- **Cross-reference comments.** Every label carries `; CODE XREF:` or
  `; DATA XREF:` comments naming its callers and readers. These are the fastest way
  to find who consumes a table — grep the table name and read the xref.
- **Offsets in xrefs.** `UpdateBall+1AC` means byte offset $1AC into the routine,
  not a line number. Useful for confirming you are looking at the right site when a
  table has several readers.
- **`dc.b` pairs.** Word values in data blocks are frequently emitted as two `dc.b`
  lines. `dc.b 0 / dc.b $1E` is the word 30. Read them in pairs, big-endian.
