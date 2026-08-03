#include "render/match_renderer.hpp"
#include "render/asset_source.hpp"
#include "render/camera.hpp"
#include "render/kit_palette.hpp"
#include "render/pitch_atlas.hpp"
#include "render/pitch_view.hpp"

#include "core/match_clock.hpp"
#include "core/match_state.hpp"
#include "core/aftertouch.hpp"
#include "core/set_pieces.hpp"
#include "core/shooting.hpp"
#include "tracekit/tracekit.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

namespace at::render {

namespace {

// Match possession.hpp kDistVeryCloseSq / kDistCloseSq — capture footprint in
// whole pitch units. Drawn radius ≈ that circle on screen.
constexpr int32_t kCaptureRadiusSq = 32; // ~5.7 u
constexpr int32_t kCloseRadiusSq   = 72; // ~8.5 u
constexpr int16_t kCentreCircleR   = 55;

void FillCircle(SDL_Renderer* r, int cx, int cy, int radius, Uint8 R, Uint8 G,
                Uint8 B) {
    SDL_SetRenderDrawColor(r, R, G, B, 255);
    if (radius <= 0) {
        SDL_RenderPoint(r, static_cast<float>(cx), static_cast<float>(cy));
        return;
    }
    const int r2 = radius * radius;
    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            if (dx * dx + dy * dy <= r2)
                SDL_RenderPoint(r, static_cast<float>(cx + dx),
                                static_cast<float>(cy + dy));
        }
    }
}

void StrokeCircle(SDL_Renderer* r, int cx, int cy, int radius, Uint8 R, Uint8 G,
                  Uint8 B) {
    if (radius <= 0) {
        FillCircle(r, cx, cy, 0, R, G, B);
        return;
    }
    SDL_SetRenderDrawColor(r, R, G, B, 255);
    // Midpoint-ish: plot 8-way points every ~1 px along the circumference.
    const int steps = radius * 6;
    for (int i = 0; i < steps; ++i) {
        const float a =
            (static_cast<float>(i) * 6.2831853f) / static_cast<float>(steps);
        const int x = cx + static_cast<int>(std::lround(std::cos(a) * radius));
        const int y = cy + static_cast<int>(std::lround(std::sin(a) * radius));
        SDL_RenderPoint(r, static_cast<float>(x), static_cast<float>(y));
    }
}

void StrokeRing(SDL_Renderer* r, int cx, int cy, int radius, Uint8 R, Uint8 G,
                Uint8 B) {
    StrokeCircle(r, cx, cy, radius, R, G, B);
    if (radius > 1) StrokeCircle(r, cx, cy, radius - 1, R, G, B);
}

int WorldRadiusToScreen(int32_t radius_sq, float scale) {
    const float world_r = std::sqrt(static_cast<float>(radius_sq));
    const int px = static_cast<int>(std::lround(world_r * scale));
    return px < 1 ? 1 : px;
}

void DrawLinePitch(SDL_Renderer* r, const DebugView& view, int match_w,
                   int match_h, int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
    const auto a = PitchToScreen(x0, y0, view, match_w, match_h);
    const auto b = PitchToScreen(x1, y1, view, match_w, match_h);
    SDL_RenderLine(r, static_cast<float>(a.x), static_cast<float>(a.y),
                   static_cast<float>(b.x), static_cast<float>(b.y));
}

void DrawBoxPitch(SDL_Renderer* r, const DebugView& view, int match_w,
                  int match_h, int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
    DrawLinePitch(r, view, match_w, match_h, x0, y0, x1, y0);
    DrawLinePitch(r, view, match_w, match_h, x1, y0, x1, y1);
    DrawLinePitch(r, view, match_w, match_h, x1, y1, x0, y1);
    DrawLinePitch(r, view, match_w, match_h, x0, y1, x0, y0);
}

