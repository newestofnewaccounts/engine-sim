#ifndef ATG_ENGINE_SIM_UI_THEME_H
#define ATG_ENGINE_SIM_UI_THEME_H

#include "delta.h"

// Simple Edition: colours for the clean, card-based look (inspired by modern
// car touchscreens), in a dark and a light variant.
struct UiTheme {
    bool dark = true;

    ysVector page;        // window background
    ysVector card;        // panels / cards
    ysVector control;     // buttons and slider tracks on a card
    ysVector controlHover;
    ysVector controlPressed;
    ysVector text;        // main text
    ysVector textMuted;   // secondary text and labels
    ysVector accent;      // selected / active (blue)
    ysVector accentText;  // text on the accent colour
    ysVector danger;      // recording, redline (red)
    ysVector warning;     // starter (amber)
    ysVector success;     // steady / running (green)
    ysVector divider;

    static UiTheme make(bool dark) {
        UiTheme t;
        t.dark = dark;

        if (dark) {
            t.page = ysColor::srgbiToLinear(0x000000);
            t.card = ysColor::srgbiToLinear(0x171A1F);
            t.control = ysColor::srgbiToLinear(0x262A31);
            t.controlHover = ysColor::srgbiToLinear(0x30353D);
            t.controlPressed = ysColor::srgbiToLinear(0x3A4049);
            t.text = ysColor::srgbiToLinear(0xFFFFFF);
            t.textMuted = ysColor::srgbiToLinear(0x9CA0A8);
            t.divider = ysColor::srgbiToLinear(0x2C3036);
        }
        else {
            t.page = ysColor::srgbiToLinear(0xEEEFF1);
            t.card = ysColor::srgbiToLinear(0xFFFFFF);
            t.control = ysColor::srgbiToLinear(0xEEEFF1);
            t.controlHover = ysColor::srgbiToLinear(0xE2E4E7);
            t.controlPressed = ysColor::srgbiToLinear(0xD5D8DC);
            t.text = ysColor::srgbiToLinear(0x171A20);
            t.textMuted = ysColor::srgbiToLinear(0x5C5E62);
            t.divider = ysColor::srgbiToLinear(0xDDDFE2);
        }

        t.accent = ysColor::srgbiToLinear(0x3E6AE1);
        t.accentText = ysColor::srgbiToLinear(0xFFFFFF);
        t.danger = ysColor::srgbiToLinear(0xE82127);
        t.warning = ysColor::srgbiToLinear(0xF4A100);
        t.success = ysColor::srgbiToLinear(0x2FB463);
        return t;
    }
};

#endif /* ATG_ENGINE_SIM_UI_THEME_H */
