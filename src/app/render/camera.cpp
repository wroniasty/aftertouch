#include "render/camera.hpp"

#include "core/referee.hpp"

#include <algorithm>

namespace at::render {

namespace {

// Octant → unit vector, 0 = N clockwise. The same table as SETPIECES.md §2 and
// PLAYER_SPRITES.md §5; the camera uses it to lead toward where a restart taker faces.
constexpr int8_t kDirVec[8][2] = {{0, -1}, {1, -1}, {1, 0},  {1, 1},
                                  {0, 1},  {-1, 1}, {-1, 0}, {-1, -1}};

int Sign(int v) { return v > 0 ? 1 : (v < 0 ? -1 : 0); }

const Entity* ControlledOf(const MatchState& s, int side) {
    if (side < 0 || side > 1) return nullptr;
    const int8_t slot = s.sides[static_cast<size_t>(side)].control.controlled_slot;
    if (slot < 0 || slot >= kPitchPlayers) return nullptr;
    return &s.players[static_cast<size_t>(slot)];
}

// While play is stopped the ball has no delta, so the lead comes from the taker's
// facing, falling back to the octant the restart wrote (CAMERA.md §5).
void StoppedLeadDirection(const MatchState& s, int& dx, int& dy) {
    uint8_t dir = s.globals.camera_direction;
    const int last = static_cast<int>(s.globals.last_team_played_before_break) - 1;
    if (const Entity* e = ControlledOf(s, last)) {
        if (e->direction >= 0 && e->direction < 8) dir = static_cast<uint8_t>(e->direction);
    }
    if (dir > 7) dir = 0;
    dx = kDirVec[dir][0];
    dy = kDirVec[dir][1];
}

bool IsCornerOrThrowIn(GameState gs) {
    switch (gs) {
    case GameState::CornerLeft:
    case GameState::CornerRight:
    case GameState::ThrowInForwardRight:
    case GameState::ThrowInCentreRight:
    case GameState::ThrowInBackRight:
    case GameState::ThrowInForwardLeft:
    case GameState::ThrowInCentreLeft:
    case GameState::ThrowInBackLeft:
        return true;
    default:
        return false;
    }
}

} // namespace

int16_t ClipCameraDestination(int32_t v, int16_t lo, int16_t hi) {
    if (hi < lo) hi = lo;
    if (v < lo) v = lo;
    if (v > hi) v = hi;
    return static_cast<int16_t>(v);
}

int16_t EaseTowards(int16_t from, int16_t to) {
    // One sixteenth of the remaining distance, then a hard 5-unit ceiling — in that
    // order. Clamping the distance first and easing after produces a visibly laggier
    // camera (CAMERA.md §11).
    int32_t delta = static_cast<int32_t>(to) - static_cast<int32_t>(from);
    delta = delta >= 0 ? (delta >> kEaseShift) : -((-delta) >> kEaseShift);
    if (delta == 0 && to != from) delta = to > from ? 1 : -1;
    if (delta > kMaxStep) delta = kMaxStep;
    if (delta < -kMaxStep) delta = -kMaxStep;
    return static_cast<int16_t>(from + delta);
}

int16_t RampLead(int16_t lead, int direction) {
    if (direction < 0 && lead > -kLeadMax) lead = static_cast<int16_t>(lead - kLeadStep);
    else if (direction > 0 && lead < kLeadMax) lead = static_cast<int16_t>(lead + kLeadStep);
    if (lead > kLeadMax) lead = kLeadMax;
    if (lead < -kLeadMax) lead = -kLeadMax;
    return lead;
}

void Camera::Reset(uint16_t roll) {
    // The one RNG draw the camera owns. It comes from presentation_rng, which HashState
    // excludes, so the coin flip cannot desynchronise a replay the way the original's
    // shared stream could (CAMERA.md §8).
    x_ = kKickoffX;
    y_ = (roll & 1) ? kKickoffBotY : kKickoffTopY;
    lead_x_ = 0;
    lead_y_ = 0;
    mode_ = CameraMode::FollowBall;
}

CameraParams Camera::Resolve(const MatchState& state) const {
    CameraParams p;
    const GameState gs = GetGameState(state);
    const GameStatePl pl = GetPl(state);

    // Priority list, first match wins (CAMERA.md §2).
    if (state.globals.show_fans_counter > 0) {
        p.frozen = true;
        return p;
    }

    const auto ref = static_cast<RefereeState>(state.globals.ref_state);
    const bool handing_card =
        ref == RefereeState::AboutToGiveCard || ref == RefereeState::Booking;
    if (handing_card && state.globals.booked_player >= 0 &&
        state.globals.booked_player < kPitchPlayers) {
        const Entity& b = state.players[static_cast<size_t>(state.globals.booked_player)];
        p.dest_x = b.pos.x.Whole();
        p.dest_y = b.pos.y.Whole();
        p.side_limit = kSideLimitBreak;
        return p;
    }

    if (gs == GameState::Penalties) {
        p.dest_x = kPenaltyCamX + kLogicalW / 2;   // fixed stare at the upper goal
        p.dest_y = kPenaltyCamY;
        p.side_limit = kSideLimitBreak;
        return p;
    }

    if (state.globals.wait_for_player_to_go_in_timer > 0 ||
        state.globals.substitute_in_progress != 0) {
        p.dest_x = kSideLimitSub;
        p.dest_y = kCentreSpotY;
        p.side_limit = kSideLimitSub;
        return p;
    }

    // Standard mode: side margin first, then the gameState sub-switch.
    p.side_limit = kSideLimitInPlay;
    if (pl != GameStatePl::InProgress && IsCornerOrThrowIn(gs))
        p.side_limit = kSideLimitBreak;

    switch (gs) {
    // "Watch them leave the pitch". NOT kStartingGame, despite CAMERA.md §3 listing it
    // here: the reference uses that state for players walking out, but our clock keeps
    // it as the state of open play after a centre kickoff (match_clock.hpp — it sets
    // InProgress and InPlay and leaves game_state alone), and set_pieces.hpp returns to
    // it after every goal. Following the reference literally parks the camera on the
    // right touchline for most of a match. PlayersToInitialPositions is our equivalent
    // of the walking-out state.
    case GameState::PlayersToInitialPositions:
    case GameState::CameraGoingToShowers:
    case GameState::GoingToHalfTime:
    case GameState::PlayersGoingToShower:
        p.dest_x = kWalkOffX;
        p.dest_y = kCentreSpotY;
        return p;
    case GameState::ResultAfterGame:
    case GameState::ResultOnHalfTime:
        p.dest_x = kCentreSpotX;
        p.dest_y = kResultY;
        return p;
    case GameState::GameEnded:
        p.dest_x = kCentreSpotX;
        p.dest_y = kCentreSpotY;
        return p;
    default:
        break;
    }

    // followTheBall — target plus the accumulated lead.
    p.dest_x = state.ball.pos.x.Whole();
    p.dest_y = state.ball.pos.y.Whole();

    int dx = 0, dy = 0;
    if (pl == GameStatePl::InProgress) {
        dx = Sign(state.ball.delta.x.Raw());
        dy = Sign(state.ball.delta.y.Raw());
    } else {
        StoppedLeadDirection(state, dx, dy);
    }
    p.lead_x = RampLead(lead_x_, dx);
    p.lead_y = RampLead(lead_y_, dy);
    return p;
}

void Camera::Apply(const CameraParams& p) {
    if (p.frozen) return;

    // Store the lead back — every mode that is not followTheBall returns zero, which is
    // what resets the accumulator when play stops.
    lead_x_ = p.lead_x;
    lead_y_ = p.lead_y;

    const int32_t want_x =
        static_cast<int32_t>(p.dest_x) - kLogicalW / 2 + p.lead_x;
    const int32_t want_y =
        static_cast<int32_t>(p.dest_y) - kLogicalH / 2 + p.lead_y;

    // First clip: the destination, against the mode's side margin.
    const int16_t dest_x = ClipCameraDestination(
        want_x, p.side_limit, static_cast<int16_t>(kCameraMaxX - p.side_limit));
    const int16_t dest_y = ClipCameraDestination(want_y, kDestMinY, kDestMaxY);

    x_ = EaseTowards(x_, dest_x);
    y_ = EaseTowards(y_, dest_y);

    // Second clip: the position, against the hard pitch limits. Different bounds on
    // purpose — the camera may sit where its destination could not.
    x_ = ClipCameraDestination(x_, kCameraMinX, kCameraMaxX);
    y_ = ClipCameraDestination(y_, kCameraMinY, kCameraMaxY);
}

void Camera::Update(const MatchState& state) {
    const CameraParams p = Resolve(state);
    // `: mode_` latched — once frozen for any reason the camera reported Frozen
    // for the rest of the match, even while it was demonstrably following the
    // ball again. Position was never affected (Apply tests p.frozen, not mode_),
    // so this was only ever a lie told to observers — but it is exactly the kind
    // of lie that makes a test look like it is covering something.
    //
    // Only these two modes are actually resolved today; the five-mode priority
    // C2 describes is reported through CameraParams, not through mode_.
    mode_ = p.frozen ? CameraMode::Frozen : CameraMode::FollowBall;
    Apply(p);
}

void Camera::SnapToDestination(const MatchState& state) {
    const CameraParams p = Resolve(state);
    if (p.frozen) return;
    lead_x_ = p.lead_x;
    lead_y_ = p.lead_y;
    x_ = ClipCameraDestination(static_cast<int32_t>(p.dest_x) - kLogicalW / 2 + p.lead_x,
                               kCameraMinX, kCameraMaxX);
    y_ = ClipCameraDestination(static_cast<int32_t>(p.dest_y) - kLogicalH / 2 + p.lead_y,
                               kCameraMinY, kCameraMaxY);
}

DebugView Camera::View() const {
    DebugView v{};
    v.min_x = x_;
    v.min_y = y_;
    v.max_x = static_cast<int16_t>(x_ + kLogicalW);
    v.max_y = static_cast<int16_t>(y_ + kLogicalH);
    return v;
}

} // namespace at::render