void DrawLandmarks(SDL_Renderer* r, const DebugView& view, int match_w,
                   int match_h) {
    SDL_SetRenderDrawColor(r, 40, 120, 55, 255);
    // Outer dead-ball / playable frame (clipped to view).
    DrawBoxPitch(r, view, match_w, match_h, kViewMinX, kViewMinY, kViewMaxX,
                 kViewMaxY);
    DrawBoxPitch(r, view, match_w, match_h, kPlayableMinX, kPlayableMinY,
                 kPlayableMaxX, kPlayableMaxY);

    // Halfway + centre spot.
    DrawLinePitch(r, view, match_w, match_h, kPlayableMinX, kCentreSpotY,
                  kPlayableMaxX, kCentreSpotY);
    {
        const auto c =
            PitchToScreen(kCentreSpotX, kCentreSpotY, view, match_w, match_h);
        FillCircle(r, c.x, c.y, 1, 50, 140, 65);
        const float scale = PitchUniformScale(view, match_w, match_h);
        const int cr =
            static_cast<int>(std::lround(static_cast<float>(kCentreCircleR) * scale));
        StrokeCircle(r, c.x, c.y, cr < 2 ? 2 : cr, 40, 120, 55);
    }

    // Penalty boxes (SIMULATION.md / SETPIECES.md).
    DrawBoxPitch(r, view, match_w, match_h, kPenBoxXMin, kPlayableMinY,
                 kPenBoxXMax, kPenaltyBoxTopY);
    DrawBoxPitch(r, view, match_w, match_h, kPenBoxXMin, kPenaltyBoxBotY,
                 kPenBoxXMax, kPlayableMaxY);

    // Goal mouths.
    SDL_SetRenderDrawColor(r, 55, 140, 70, 255);
    DrawLinePitch(r, view, match_w, match_h, kGoalMouthMinX, kPlayableMinY,
                  kGoalMouthMaxX, kPlayableMinY);
    DrawLinePitch(r, view, match_w, match_h, kGoalMouthMinX, kPlayableMaxY,
                  kGoalMouthMaxX, kPlayableMaxY);
}

// Without a camera (tests, and the no-packs fallback path) show the whole dead-ball
// box, which is what C1a did before C2 existed.
DebugView ViewFor(const Camera* camera) {
    return camera ? camera->View() : FullPitchView();
}

const char* GameStateName(GameState gs) {
    switch (gs) {
    case GameState::PlayersToInitialPositions: return "SETUP";
    case GameState::GoalOutLeft:               return "GK-L";
    case GameState::GoalOutRight:              return "GK-R";
    case GameState::KeeperHoldsBall:           return "HOLD";
    case GameState::CornerLeft:                return "CR-L";
    case GameState::CornerRight:               return "CR-R";
    case GameState::FreeKickLeft1:             return "FK-L1";
    case GameState::FreeKickLeft2:             return "FK-L2";
    case GameState::FreeKickLeft3:             return "FK-L3";
    case GameState::FreeKickCentre:            return "FK-C";
    case GameState::FreeKickRight1:            return "FK-R1";
    case GameState::FreeKickRight2:            return "FK-R2";
    case GameState::FreeKickRight3:            return "FK-R3";
    case GameState::Foul:                      return "FOUL";
    case GameState::Penalty:                   return "PEN";
    case GameState::ThrowInForwardRight:       return "TI-FR";
    case GameState::ThrowInCentreRight:        return "TI-CR";
    case GameState::ThrowInBackRight:          return "TI-BR";
    case GameState::ThrowInForwardLeft:        return "TI-FL";
    case GameState::ThrowInCentreLeft:         return "TI-CL";
    case GameState::ThrowInBackLeft:           return "TI-BL";
    case GameState::StartingGame:              return "KO";
    case GameState::CameraGoingToShowers:      return "SHOWER";
    case GameState::GoingToHalfTime:           return "→HT";
    case GameState::PlayersGoingToShower:      return "SHOWER";
    case GameState::ResultOnHalfTime:          return "HT-RES";
    case GameState::ResultAfterGame:           return "FT-RES";
    case GameState::FirstExtraStarting:        return "ET";
    case GameState::FirstExtraEnded:           return "ET-END";
    case GameState::FirstHalfEnded:            return "HT";
    case GameState::GameEnded:                 return "FT";
    case GameState::Penalties:                 return "PENS";
    }
    return "?";
}

