#include "ui_imgui/fonts.hpp"
#include <imgui.h>

namespace at::ui {

void LoadFonts() {
    ImGuiIO& io = ImGui::GetIO();

    ImFontConfig cfg;
    cfg.OversampleH = 1;      // no horizontal oversampling
    cfg.OversampleV = 1;      // no vertical oversampling
    cfg.PixelSnapH  = true;   // snap glyphs to the pixel grid

    // The player database will contain Polish, Turkish, Czech, Greek and
    // Cyrillic names. GetGlyphRangesDefault() covers Basic Latin and Latin-1
    // only, which loses every diacritic that matters.
    static ImVector<ImWchar> ranges;
    ImFontGlyphRangesBuilder builder;
    builder.AddRanges(io.Fonts->GetGlyphRangesDefault());
    builder.AddRanges(io.Fonts->GetGlyphRangesGreek());
    builder.AddRanges(io.Fonts->GetGlyphRangesCyrillic());

    static const ImWchar latin_ext[] = {
        0x0100, 0x017F,   // Latin Extended-A: Polish, Czech, Turkish, Croatian
        0x0180, 0x024F,   // Latin Extended-B
        0,
    };
    builder.AddRanges(latin_ext);
    builder.BuildRanges(&ranges);

    // Load at an exact integer size. Fractional sizes defeat pixel snapping.
    ImFont* font = io.Fonts->AddFontFromFileTTF(
        AT_ASSET_DIR "/fonts/ui.ttf", 16.0f, &cfg, ranges.Data);

    // If the TTF is missing, AddFontFromFileTTF returns null and ImGui silently
    // falls back to its built-in font (which lacks the diacritics above). Make
    // that failure loud rather than letting it hide behind boxes at runtime.
    if (!font) {
        io.Fonts->AddFontDefault();
    }
}

} // namespace at::ui
