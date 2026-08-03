#include "ui_imgui/screens/sandbox_menu.hpp"

#include <imgui.h>

#include <cfloat>
#include <cstdio>

namespace at::ui {

namespace {

using mode::kSandboxMaxOutfield;
using mode::SandboxConfig;
using mode::SandboxPlayer;

// Attributes are 0–7 whole bytes (A5 / DATA.md §3, corrected by B13 / R2).
constexpr int kAttrMin = 0;
constexpr int kAttrMax = static_cast<int>(at::kAttrMax);

struct AttrColumn {
    const char* label;
    uint8_t PlayerAttrs::*field;
};

constexpr AttrColumn kAttrColumns[] = {
    {"Pas", &PlayerAttrs::passing},   {"Sho", &PlayerAttrs::shooting},
    {"Hea", &PlayerAttrs::heading},   {"Tac", &PlayerAttrs::tackling},
    {"Ctl", &PlayerAttrs::ball_control}, {"Spd", &PlayerAttrs::speed},
    {"Fin", &PlayerAttrs::finishing},
};
constexpr int kAttrColumnCount =
    static_cast<int>(sizeof(kAttrColumns) / sizeof(kAttrColumns[0]));

// Real-time budget for 90 displayed minutes, per MatchClock's length table.
const char* const kLengthLabels[4] = {
    "shortest (~3 min)", "short (~5 min)", "medium (~7 min)", "longest (~10 min)"};

void SetAll(SandboxConfig& cfg, uint8_t v) {
    for (int i = 0; i < kSandboxMaxOutfield; ++i) {
        for (const AttrColumn& c : kAttrColumns) cfg.field[i].attrs.*c.field = v;
    }
}

bool AttrCell(const char* id, uint8_t& value) {
    int v = static_cast<int>(value);
    ImGui::PushID(id);
    ImGui::SetNextItemWidth(-FLT_MIN);
    const bool changed = ImGui::DragInt("##a", &v, 0.1f, kAttrMin, kAttrMax);
    ImGui::PopID();
    if (changed) {
        if (v < kAttrMin) v = kAttrMin;
        if (v > kAttrMax) v = kAttrMax;
        value = static_cast<uint8_t>(v);
    }
    return changed;
}

void DrawKeeperRow(const char* label, SandboxPlayer& p) {
    ImGui::PushID(label);
    int sk = static_cast<int>(p.goalie_skill);
    ImGui::SetNextItemWidth(180.0f);
    if (ImGui::SliderInt(label, &sk, kAttrMin, kAttrMax))
        p.goalie_skill = static_cast<uint8_t>(sk);
    ImGui::PopID();
}

} // namespace

SandboxAction DrawSandboxMenu(SandboxConfig& cfg) {
    SandboxAction action = SandboxAction::None;

    ImGui::SetNextWindowPos(ImVec2(40, 40), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(720, 620), ImGuiCond_FirstUseEver);
    ImGui::Begin("SANDBOX — test match setup");

    ImGui::TextUnformatted(
        "N players of one side against a lone opposing keeper. R resets to this "
        "kickoff during the match.");
    ImGui::Separator();

    int n = static_cast<int>(cfg.outfield_count);
    if (ImGui::SliderInt("Outfield players", &n, 1, kSandboxMaxOutfield))
        cfg.outfield_count = static_cast<uint8_t>(n);
    ImGui::Checkbox("Own goalkeeper", &cfg.own_keeper);

    int dir = cfg.attack_down ? 0 : 1;
    ImGui::TextUnformatted("Direction of play");
    ImGui::RadioButton("Attack down (bottom goal)", &dir, 0);
    ImGui::SameLine();
    ImGui::RadioButton("Attack up (top goal)", &dir, 1);
    cfg.attack_down = (dir == 0);

    int roles = cfg.spawn_as_attackers ? 0 : 1;
    ImGui::TextUnformatted("Tactic roles");
    ImGui::RadioButton("Forward (attackers first)", &roles, 0);
    ImGui::SameLine();
    ImGui::RadioButton("Back (defenders first)", &roles, 1);
    cfg.spawn_as_attackers = (roles == 0);

    int len = static_cast<int>(cfg.game_length & 3u);
    ImGui::SetNextItemWidth(220.0f);
    if (ImGui::Combo("Match length", &len, kLengthLabels, 4))
        cfg.game_length = static_cast<uint8_t>(len & 3);
    ImGui::Checkbox("Reset to kickoff at half time", &cfg.reset_at_half_time);
    ImGui::SetItemTooltip(
        "Half time swaps ends, which would flip the direction chosen above.");

    int seed = static_cast<int>(cfg.seed);
    ImGui::SetNextItemWidth(220.0f);
    if (ImGui::InputInt("Seed (hex)", &seed, 0, 0,
                        ImGuiInputTextFlags_CharsHexadecimal))
        cfg.seed = static_cast<uint32_t>(seed);

    ImGui::SeparatorText("Attributes (0-7)");
    if (ImGui::Button("All 7")) SetAll(cfg, kAttrMax);
    ImGui::SameLine();
    if (ImGui::Button("All 4")) SetAll(cfg, kAttrMax / 2);
    ImGui::SameLine();
    if (ImGui::Button("All 0")) SetAll(cfg, 0);
    ImGui::SameLine();
    ImGui::TextDisabled("(drag a cell to edit)");

    if (ImGui::BeginTable("attrs", kAttrColumnCount + 1,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Player", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        for (const AttrColumn& c : kAttrColumns) ImGui::TableSetupColumn(c.label);
        ImGui::TableHeadersRow();

        // "Set all" row: writes the same value down every spawned column.
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("set all");
        static PlayerAttrs bulk{8, 8, 8, 8, 8, 8, 8, 0};
        for (int c = 0; c < kAttrColumnCount; ++c) {
            ImGui::TableNextColumn();
            char id[16];
            std::snprintf(id, sizeof id, "bulk%d", c);
            if (AttrCell(id, bulk.*kAttrColumns[c].field)) {
                const uint8_t v = bulk.*kAttrColumns[c].field;
                for (int i = 0; i < kSandboxMaxOutfield; ++i)
                    cfg.field[i].attrs.*kAttrColumns[c].field = v;
            }
        }

        for (int i = 0; i < static_cast<int>(cfg.outfield_count); ++i) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("player %d", i + 1);
            for (int c = 0; c < kAttrColumnCount; ++c) {
                ImGui::TableNextColumn();
                char id[16];
                std::snprintf(id, sizeof id, "p%d_%d", i, c);
                AttrCell(id, cfg.field[i].attrs.*kAttrColumns[c].field);
            }
        }
        ImGui::EndTable();
    }

    ImGui::SeparatorText("Goalkeepers");
    DrawKeeperRow("Opposing keeper skill", cfg.opponent_keeper);
    if (cfg.own_keeper) DrawKeeperRow("Own keeper skill", cfg.keeper);

    ImGui::Separator();
    if (ImGui::Button("START", ImVec2(200, 40))) action = SandboxAction::Start;
    ImGui::SameLine();
    if (ImGui::Button("Back", ImVec2(120, 40))) action = SandboxAction::Back;

    ImGui::End();
    return action;
}

} // namespace at::ui