const char* PhaseLabel(MatchPhase phase, uint8_t period) {
    switch (phase) {
    case MatchPhase::KickOff:  return "KO";
    case MatchPhase::InPlay:   return period == 0 ? "1H" : "2H";
    case MatchPhase::Goal:     return "GOAL";
    case MatchPhase::HalfTime: return "HT";
    case MatchPhase::FullTime: return "FT";
    case MatchPhase::SetPiece: return "SP";
    }
    return "?";
}

constexpr int kHeightPixelsPerUnit = 1;
constexpr int kMaxHeightLift = 40;

// REFEREE.md §4 lifts the booked player's number by kPlayerNumberOffset = 20. The
// controlled-player marker is the same device (SIMULATION.md §2's
// UpdateControlledPlayerNumbers), so it floats at the same height.
constexpr int kPlayerNumberOffset = 20;

// A pitch slot's entry in its side's team sheet. Slots are 0..10 home, 11..21 away.
const SquadPlayer* SheetEntry(const MatchState& state, int side, int slot) {
    const int index = slot - side * 11;
    if (index < 0 || index >= static_cast<int>(kMatchSquadSize)) return nullptr;
    return &state.sides[static_cast<size_t>(side)].squad[static_cast<size_t>(index)];
}

uint8_t FaceOf(const MatchState& state, int side, int slot) {
    const SquadPlayer* p = SheetEntry(state, side, slot);
    if (!p) return 0;
    return p->face < kFaceCount ? p->face : 0;
}

// The controlled player wears his number above his head — the original's own marker,
// and what C1a's white ring stood in for.
void DrawShirtNumber(SDL_Renderer* r, const MatchState& state, IAssetSource* assets,
                     const GamePalette& gpal, int side, int slot, ScreenPosF sp,
                     float scale) {
    if (!assets) return;
    // A CPU side has no controlled player worth marking.
    if (state.sides[static_cast<size_t>(side)].control.player_number == 0) return;
    const SquadPlayer* p = SheetEntry(state, side, slot);
    if (!p) return;
    int number = p->shirt_number;
    if (number <= 0) number = slot - side * 11 + 1;
    const SpriteSheet* glyph = assets->Number(number);
    if (!glyph) return;
    DrawIndexedSprite(r, *glyph, gpal.rgba, gpal.count, sp.x,
                      sp.y - static_cast<float>(kPlayerNumberOffset) * scale, scale);
}

} // namespace

