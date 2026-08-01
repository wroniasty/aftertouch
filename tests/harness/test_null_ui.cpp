#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "app_model.hpp"
#include "ui/null_backend.hpp"

using namespace at;

TEST_CASE("NullUiBackend drains scripted intents without SDL") {
    NullUiBackend ui;
    ui.Script({
        Intent{Intent::Kind::StartMatch},
        Intent{Intent::Kind::ExitMatch},
        Intent{Intent::Kind::Quit},
    });

    AppModel model;
    REQUIRE(ui.Init(nullptr, nullptr));
    ui.BeginFrame();
    CHECK(ui.DrawScreen(ScreenId::MainMenu, model).kind == Intent::Kind::StartMatch);
    CHECK(ui.DrawScreen(ScreenId::Match, model).kind == Intent::Kind::ExitMatch);
    CHECK(ui.DrawScreen(ScreenId::MainMenu, model).kind == Intent::Kind::Quit);
    CHECK(ui.DrawScreen(ScreenId::MainMenu, model).kind == Intent::Kind::None);
    CHECK(ui.Remaining() == 0u);
    ui.EndFrame(nullptr);
    ui.Shutdown();
}
