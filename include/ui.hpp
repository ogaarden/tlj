#ifndef UI_HPP
#define UI_HPP

#include <raylib.h>

// =====================================================================
// FELLES UI: skalering, paneler, logo og slottsbakgrunnen
//
// Alle menyer tegnes på et virtuelt lerret på 1280x700 (Settings::SCREEN_WIDTH/HEIGHT).
// Lerretet skaleres og sentreres i vinduet, så layouten ser lik ut uansett
// vindusstørrelse. Bakgrunner og mørklegging tegnes utenfor lerretet, rett på
// hele vinduet, så det aldri blir svarte kanter.
// =====================================================================

namespace UI {
    // Hvor mye lerretet er skalert (1.0 = 1280x700-vindu)
    float Scale();

    // Alt mellom BeginCanvas/EndCanvas tegnes i virtuelle 1280x700-koordinater
    void BeginCanvas();
    void EndCanvas();

    // Farger som går igjen i hele UI-et (gull, kongerødt og mørk stein)
    constexpr Color GOLD_LIGHT = { 255, 214,  90, 255 };
    constexpr Color GOLD_DARK  = { 150, 105,  25, 255 };
    constexpr Color ROYAL_RED  = { 150,  24,  36, 255 };
    constexpr Color PANEL_BG   = {  22,  18,  30, 215 };
    constexpr Color PANEL_EDGE = {  90,  74,  50, 255 };
    constexpr Color INK        = {  12,  10,  16, 255 };

    // Mørkt panel med gullkant og små nagler i hjørnene
    void DrawPanel(Rectangle r, float scale = 1.0f, Color edge = PANEL_EDGE, Color fill = PANEL_BG);

    // Tekst med mørk kontur og skygge, så den kan leses over hva som helst
    void DrawOutlinedText(const char* text, float x, float y, float size, Color color, float outline = 2.0f);
    void DrawCenteredText(const char* text, float centerX, float y, float size, Color color, float outline = 2.0f);

    // Rund glød som går fra `inner` i midten til `outer` i kanten.
    // Egen versjon fordi DrawCircleGradient har ulik signatur i raylib 5.5 og nyere.
    void DrawGlow(Vector2 center, float radius, Color inner, Color outer);

    // Fylt bar (HP, XP, boss) med glans øverst og ramme
    void DrawBar(Rectangle r, float pct, Color fill, Color back, float scale = 1.0f);

    // "THE LAST" over et stort, bølgende "JESTER" med narrelue på J-en.
    // centerX/top er i lerret-koordinater, width er hvor bred JESTER skal bli.
    void DrawTitleLogo(float centerX, float top, float width, float time);

    // Slottet i bakgrunnen (tegnes over hele vinduet). dim: 0 = fullt lys, 1 = helt svart
    void DrawCastleBackdrop(float time, float dim = 0.0f);

    // F11: veksle fullskjerm (kantløst vindu)
    void HandleWindowShortcuts();
}

#endif // UI_HPP