void DrawMatch(SDL_Renderer* r, const MatchState& state, int match_w, int match_h,
               const Camera* camera, IAssetSource* assets, PitchAtlas* atlas,
               const KitBank* kits) {
    const DebugView view = ViewFor(camera);

    bool drew_tiles = false;
    if (assets && atlas) {
        const PitchTiles* tiles = assets->Pitch(PitchType::Normal);
        if (tiles && atlas->Ensure(r, tiles)) {
            // Letterbox bars outside the used pitch mapping stay dark from clear.
            SDL_SetRenderDrawColor(r, 12, 40, 20, 255);
            SDL_FRect fill{0, 0, static_cast<float>(match_w),
                           static_cast<float>(match_h)};
            SDL_RenderFillRect(r, &fill);
            atlas->Draw(r, view, match_w, match_h);
            drew_tiles = true;
        }
    }
    if (!drew_tiles) {
        SDL_SetRenderDrawColor(r, 20, 90, 40, 255);
        SDL_FRect pitch{0, 0, static_cast<float>(match_w),
                        static_cast<float>(match_h)};
        SDL_RenderFillRect(r, &pitch);
        // Landmarks only when there is no tile art — tiles already carry lines/goals.
        DrawLandmarks(r, view, match_w, match_h);
    }

    const float scale = PitchUniformScale(view, match_w, match_h);
    const float spr_scale = scale < 1.f ? 1.f : scale;
    const GamePalette gpal = assets ? assets->Palette() : GamePalette{};
    const bool use_sprites = assets != nullptr;

    // Y-sort draw list: players + ball as a late sprite at ball y.
    struct DrawItem {
        int16_t y     = 0;
        int     kind  = 0; // 0 player slot, 1 ball
        int     index = 0;
    };
    std::array<DrawItem, kPitchPlayers + 1> items{};
    int n_items = 0;
    for (int i = 0; i < kPitchPlayers; ++i) {
        // Sent off, or never spawned in a C1b sandbox: not on the pitch.
        if (IsOffPitch(state.players[static_cast<size_t>(i)])) continue;
        items[static_cast<size_t>(n_items++)] = {
            state.players[static_cast<size_t>(i)].pos.y.Whole(), 0, i};
    }
    items[static_cast<size_t>(n_items++)] = {state.ball.pos.y.Whole(), 1, 0};
    std::sort(items.begin(), items.begin() + n_items,
              [](const DrawItem& a, const DrawItem& b) { return a.y < b.y; });

    const int player_r = WorldRadiusToScreen(kCaptureRadiusSq, scale);
    const int ctrl_r   = WorldRadiusToScreen(kCloseRadiusSq, scale);

    for (int ii = 0; ii < n_items; ++ii) {
        const DrawItem& it = items[static_cast<size_t>(ii)];
        if (it.kind == 1) {
            const int16_t bx = state.ball.pos.x.Whole();
            const int16_t by = state.ball.pos.y.Whole();
            const int z = std::max(0, static_cast<int>(state.ball.pos.z.Whole()));
            const ScreenPosF ground = PitchToScreenF(bx, by, view, match_w, match_h);
            const int lift = std::min(z * kHeightPixelsPerUnit, kMaxHeightLift);
            const float ball_y = ground.y - static_cast<float>(lift);

            const int ball_frame = state.ball.image_index < 0
                                       ? 0
                                       : static_cast<int>(state.ball.image_index);
            const SpriteSheet* ball   = assets ? assets->Ball(ball_frame) : nullptr;
            const SpriteSheet* shadow = assets ? assets->BallShadow() : nullptr;
            if (use_sprites && ball) {
                // The shadow is the original's own sprite and stays on the ground while
                // the ball lifts — which is the whole reason it is a separate sprite.
                if (shadow)
                    DrawIndexedSprite(r, *shadow, gpal.rgba, gpal.count, ground.x,
                                      ground.y, spr_scale);
                DrawIndexedSprite(r, *ball, gpal.rgba, gpal.count, ground.x, ball_y,
                                  spr_scale);
            } else {
                FillCircle(r, static_cast<int>(ground.x + 0.5f),
                           static_cast<int>(ground.y + 0.5f), 0, 15, 50, 25);
                FillCircle(r, static_cast<int>(ground.x + 0.5f),
                           static_cast<int>(ball_y + 0.5f), z >= 12 ? 1 : 0, 245,
                           245, 245);
            }
            continue;
        }

        const int i = it.index;
        const Entity& e = state.players[static_cast<size_t>(i)];
        const ScreenPosF sp =
            PitchToScreenF(e.pos.x.Whole(), e.pos.y.Whole(), view, match_w, match_h);
        const bool home = e.team_number == 1;
        const int side = home ? 0 : 1;
        const bool ctrl =
            state.sides[static_cast<size_t>(side)].control.controlled_slot == i;
        const bool keeper = e.player_ordinal == 1;

        if (use_sprites) {
            // The frame is the simulation's, not the renderer's: image_index comes out
            // of the deterministic stepper (core/animation.hpp).
            const int frame = e.image_index < 0 ? 0 : static_cast<int>(e.image_index);
            const ShirtGeometry geo =
                kits ? kits->Geometry(side) : ShirtGeometry::VerticalStripes;
            const SpriteSheet* sheet =
                keeper ? assets->Keeper(frame) : assets->Player(geo, frame);
            if (!sheet) sheet = assets->Player(geo, 0);

            // A kit is a palette. One bake per (side, face) at kickoff; the sprite
            // texture cache keys on the palette pointer, so this costs one texture set.
            std::span<const uint8_t> pal = gpal.rgba;
            uint32_t pal_count = gpal.count;
            if (kits && kits->Ready()) {
                const uint8_t face = FaceOf(state, side, i);
                const KitPalette& kp =
                    keeper ? kits->Keeper(side) : kits->Outfield(side, face);
                pal = kp.Bytes();
                pal_count = kPaletteEntries;
            }

            if (sheet) {
                const int z =
                    std::max(0, static_cast<int>(e.pos.z.Whole()));
                const float lift =
                    static_cast<float>(std::min(z * kHeightPixelsPerUnit, kMaxHeightLift));
                DrawIndexedSprite(r, *sheet, pal, pal_count, sp.x, sp.y - lift,
                                  spr_scale);
                if (ctrl) DrawShirtNumber(r, state, assets, gpal, side, i, sp, spr_scale);
                continue;
            }
        }

        // Fallback dots when packs missing.
        const bool gk = e.player_ordinal == 1;
        const int rad = ctrl ? ctrl_r : player_r;
        Uint8 R, G, B;
        if (home) {
            R = ctrl ? 70 : (gk ? 25 : 40);
            G = ctrl ? 150 : (gk ? 90 : 120);
            B = ctrl ? 255 : (gk ? 180 : 220);
        } else {
            R = ctrl ? 255 : (gk ? 160 : 220);
            G = ctrl ? 90 : (gk ? 40 : 60);
            B = ctrl ? 90 : (gk ? 40 : 60);
        }
        FillCircle(r, static_cast<int>(sp.x + 0.5f), static_cast<int>(sp.y + 0.5f),
                   rad, R, G, B);
    }
}

