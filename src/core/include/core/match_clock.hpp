#pragma once
#include "core/match_state.hpp"
#include "core/profile.hpp"

#include <array>
#include <cstdint>

// Fixed-step match clock — doc/implementation/B2-frame-state-machine.md
// and doc/SIMULATION.md §3. No lastFrameTicks; one Step = one tick.

namespace at {

inline constexpr std::array<int16_t, 4> kGameLenSecondsTable = {30, 18, 12, 9};

inline constexpr int16_t ClockRefill() {
    return IsAmigaProfile() ? int16_t{49} : int16_t{70};
}

inline constexpr int16_t InjuryCounterStart() {
    return IsAmigaProfile() ? int16_t{50} : int16_t{55};
}

// Ticks of InProgress clock time for 90 displayed minutes at the given length.
inline constexpr uint32_t TicksForNinetyMinutes(uint8_t game_length) {
    const int16_t delta = kGameLenSecondsTable[game_length & 3u];
    // 5400 game-seconds * refill / delta
    return static_cast<uint32_t>(5400 * static_cast<int32_t>(ClockRefill()) /
                                 static_cast<int32_t>(delta));
}

inline bool ProlongLastMinute(const MatchState& s) {
    const int16_t by = s.ball.pos.y.Whole();
    if (by <= kPenaltyBoxTopY || by > kPenaltyBoxBotY) return true;
    const uint8_t attacking = (by > kCentreSpotY) ? uint8_t{1} : uint8_t{2};
    return s.clock.last_team_played == attacking;
}

inline GameStatePl GetPl(const MatchState& s) {
    return static_cast<GameStatePl>(s.globals.game_state_pl);
}

inline GameState GetGameState(const MatchState& s) {
    return static_cast<GameState>(s.globals.game_state);
}

inline void SetPl(MatchState& s, GameStatePl pl) {
    s.globals.game_state_pl = static_cast<uint8_t>(pl);
}

inline void SetGameState(MatchState& s, GameState gs) {
    s.globals.game_state = static_cast<uint8_t>(gs);
}

inline void SyncPhaseFromClock(MatchState& s) {
    if (GetGameState(s) == GameState::GameEnded ||
        GetGameState(s) == GameState::ResultAfterGame) {
        s.phase = MatchPhase::FullTime;
        return;
    }
    if (GetGameState(s) == GameState::GoingToHalfTime ||
        GetGameState(s) == GameState::FirstHalfEnded ||
        GetGameState(s) == GameState::ResultOnHalfTime) {
        s.phase = MatchPhase::HalfTime;
        return;
    }
    if (GetPl(s) == GameStatePl::InProgress) {
        s.phase = MatchPhase::InPlay;
        return;
    }
    if (s.clock.match_started == 0 ||
        GetGameState(s) == GameState::StartingGame ||
        GetGameState(s) == GameState::PlayersToInitialPositions) {
        s.phase = MatchPhase::KickOff;
    }
}

// Swap ends: teamPlayingUp / teamStarting become 3 − x (SIMULATION §3).
inline void SwapEnds(MatchState& s) {
    if (s.globals.team_playing_up == 1 || s.globals.team_playing_up == 2)
        s.globals.team_playing_up =
            static_cast<uint8_t>(3 - s.globals.team_playing_up);
    if (s.globals.team_starting == 1 || s.globals.team_starting == 2)
        s.globals.team_starting =
            static_cast<uint8_t>(3 - s.globals.team_starting);
}

inline void PlaceBallAtCentre(MatchState& s) {
    s.ball.pos.x = Fix::FromInt(kCentreSpotX);
    s.ball.pos.y = Fix::FromInt(kCentreSpotY);
    s.ball.pos.z = Fix{};
    s.ball.delta = {};
    s.ball.dest_x = kCentreSpotX;
    s.ball.dest_y = kCentreSpotY;
    s.ball.speed = 0;
    s.globals.foul_x = kCentreSpotX;
    s.globals.foul_y = kCentreSpotY;
}

// First-Step match bootstrap: roll kick-off sides, centre spot, brief stoppage.
inline void BeginMatchIfNeeded(MatchState& s) {
    if (s.clock.match_started) return;
    s.clock.match_started = 1;
    s.clock.game_length   = s.clock.game_length; // keep caller override
    // Roll 1 or 2 for each.
    s.globals.team_playing_up =
        static_cast<uint8_t>((s.gameplay_rng.Draw() & 1u) + 1u);
    s.globals.team_starting =
        static_cast<uint8_t>((s.gameplay_rng.Draw() & 1u) + 1u);
    PlaceBallAtCentre(s);
    // B3: Normal surface until weather/pitch selection exists.
    s.surface = MatchSurface{}; // 0 / 64 / 96
    SetGameState(s, GameState::StartingGame);
    SetPl(s, GameStatePl::Stopped);
    s.clock.stoppage_event_timer = 2;
    s.phase = MatchPhase::KickOff;
}

inline void StartSecondHalf(MatchState& s) {
    SetGameState(s, GameState::StartingGame);
    SetPl(s, GameStatePl::InProgress);
    s.clock.period = 1;
    s.clock.end_game_counter = 0;
    s.clock.game_seconds = 0;
    // displayed_minute stays at 45
    PlaceBallAtCentre(s);
    s.phase = MatchPhase::InPlay;
    MarkBallLoose(s);
}

inline void EndFirstHalf(MatchState& s) {
    s.clock.displayed_minute = 45;
    s.clock.game_seconds = 0;
    s.clock.end_game_counter = 0;
    SetGameState(s, GameState::FirstHalfEnded);
    SetPl(s, GameStatePl::Stopped);
    s.clock.stoppage_event_timer = 100;
    SwapEnds(s);
    s.phase = MatchPhase::HalfTime;
}

inline void EndSecondHalf(MatchState& s) {
    s.clock.displayed_minute = 90;
    s.clock.game_seconds = 0;
    s.clock.end_game_counter = 0;
    SetGameState(s, GameState::GameEnded);
    SetPl(s, GameStatePl::Stopped);
    s.phase = MatchPhase::FullTime;
    // Score left as-is (0–0 unless something else scored).
}

// Advance one InProgress tick of clock. May trigger period end.
inline void UpdateTime(MatchState& s) {
    // Stoppage ceremony (kick-off / half-time).
    if (GetPl(s) != GameStatePl::InProgress) {
        if (s.clock.stoppage_event_timer > 0) {
            --s.clock.stoppage_event_timer;
            if (s.clock.stoppage_event_timer == 0) {
                if (GetGameState(s) == GameState::FirstHalfEnded ||
                    GetGameState(s) == GameState::GoingToHalfTime ||
                    GetGameState(s) == GameState::ResultOnHalfTime) {
                    StartSecondHalf(s);
                } else if (GetGameState(s) == GameState::StartingGame ||
                           GetGameState(s) == GameState::PlayersToInitialPositions) {
                    SetPl(s, GameStatePl::InProgress);
                    SetGameState(s, GameState::StartingGame);
                    s.phase = MatchPhase::InPlay;
                    MarkBallLoose(s);
                }
            }
        }
        SyncPhaseFromClock(s);
        return;
    }

    // Already finished.
    if (GetGameState(s) == GameState::GameEnded) {
        s.phase = MatchPhase::FullTime;
        return;
    }

    const int16_t period_end_minute =
        (s.clock.period == 0) ? int16_t{45} : int16_t{90};

    // Injury-time whistle deferral.
    if (s.clock.end_game_counter > 0) {
        if (ProlongLastMinute(s)) {
            s.clock.end_game_counter = InjuryCounterStart();
        } else {
            --s.clock.end_game_counter;
            if (s.clock.end_game_counter == 0) {
                if (s.clock.period == 0)
                    EndFirstHalf(s);
                else
                    EndSecondHalf(s);
            }
        }
        SyncPhaseFromClock(s);
        return;
    }

    const int16_t delta =
        kGameLenSecondsTable[static_cast<size_t>(s.clock.game_length & 3u)];
    s.clock.seconds_accumulator =
        static_cast<int16_t>(s.clock.seconds_accumulator - delta);
    if (s.clock.seconds_accumulator < 0) {
        s.clock.seconds_accumulator =
            static_cast<int16_t>(s.clock.seconds_accumulator + ClockRefill());
        if (s.clock.game_seconds < 0) {
            // Injury-time entry sentinel; treat as 0 and continue.
            s.clock.game_seconds = 0;
        }
        ++s.clock.game_seconds;
        if (s.clock.game_seconds >= 60) {
            s.clock.game_seconds = 0;
            const int16_t next_minute =
                static_cast<int16_t>(s.clock.displayed_minute + 1);
            if (next_minute >= period_end_minute) {
                // Enter last-minute / injury-time instead of bumping past end.
                s.clock.displayed_minute =
                    static_cast<int16_t>(period_end_minute - 1);
                s.clock.game_seconds = -1;
                s.clock.end_game_counter = InjuryCounterStart();
            } else {
                s.clock.displayed_minute = next_minute;
            }
        }
    }
    SyncPhaseFromClock(s);
}

inline void UpdateStats(MatchState& s) {
    if (GetPl(s) != GameStatePl::InProgress) return;
    const uint8_t team = s.clock.last_team_played;
    if (team == 1 || team == 2) {
        ++s.sides[static_cast<size_t>(team - 1)].stats.possession;
    }
}

} // namespace at
