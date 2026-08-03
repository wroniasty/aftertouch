#pragma once
#include "core/match_state.hpp"

#include <cstdint>
#include <optional>

// Pure out-of-play classification — SIMULATION.md §5.
// Not wired into MatchEngine::Step (B2); B3 calls when the ball can leave play.

namespace at {

struct OutOfPlayResult {
    GameState state = GameState::StartingGame;
    bool      is_goal = false;
};

// last_team: 1 or 2 = who last played; 0 = unknown (touchline defaults to home restart).
// ball_z_whole: integer z (high word); goal requires z <= 15.
// defends_top_team: globals.team_playing_up (1 or 2); 0 = unknown, see below.
inline std::optional<OutOfPlayResult> ClassifyBallOutOfPlay(int16_t x, int16_t y,
                                                            int16_t ball_z_whole,
                                                            uint8_t last_team,
                                                            uint8_t defends_top_team = 0) {
    const bool inside_x = x >= kPlayableMinX && x <= kPlayableMaxX;
    const bool inside_y = y >= kPlayableMinY && y <= kPlayableMaxY;
    if (inside_x && inside_y) return std::nullopt;

    OutOfPlayResult r;

    // Past goal line (y out).
    if (y < kPlayableMinY || y > kPlayableMaxY) {
        const bool goal_mouth =
            ball_z_whole <= 15 && (x - 1) >= 302 && (x - 1) <= 366;
        if (goal_mouth) {
            r.is_goal = true;
            r.state = (y < kPlayableMinY) ? GameState::GoalOutLeft
                                         : GameState::GoalOutRight;
            return r;
        }
        // SIMULATION §5: corner if lastTeamPlayed is the team defending that
        // byline, otherwise a goal kick. defends_top_team is teamPlayingUp, which
        // defends the top (low y) line; the other side defends the bottom.
        const bool left = x < kCentreSpotX;
        const bool at_top = y < kPlayableMinY;
        // 0 = caller did not supply teamPlayingUp; fall back to the legacy guess
        // (equivalent to assuming teamPlayingUp == 2).
        const uint8_t defending =
            (defends_top_team == 1 || defends_top_team == 2)
                ? (at_top ? defends_top_team
                          : static_cast<uint8_t>(3 - defends_top_team))
                : (at_top ? uint8_t{2} : uint8_t{1});
        if (last_team == defending) {
            r.state = left ? GameState::CornerLeft : GameState::CornerRight;
        } else {
            // Goal kick — encoded as GoalOut* situation for the restart side.
            r.state = left ? GameState::GoalOutLeft : GameState::GoalOutRight;
        }
        return r;
    }

    // Touchline (x out, y still in range of pitch length).
    const bool left_touch = x < kPlayableMinX;
    // y thirds: <342 forward, 342..555 centre, >=556 back (SIMULATION §5).
    GameState base;
    if (y < 342) {
        base = left_touch ? GameState::ThrowInForwardLeft
                          : GameState::ThrowInForwardRight;
    } else if (y < 556) {
        base = left_touch ? GameState::ThrowInCentreLeft
                          : GameState::ThrowInCentreRight;
    } else {
        base = left_touch ? GameState::ThrowInBackLeft
                          : GameState::ThrowInBackRight;
    }
    r.state = base;
    return r;
}

} // namespace at