void DrawMatchStatus(SDL_Renderer* r, const MatchState& state, const char* mode_tag) {
    SDL_SetRenderDrawColor(r, 235, 235, 235, 255);
    char line[96];
    const int minute = static_cast<int>(state.clock.displayed_minute);
    int sec = static_cast<int>(state.clock.game_seconds);
    if (sec < 0) sec = 0;
    if (sec > 59) sec = 59;
    SDL_snprintf(line, sizeof line, "%s%d'%02d  %u-%u  %s", mode_tag ? mode_tag : "",
                 minute, sec, state.score[0], state.score[1],
                 PhaseLabel(state.phase, state.clock.period));
    SDL_RenderDebugText(r, 8.0f, 8.0f, line);

    if (state.phase == MatchPhase::HalfTime) {
        SDL_SetRenderDrawColor(r, 255, 220, 120, 255);
        SDL_snprintf(line, sizeof line, "[HALF TIME] %u-%u", state.score[0],
                     state.score[1]);
        SDL_RenderDebugText(r, 8.0f, 22.0f, line);
    } else if (state.phase == MatchPhase::FullTime) {
        SDL_SetRenderDrawColor(r, 255, 200, 100, 255);
        SDL_snprintf(line, sizeof line, "[FULL TIME] %u-%u  — ESC menu", state.score[0],
                     state.score[1]);
        SDL_RenderDebugText(r, 8.0f, 22.0f, line);
    }
}

