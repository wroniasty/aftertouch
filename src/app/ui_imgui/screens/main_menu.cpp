#include "ui_imgui/screens/main_menu.hpp"
#include "app_model.hpp"

#include <imgui.h>

namespace at::ui {

Intent DrawMainMenu(AppModel& model) {
    (void)model;
    Intent intent;

    ImGui::SetNextWindowPos(ImVec2(40, 40), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(320, 200), ImGuiCond_FirstUseEver);
    ImGui::Begin("aftertouch");
    if (ImGui::Button("MATCH", ImVec2(280, 48))) {
        intent.kind = Intent::Kind::StartMatch;
    }
    ImGui::End();

    return intent;
}

} // namespace at::ui
