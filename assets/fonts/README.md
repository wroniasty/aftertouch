# UI font

`ui.ttf` is **DejaVu Sans**, used as placeholder UI art for milestone 1. It is a
libre font (DejaVu Fonts License, based on the Bitstream Vera license) and is
freely redistributable, which is why it can live in the repo — unlike the
original game's assets (see PLAN.md section 10) or a proprietary system font.

It was chosen because it covers the glyph ranges the player database needs:
Latin Extended-A/B (Polish, Czech, Turkish, Croatian), Greek and Cyrillic.

Replace it with your own art whenever you like; `src/app/ui_imgui/fonts.cpp`
loads exactly `assets/fonts/ui.ttf` at an integer pixel size.

DejaVu Fonts License: https://dejavu-fonts.github.io/License.html
