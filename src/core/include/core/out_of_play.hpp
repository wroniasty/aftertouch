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
inline std::optional<OutOfPlayResult> ClassifyBallOutOfPlay(int16_t x, int16_t y,
                                                            int16_t ball_z_whole,
                                                            uint8_t last_team) {
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
        // Corner if last team is defending that end; else goal kick.
        // Top of pitch (low y): defending team is team playing "down" toward top.
        // Simplified: last_team == 1 → corner when ball goes out at top (their attack
        // end if playing up); use left/right by x < centre.
        const bool left = x < kCentreSpotX;
        // Goal kick vs corner: if last_team is the defending team for that byline,
        // corner to the attackers. Without teamPlayingUp context, treat:
        // out at low y with last_team==2 → corner for 1; else goal kick coded as
        // GoalOut* for B8 to reinterpret — use Corner when last_team is the side
        // that would concede a goal kick from that line incorrectly.
        // SIMULATION: corner if lastTeamPlayed is the defending team.
        // Defending top (y < min): team whose goal is at top. With team_playing_up
        // passed separately — for pure helper, use last_team==1 as "home attacks up".
        // Corner when the defending team last touched.
        const bool at_top = y < kPlayableMinY;
        // If last_team is defending that end → corner for opponent.
        // Home (1) defends bottom when playing up; away (2) defends top when home plays up.
        // Without team_playing_up, approximate: at_top → defending is 2, at_bot → 1.
        const uint8_t defending = at_top ? uint8_t{2} : uint8_t{1};
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