void DrawMatchHud(SDL_Renderer* r, const MatchState& state, const Camera* camera,
                  int sim_speed_pct, const char* mode_tag,
                  const tracekit::KickTelemetry* kick) {
    SDL_SetRenderDrawColor(r, 235, 235, 235, 255);
    char line[192];

    const int minute = static_cast<int>(state.clock.displayed_minute);
    int sec = static_cast<int>(state.clock.game_seconds);
    if (sec < 0) sec = 0;
    if (sec > 59) sec = 59;

    const GameState gs = GetGameState(state);
    const GameStatePl pl = GetPl(state);
    const char* pl_tag =
        (pl == GameStatePl::InProgress)     ? ""
        : (pl == GameStatePl::Stopped)      ? " STOP"
        : (pl == GameStatePl::WaitingOnPlayer) ? " WAIT"
                                              : "";

    const int whole = sim_speed_pct / 100;
    const int frac = sim_speed_pct % 100;
    SDL_snprintf(line, sizeof line,
                 "%s%d'%02d  %u-%u  %s  %s%s  cam(%d,%d) lead(%+d,%+d)  sim %d.%02dx",
                 mode_tag ? mode_tag : "", minute, sec, state.score[0],
                 state.score[1], PhaseLabel(state.phase, state.clock.period),
                 GameStateName(gs), pl_tag, camera ? camera->X() : 0,
                 camera ? camera->Y() : 0, camera ? camera->LeadX() : 0,
                 camera ? camera->LeadY() : 0, whole, frac);
    SDL_RenderDebugText(r, 8.0f, 8.0f, line);

    const auto& tc = state.sides[0].control;
    SDL_snprintf(line, sizeof line,
                 "P%d has=%d tgt=%d  z=%d spd=%d  spin=%d%s%s  fire=%d",
                 static_cast<int>(tc.controlled_slot),
                 static_cast<int>(tc.player_has_ball),
                 static_cast<int>(tc.pass_to_slot), state.ball.pos.z.Whole(),
                 state.ball.speed, static_cast<int>(tc.spin_timer),
                 tc.spin_cw ? " CW" : "", tc.spin_ccw ? " CCW" : "",
                 static_cast<int>(tc.fire_counter));
    SDL_RenderDebugText(r, 8.0f, 20.0f, line);

    // B6a control line: the charge in progress, then what the last strike did.
    // Feel complaints should be reportable as tick counts.
    const bool charging = tc.fire_pressed != 0 && tc.fire_counter > 0;
    char charge[48] = "";
    if (charging) {
        SDL_snprintf(charge, sizeof charge, "hold %d/%d %s", tc.fire_counter,
                     static_cast<int>(kFireHoldThreshold),
                     tc.fire_counter >= kFireHoldThreshold ? "SHOT" : "pass");
    }
    if (kick && kick->strike_tick >= 0) {
        const int since = static_cast<int>(state.tick) -
                          static_cast<int>(kick->strike_tick);
        SDL_snprintf(line, sizeof line,
                     "%-22s last %s: press->strike=%d  latch=%s@%d  %s  +%dt",
                     charge, kick->was_pass ? "pass" : "shot",
                     static_cast<int>(kick->press_to_strike),
                     kick->latch_side == 0 ? "-"
                                           : (kick->latch_side > 0 ? "CW" : "CCW"),
                     static_cast<int>(kick->latch_spin),
                     tracekit::VerticalToken(kick->vertical), since);
    } else {
        SDL_snprintf(line, sizeof line, "%-22s window %d/%d", charge,
                     tc.spin_timer < 0 ? 0 : static_cast<int>(tc.spin_timer) + 1,
                     static_cast<int>(kAftertouchWindow));
    }
    SDL_RenderDebugText(r, 8.0f, 32.0f, line);

    SDL_RenderDebugText(
        r, 8.0f, 44.0f,
        "arrows=move  space=kick  R=reset  F1=debug  -/+=speed  ESC=menu");
}

} // namespace at::render
