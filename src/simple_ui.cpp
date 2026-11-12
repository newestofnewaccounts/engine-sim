#include "../include/simple_ui.h"

#include "../include/engine_sim_application.h"
#include "../include/ui_utilities.h"
#include "../include/sound_studio.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <iterator>
#include <sstream>

#undef min
#undef max

namespace {
    std::string percent(double v) {
        return std::to_string((int)std::round(v * 100.0)) + "%";
    }

    std::string fixed(double v, int precision) {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(precision) << v;
        return ss.str();
    }

    Bounds box(float left, float top, float width, float height) {
        return Bounds(width, height, { left, top }, Bounds::tl);
    }
}

// ============================================================================
// SimpleButton

SimpleButton::SimpleButton() {
    m_checkMouse = true;
    m_fontSize = 18.0f;
    m_active = false;
    m_enabled = true;
    m_useActiveColor = false;
    m_activeColor = ysMath::LoadVector(0.0f, 0.0f, 0.0f, 0.0f);
    m_alignLeft = false;
    m_quiet = false;
}

SimpleButton::~SimpleButton() {
    /* void */
}

void SimpleButton::update(float dt) {
    m_mouseBounds = m_bounds;
    UiElement::update(dt);
}

void SimpleButton::render() {
    const UiTheme &theme = m_app->getTheme();

    ysVector fill = m_quiet ? theme.card : theme.control;
    ysVector textColor = m_enabled ? theme.text : theme.textMuted;
    if (m_active) {
        fill = m_useActiveColor ? m_activeColor : theme.accent;
        textColor = theme.accentText;
    }

    if (m_enabled) {
        if (isMouseHeld()) fill = mix(fill, theme.text, 0.16f);
        else if (isMouseOver()) fill = mix(fill, theme.text, 0.07f);
    }

    const float radius = std::min(12.0f, m_bounds.height() * 0.3f);
    drawRoundedBox(m_bounds, radius, fill);

    if (m_alignLeft) {
        drawTextColored(m_text, m_bounds.inset(16.0f), m_fontSize, Bounds::lm, Bounds::lm, textColor);
    }
    else {
        drawCenteredTextColored(m_text, m_bounds, m_fontSize, textColor);
    }

    UiElement::render();
}

void SimpleButton::onMouseClick(const Point &mouseLocal) {
    UiElement::onMouseClick(mouseLocal);
    if (m_enabled && m_onClick) m_onClick();
}

// ============================================================================
// SimpleSlider

SimpleSlider::SimpleSlider() {
    m_checkMouse = true;
    m_value = nullptr;
    m_changed = nullptr;
    m_fontSize = 18.0f;
    m_color = ysMath::LoadVector(0.0f, 0.0f, 0.0f, 0.0f);
}

SimpleSlider::~SimpleSlider() {
    /* void */
}

void SimpleSlider::update(float dt) {
    m_mouseBounds = m_bounds;
    UiElement::update(dt);
}

void SimpleSlider::render() {
    const UiTheme &theme = m_app->getTheme();
    const double value = std::min(1.0, std::max(0.0, (m_value != nullptr) ? *m_value : 0.0));
    const float radius = std::min(12.0f, m_bounds.height() * 0.3f);

    ysVector track = theme.control;
    if (isMouseHeld()) track = mix(track, theme.text, 0.10f);
    else if (isMouseOver()) track = mix(track, theme.text, 0.05f);
    drawRoundedBox(m_bounds, radius, track);

    // Filled part, like a touchscreen slider
    if (value > 0.001) {
        Bounds filled = m_bounds;
        filled.m1.x = m_bounds.m0.x + std::max(radius * 2, (float)(m_bounds.width() * value));
        drawRoundedBox(filled, radius, mix(track, m_color, theme.dark ? 0.75f : 0.55f));
    }

    const std::string valueText = m_format ? m_format(value) : percent(value);
    const Bounds inner = m_bounds.inset(14.0f);
    drawTextColored(m_label, inner, m_fontSize, Bounds::lm, Bounds::lm, theme.text);
    drawTextColored(valueText, inner, m_fontSize, Bounds::rm, Bounds::rm, theme.text);
}

void SimpleSlider::onMouseDown(const Point &mouseLocal) {
    UiElement::onMouseDown(mouseLocal);
    setFromX(mouseLocal.x);
}

void SimpleSlider::onDrag(const Point &p0, const Point &mouse0, const Point &mouse) {
    setFromX(mouse.x);
}

void SimpleSlider::onMouseScroll(int scroll) {
    if (m_value == nullptr) return;
    *m_value = std::min(1.0, std::max(0.0, *m_value + (scroll > 0 ? 0.02 : -0.02)));
    if (m_changed != nullptr) *m_changed = true;
}

void SimpleSlider::setFromX(float x) {
    if (m_value == nullptr) return;

    const Bounds inner = m_bounds.inset(4.0f);
    double v = (x - inner.left()) / std::max(1.0f, inner.width());
    v = std::min(1.0, std::max(0.0, v));
    if (v < 0.02) v = 0.0;

    *m_value = v;
    if (m_changed != nullptr) *m_changed = true;
}

// ============================================================================
// SimpleControlBar

SimpleControlBar::SimpleControlBar() {
    m_state = nullptr;
    m_start = m_ignition = m_gearDown = m_gearUp = m_dyno = nullptr;
    m_engines = m_builder = m_view = m_help = m_studio = m_theme = nullptr;
    m_throttle = m_volume = nullptr;
}

SimpleControlBar::~SimpleControlBar() {
    /* void */
}

void SimpleControlBar::build(SimpleUiState *state) {
    m_state = state;

    auto button = [this](const std::string &text, std::function<void()> onClick) {
        SimpleButton *b = addElement<SimpleButton>();
        b->m_text = text;
        b->m_onClick = onClick;
        return b;
    };

    using T = SimpleCommand::Type;
    const UiTheme &theme = m_app->getTheme();

    m_start = button("Hold to start", nullptr);
    m_start->m_useActiveColor = true;
    m_start->m_activeColor = theme.warning;
    m_ignition = button("Ignition", [state]() { state->push(T::ToggleIgnition); });
    m_ignition->m_useActiveColor = true;
    m_ignition->m_activeColor = theme.success;
    m_gearDown = button("Gear -", [state]() { state->push(T::GearDown); });
    m_gearUp = button("Gear +", [state]() { state->push(T::GearUp); });

    m_throttle = addElement<SimpleSlider>();
    m_throttle->m_label = "Throttle";
    m_throttle->m_value = &state->throttle;
    m_throttle->m_color = theme.accent;

    m_dyno = button("Dyno", [state]() { state->push(T::ToggleDyno); });

    m_volume = addElement<SimpleSlider>();
    m_volume->m_label = "Volume";
    m_volume->m_value = &state->volume;
    m_volume->m_changed = &state->volumeChanged;
    m_volume->m_color = theme.accent;

    m_engines = button("Engines", [state]() { state->push(T::OpenEngines); });
    m_builder = button("Build engine", [state]() { state->push(T::OpenBuilder); });
    m_view = button("View", [state]() { state->push(T::CycleView); });
    m_help = button("Help", [state]() { state->push(T::OpenHelp); });
    m_studio = button("Sound studio", [state]() { state->push(T::ToggleStudio); });
    m_theme = button("Light", [state]() { state->push(T::ToggleTheme); });

    for (SimpleButton *b : { m_engines, m_builder, m_view, m_help, m_theme }) b->m_quiet = true;
}

void SimpleControlBar::update(float dt) {
    if (m_state == nullptr) return;

    // Lay out the controls left to right using relative widths
    struct Slot { UiElement *element; float weight; };
    const Slot slots[] = {
        { m_start, 1.6f },
        { m_ignition, 1.4f },
        { m_gearDown, 0.8f },
        { nullptr, 0.8f }, // gear display
        { m_gearUp, 0.8f },
        { m_throttle, 2.6f },
        { m_dyno, 0.9f },
        { m_volume, 1.8f },
        { m_engines, 1.2f },
        { m_builder, 1.5f },
        { m_view, 1.3f },
        { m_studio, 1.5f },
        { m_theme, 0.8f },
        { m_help, 0.8f }
    };

    float totalWeight = 0;
    for (const Slot &s : slots) totalWeight += s.weight;

    // A floating dock with a little space around it
    const float padding = 8.0f;
    const Bounds inner = m_bounds.inset(10.0f).inset(padding);
    const float available = inner.width() - padding * (float)(std::size(slots) - 1);
    const float fontSize = std::max(13.0f, std::min(19.0f, inner.height() * 0.36f));

    float x = inner.left();
    for (const Slot &s : slots) {
        const float w = available * s.weight / totalWeight;
        const Bounds b = box(x, inner.top(), w, inner.height());
        if (s.element != nullptr) {
            s.element->m_bounds = b;
        }
        else {
            m_gearBounds = b;
        }
        x += w + padding;
    }

    for (SimpleButton *b : { m_start, m_ignition, m_gearDown, m_gearUp, m_dyno, m_engines, m_builder, m_view, m_help, m_studio, m_theme }) {
        b->m_fontSize = fontSize;
    }
    m_throttle->m_fontSize = fontSize;
    m_volume->m_fontSize = fontSize;

    m_state->starterHeld = m_start->isMouseHeld();
    m_start->m_active = m_state->starterHeld;
    m_start->m_text = m_state->starterHeld ? "Starting..." : "Hold to start";

    m_ignition->m_active = m_state->ignitionOn;
    m_ignition->m_text = m_state->ignitionOn ? "Ignition on" : "Ignition off";

    m_dyno->m_active = m_state->dynoOn;

    switch (m_state->viewMode) {
    case 0: m_view->m_text = "View: Detailed"; break;
    case 1: m_view->m_text = "View: Engine"; break;
    default: m_view->m_text = "View: Simple"; break;
    }

    m_theme->m_text = m_app->getTheme().dark ? "Light" : "Dark";

    m_help->m_active = m_state->overlay == SimpleOverlay::Help;
    m_studio->m_active = m_state->studioOpen;
    m_studio->m_useActiveColor = m_state->recording;
    m_studio->m_activeColor = m_app->getTheme().danger;
    m_studio->m_text = m_state->recording ? "Recording" : "Sound studio";
    m_engines->m_active = m_state->overlay == SimpleOverlay::Engines;
    m_builder->m_active = m_state->overlay == SimpleOverlay::Builder;

    UiElement::update(dt);
}

void SimpleControlBar::render() {
    const UiTheme &theme = m_app->getTheme();
    drawRoundedBox(m_bounds.inset(10.0f), 18.0f, theme.card);

    if (m_state != nullptr) {
        const std::string gear = (m_state->gear < 0) ? "N" : std::to_string(m_state->gear + 1);
        const float h = m_gearBounds.height();
        drawCenteredTextColored(gear, m_gearBounds, std::max(18.0f, h * 0.62f), theme.text);
    }

    UiElement::render();
}

// ============================================================================
// SimpleMessageBanner

SimpleMessageBanner::SimpleMessageBanner() {
    m_state = nullptr;
}

SimpleMessageBanner::~SimpleMessageBanner() {
    /* void */
}

void SimpleMessageBanner::update(float dt) {
    if (m_state != nullptr && m_state->messageTimer > 0) {
        m_state->messageTimer = std::max(0.0f, m_state->messageTimer - dt);
    }

    UiElement::update(dt);
}

void SimpleMessageBanner::render() {
    if (m_state == nullptr || m_state->messageTimer <= 0 || m_state->message.empty()) return;

    // A pill-shaped toast at the top
    const UiTheme &theme = m_app->getTheme();
    const float fontSize = 20.0f;
    const float width = std::min(m_bounds.width() - 20.0f, textWidth(m_state->message, fontSize) + 56.0f);
    const Bounds b(width, 46.0f, m_bounds.getPosition(Bounds::tm) + Point(0.0f, -14.0f), Bounds::tm);

    drawRoundedBox(b, 23.0f, m_state->messageIsError ? theme.danger : theme.card);
    drawCenteredTextColored(
        m_state->message, b, fontSize, m_state->messageIsError ? theme.accentText : theme.text);
}

// ============================================================================
// SimpleOverlayPanel

SimpleOverlayPanel::SimpleOverlayPanel() {
    m_state = nullptr;
    m_close = m_prevPage = m_nextPage = m_openBuilder = m_build = m_cancel = nullptr;
    for (int i = 0; i < EngineRows; ++i) m_engineRows[i] = m_editRows[i] = nullptr;
    for (int i = 0; i < SimpleUiState::BuilderRows; ++i) m_builderLess[i] = m_builderMore[i] = nullptr;
}

SimpleOverlayPanel::~SimpleOverlayPanel() {
    /* void */
}

void SimpleOverlayPanel::build(SimpleUiState *state) {
    m_state = state;
    using T = SimpleCommand::Type;

    auto button = [this](const std::string &text, std::function<void()> onClick) {
        SimpleButton *b = addElement<SimpleButton>();
        b->m_text = text;
        b->m_onClick = onClick;
        return b;
    };

    m_close = button("Close", [state]() { state->push(T::CloseOverlay); });

    for (int i = 0; i < EngineRows; ++i) {
        m_engineRows[i] = button("", [state, i]() {
            state->push(T::LoadEngine, state->enginesPage * EngineRows + i);
        });
        m_editRows[i] = button("Edit", [state, i]() {
            state->push(T::EditCustomEngine, state->enginesPage * EngineRows + i);
        });
    }

    m_prevPage = button("< Previous", [state]() { state->push(T::EnginesPage, -1); });
    m_nextPage = button("Next >", [state]() { state->push(T::EnginesPage, 1); });
    m_openBuilder = button("Build your own engine", [state]() { state->push(T::OpenBuilder); });

    for (int i = 0; i < SimpleUiState::BuilderRows; ++i) {
        m_builderLess[i] = button("<", [state, i]() { state->push(T::BuilderAdjust, i, -1); });
        m_builderMore[i] = button(">", [state, i]() { state->push(T::BuilderAdjust, i, 1); });
    }

    m_build = button("Build and start", [state]() { state->push(T::BuildEngine); });
    m_build->m_active = true;
    m_cancel = button("Cancel", [state]() { state->push(T::CloseOverlay); });

    m_close->m_quiet = true;
    for (int i = 0; i < EngineRows; ++i) m_engineRows[i]->m_alignLeft = true;

    hideAll();
}

void SimpleOverlayPanel::hideAll() {
    for (UiElement *child : m_children) {
        child->setVisible(false);
    }
}

Bounds SimpleOverlayPanel::panelBounds() const {
    const float w = std::min(m_bounds.width() - 40.0f, 1100.0f);
    const float h = std::min(m_bounds.height() - 40.0f, 760.0f);
    return Bounds(w, h, m_bounds.getPosition(Bounds::center), Bounds::center);
}

void SimpleOverlayPanel::update(float dt) {
    if (m_state == nullptr) return;

    hideAll();

    const bool open = m_state->overlay != SimpleOverlay::None;
    setVisible(open);
    if (open) {
        const Bounds panel = panelBounds();
        m_close->setVisible(true);
        m_close->m_bounds = box(panel.right() - 130.0f, panel.top() - 12.0f, 118.0f, 40.0f);

        switch (m_state->overlay) {
        case SimpleOverlay::Help: layoutHelp(panel); break;
        case SimpleOverlay::Engines: layoutEngines(panel); break;
        case SimpleOverlay::Builder: layoutBuilder(panel); break;
        default: break;
        }
    }

    UiElement::update(dt);
}

void SimpleOverlayPanel::render() {
    if (m_state == nullptr || m_state->overlay == SimpleOverlay::None) return;

    // Cover everything behind the panel, then a rounded card
    const UiTheme &theme = m_app->getTheme();
    drawBox(m_bounds, theme.page);

    const Bounds panel = panelBounds();
    drawRoundedBox(panel, 22.0f, theme.card);

    switch (m_state->overlay) {
    case SimpleOverlay::Help: renderHelp(panel); break;
    case SimpleOverlay::Engines: renderEngines(panel); break;
    case SimpleOverlay::Builder: renderBuilder(panel); break;
    default: break;
    }

    UiElement::render();
}

// --- Help -------------------------------------------------------------------

void SimpleOverlayPanel::layoutHelp(const Bounds &panel) {
    (void)panel;
}

void SimpleOverlayPanel::renderHelp(const Bounds &panel) {
    const float left = panel.left() + 30.0f;
    float y = panel.top() - 30.0f;

    const UiTheme &theme = m_app->getTheme();
    drawAlignedText("How to use Engine Simulator", box(left, y, 700.0f, 40.0f), 34.0f, Bounds::tl, Bounds::tl);
    y -= 60.0f;

    const char *steps[] = {
        "1.  Hold Hold to start until the engine runs (the ignition turns on by itself).",
        "2.  Drag Throttle to rev the engine. Drag it back to the left to idle.",
        "3.  Use Gear + and Gear - to drive. N means neutral.",
        "4.  Engines lets you pick a different engine.",
        "5.  Build engine lets you design your own engine in a few clicks.",
        "6.  Dyno measures power and torque. View switches between simple and detailed screens.",
        "7.  Drag the engine picture to move it, scroll over it to zoom.",
        "8.  Sound studio holds one sound (idle, cruise, overrun, redline...) and records WAV files.",
        "9.  Light / Dark switches the colour theme."
    };
    for (const char *s : steps) {
        drawAlignedText(s, box(left, y, panel.width() - 60.0f, 28.0f), 20.0f, Bounds::tl, Bounds::tl);
        y -= 32.0f;
    }

    y -= 20.0f;
    drawAlignedText("Keyboard shortcuts", box(left, y, 600.0f, 32.0f), 26.0f, Bounds::tl, Bounds::tl);
    y -= 44.0f;

    const char *keys[][2] = {
        { "S (hold)", "Starter" },
        { "A", "Ignition on / off" },
        { "Q  W  E  R", "Throttle 1% / 10% / 20% / 100%" },
        { "Space + scroll", "Fine throttle" },
        { "Up / Down arrows", "Change gear" },
        { "Shift (hold)", "Clutch" },
        { "D", "Dyno on / off" },
        { "H, then G + scroll", "Hold RPM on the dyno" },
        { "Z + scroll", "Volume" },
        { "Tab", "Change view" },
        { "F", "Fullscreen" },
        { "F1", "Show this help" },
        { "Enter", "Reload the engine" },
        { "1 - 5 (hold)", "Slow motion" },
        { "Esc", "Close this screen / quit" }
    };

    const int perColumn = 8;
    const float columnWidth = (panel.width() - 60.0f) / 2.0f;
    const float top = y;
    for (int i = 0; i < (int)(sizeof(keys) / sizeof(keys[0])); ++i) {
        const int column = i / perColumn;
        const float cx = left + column * columnWidth;
        const float cy = top - (i % perColumn) * 30.0f;
        drawAlignedText(keys[i][0], box(cx, cy, 220.0f, 26.0f), 18.0f, Bounds::tl, Bounds::tl);
        drawTextColored(keys[i][1], box(cx + 230.0f, cy, columnWidth - 240.0f, 26.0f), 18.0f, Bounds::tl, Bounds::tl, theme.textMuted);
    }
}

// --- Engine picker ----------------------------------------------------------

void SimpleOverlayPanel::layoutEngines(const Bounds &panel) {
    const int total = (int)m_state->engines.size();
    const int pages = std::max(1, (total + EngineRows - 1) / EngineRows);
    m_state->enginesPage = std::max(0, std::min(pages - 1, m_state->enginesPage));

    const float left = panel.left() + 30.0f;
    const float width = panel.width() - 60.0f;
    const float rowHeight = 40.0f;
    const float top = panel.top() - 90.0f;

    for (int i = 0; i < EngineRows; ++i) {
        const int index = m_state->enginesPage * EngineRows + i;
        if (index >= total) continue;

        const EngineEntry &entry = m_state->engines[index];
        const bool current = entry.relativePath == m_state->currentEnginePath;

        const float rowTop = top - i * (rowHeight + 6.0f);
        const float editWidth = entry.custom ? 100.0f : 0.0f;

        m_engineRows[i]->setVisible(true);
        m_engineRows[i]->m_bounds = box(left, rowTop, width - editWidth - (entry.custom ? 8.0f : 0.0f), rowHeight);
        m_engineRows[i]->m_text = (entry.custom ? "My engine:  " : "") + entry.name + (current ? "   (running)" : "");
        m_engineRows[i]->m_active = current;
        m_engineRows[i]->m_fontSize = 19.0f;

        if (entry.custom) {
            m_editRows[i]->setVisible(true);
            m_editRows[i]->m_bounds = box(left + width - editWidth, rowTop, editWidth, rowHeight);
        }
    }

    const float bottom = panel.bottom() + 20.0f + 44.0f;
    m_prevPage->setVisible(pages > 1);
    m_nextPage->setVisible(pages > 1);
    m_prevPage->m_enabled = m_state->enginesPage > 0;
    m_nextPage->m_enabled = m_state->enginesPage < pages - 1;
    m_prevPage->m_bounds = box(left, bottom, 170.0f, 44.0f);
    m_nextPage->m_bounds = box(left + 180.0f, bottom, 170.0f, 44.0f);

    m_openBuilder->setVisible(true);
    m_openBuilder->m_bounds = box(panel.right() - 30.0f - 320.0f, bottom, 320.0f, 44.0f);
}

void SimpleOverlayPanel::renderEngines(const Bounds &panel) {
    const float left = panel.left() + 30.0f;
    drawAlignedText("Choose an engine", box(left, panel.top() - 30.0f, 600.0f, 40.0f), 34.0f, Bounds::tl, Bounds::tl);

    const int total = (int)m_state->engines.size();
    const int pages = std::max(1, (total + EngineRows - 1) / EngineRows);
    if (total == 0) {
        drawAlignedText(
            "No engines were found in the assets/engines folder.",
            box(left, panel.top() - 100.0f, panel.width() - 60.0f, 30.0f), 20.0f, Bounds::tl, Bounds::tl);
    }
    else if (pages > 1) {
        drawAlignedText(
            "Page " + std::to_string(m_state->enginesPage + 1) + " of " + std::to_string(pages),
            box(left + 370.0f, panel.bottom() + 64.0f, 200.0f, 44.0f), 18.0f, Bounds::lm, Bounds::lm);
    }
}

// --- Engine builder ---------------------------------------------------------

std::string SimpleOverlayPanel::builderRowLabel(int row) const {
    switch (row) {
    case 0: return "Layout";
    case 1: return "Cylinders";
    case 2: return "Size";
    case 3: return "Redline";
    case 4: return "Character";
    case 5: return "Camshaft";
    case 6: return "Exhaust";
    }

    return "";
}

std::string SimpleOverlayPanel::builderRowValue(int row) const {
    const EngineBuilderSettings &s = m_state->builder;
    switch (row) {
    case 0: return EngineBuilder::layoutName(s.layout);
    case 1: return std::to_string(s.cylinders);
    case 2: return fixed(s.displacementLiters, 1) + " L  (" + std::to_string((int)std::round(s.displacementLiters * 61.0237)) + " cu in)";
    case 3: return std::to_string(s.redlineRpm) + " rpm";
    case 4: return EngineBuilder::characterName(s.character);
    case 5: return EngineBuilder::camName(s.cam);
    case 6: return EngineBuilder::exhaustName(s.exhaust);
    }

    return "";
}

void SimpleOverlayPanel::layoutBuilder(const Bounds &panel) {
    const float left = panel.left() + 30.0f;
    const float top = panel.top() - 100.0f;
    const float rowHeight = 50.0f;
    const float labelWidth = 220.0f;
    const float valueWidth = 440.0f;

    for (int i = 0; i < SimpleUiState::BuilderRows; ++i) {
        const float rowTop = top - i * (rowHeight + 10.0f);
        m_builderLess[i]->setVisible(true);
        m_builderMore[i]->setVisible(true);
        m_builderLess[i]->m_bounds = box(left + labelWidth, rowTop, 56.0f, rowHeight);
        m_builderMore[i]->m_bounds = box(left + labelWidth + 66.0f + valueWidth, rowTop, 56.0f, rowHeight);
        m_builderLess[i]->m_fontSize = 24.0f;
        m_builderMore[i]->m_fontSize = 24.0f;
    }

    const float bottom = panel.bottom() + 20.0f + 50.0f;
    m_build->setVisible(true);
    m_cancel->setVisible(true);
    m_build->m_bounds = box(panel.right() - 30.0f - 260.0f, bottom, 260.0f, 50.0f);
    m_cancel->m_bounds = box(panel.right() - 30.0f - 260.0f - 10.0f - 160.0f, bottom, 160.0f, 50.0f);
    m_build->m_fontSize = 22.0f;
    m_cancel->m_fontSize = 22.0f;
}

void SimpleOverlayPanel::renderBuilder(const Bounds &panel) {
    const EngineBuilderSettings &s = m_state->builder;
    const float left = panel.left() + 30.0f;

    const UiTheme &theme = m_app->getTheme();
    drawAlignedText("Build your own engine", box(left, panel.top() - 30.0f, 700.0f, 40.0f), 34.0f, Bounds::tl, Bounds::tl);

    const float top = panel.top() - 100.0f;
    const float rowHeight = 50.0f;
    const float labelWidth = 220.0f;
    const float valueWidth = 440.0f;

    for (int i = 0; i < SimpleUiState::BuilderRows; ++i) {
        const float rowTop = top - i * (rowHeight + 10.0f);
        drawTextColored(builderRowLabel(i), box(left, rowTop, labelWidth, rowHeight), 22.0f, Bounds::lm, Bounds::lm, theme.textMuted);

        const Bounds valueBox = box(left + labelWidth + 61.0f, rowTop, valueWidth, rowHeight);
        drawRoundedBox(valueBox, 12.0f, theme.control);
        drawCenteredTextColored(builderRowValue(i), valueBox, 22.0f, theme.text);
    }

    // Summary
    double bore = 0, stroke = 0;
    EngineBuilder::boreStrokeMm(s, &bore, &stroke);

    std::string firing;
    for (int c : EngineBuilder::firingOrder(s.layout, s.cylinders)) {
        if (!firing.empty()) firing += "-";
        firing += std::to_string(c);
    }

    const float infoTop = top - SimpleUiState::BuilderRows * (rowHeight + 10.0f) - 10.0f;
    drawAlignedText(
        EngineBuilder::engineName(s) + "    Bore " + fixed(bore, 1) + " mm  x  Stroke " + fixed(stroke, 1) + " mm    Firing order " + firing,
        box(left, infoTop, panel.width() - 60.0f, 30.0f), 20.0f, Bounds::tl, Bounds::tl);
    drawTextColored(
        "Your engine is saved under Engines as \"My engine\", so you can play it again later.",
        box(left, infoTop - 36.0f, panel.width() - 60.0f, 30.0f), 18.0f, Bounds::tl, Bounds::tl, theme.textMuted);
}

// ============================================================================
// SimpleStudioPanel

SimpleStudioPanel::SimpleStudioPanel() {
    m_state = nullptr;
    for (int i = 0; i < ModeButtons; ++i) m_modes[i] = nullptr;
    m_rpmDown = m_rpmUp = m_record = m_openFolder = nullptr;
    m_target = m_load = nullptr;
}

SimpleStudioPanel::~SimpleStudioPanel() {
    /* void */
}

void SimpleStudioPanel::build(SimpleUiState *state) {
    m_state = state;
    using T = SimpleCommand::Type;

    auto button = [this](const std::string &text, std::function<void()> onClick) {
        SimpleButton *b = addElement<SimpleButton>();
        b->m_text = text;
        b->m_onClick = onClick;
        return b;
    };

    const UiTheme &theme = m_app->getTheme();

    for (int i = 0; i < ModeButtons; ++i) {
        const std::string name = SoundStudio::modeName((SoundStudio::Mode)i);
        m_modes[i] = button(name, [state, i]() { state->push(T::StudioMode, i); });
    }

    m_rpmDown = button("-100", [state]() { state->push(T::StudioRpmStep, -100); });
    m_rpmUp = button("+100", [state]() { state->push(T::StudioRpmStep, 100); });

    m_target = addElement<SimpleSlider>();
    m_target->m_label = "Target";
    m_target->m_value = &state->studioTargetNorm;
    m_target->m_color = theme.accent;
    m_target->m_format = [state](double) {
        return std::to_string((int)std::round(state->studioTargetRpm)) + " rpm";
    };

    m_load = addElement<SimpleSlider>();
    m_load->m_label = "Load";
    m_load->m_value = &state->studioLoad;
    m_load->m_color = theme.accent;

    m_record = button("Record", [state]() { state->push(T::ToggleRecord); });
    m_record->m_useActiveColor = true;
    m_record->m_activeColor = theme.danger;
    m_openFolder = button("Open folder", [state]() { state->push(T::OpenRecordings); });
}

void SimpleStudioPanel::update(float dt) {
    if (m_state == nullptr) return;

    const Bounds inner = m_bounds.inset(16.0f);
    const float left = inner.left();
    const float width = inner.width();
    const float gap = 8.0f;
    const float rowH = std::max(36.0f, std::min(52.0f, inner.height() / 17.0f));
    const float fontSize = std::max(14.0f, std::min(19.0f, rowH * 0.38f));
    float y = inner.top() - 44.0f; // below the title

    m_rpmBox = box(left, y, width, rowH * 2.2f);
    y -= rowH * 2.2f + gap * 2;

    m_modeLabel = box(left, y, width, 24.0f);
    y -= 28.0f;

    // Drive gets its own row, the six sound modes go in pairs
    const float half = (width - gap) / 2;
    m_modes[0]->m_bounds = box(left, y, width, rowH);
    y -= rowH + gap;
    for (int i = 1; i < ModeButtons; ++i) {
        const int column = (i - 1) % 2;
        m_modes[i]->m_bounds = box(left + column * (half + gap), y, half, rowH);
        if (column == 1) y -= rowH + gap;
    }
    y -= gap;

    const SoundStudio::Mode mode = (SoundStudio::Mode)m_state->studioMode;
    for (int i = 0; i < ModeButtons; ++i) {
        m_modes[i]->m_active = (i == m_state->studioMode);
        m_modes[i]->m_fontSize = fontSize;
    }

    m_targetLabel = box(left, y, width, 24.0f);
    y -= 28.0f;
    const float stepW = std::min(90.0f, width * 0.2f);
    m_rpmDown->m_bounds = box(left, y, stepW, rowH);
    m_target->m_bounds = box(left + stepW + gap, y, width - 2 * (stepW + gap), rowH);
    m_rpmUp->m_bounds = box(left + width - stepW, y, stepW, rowH);
    y -= rowH + gap * 2;

    const bool targetUsed = SoundStudio::usesTargetRpm(mode);
    m_rpmDown->m_enabled = targetUsed;
    m_rpmUp->m_enabled = targetUsed;

    m_loadLabel = box(left, y, width, 24.0f);
    y -= 28.0f;
    m_load->m_bounds = box(left, y, width, rowH);
    y -= rowH + gap * 2;

    m_record->m_bounds = box(left, y, width * 0.6f - gap / 2, rowH * 1.2f);
    m_openFolder->m_bounds = box(left + width * 0.6f + gap / 2, y, width * 0.4f - gap / 2, rowH * 1.2f);
    y -= rowH * 1.2f + gap * 2;

    m_record->m_active = m_state->recording;
    if (m_state->recording) {
        const int tenths = (int)(m_state->recordSeconds * 10);
        const int seconds = (tenths / 10) % 60;
        m_record->m_text = "Stop   " + std::to_string(tenths / 600) + ":"
            + (seconds < 10 ? "0" : "") + std::to_string(seconds)
            + "." + std::to_string(tenths % 10);
    }
    else {
        m_record->m_text = "Record";
    }

    for (SimpleButton *b : { m_rpmDown, m_rpmUp, m_record, m_openFolder }) b->m_fontSize = fontSize;
    m_target->m_fontSize = fontSize;
    m_load->m_fontSize = fontSize;

    m_infoBox = Bounds(width, std::max(1.0f, y - inner.bottom()), { left, y }, Bounds::tl);

    UiElement::update(dt);
}

void SimpleStudioPanel::render() {
    if (m_state == nullptr) return;

    const UiTheme &theme = m_app->getTheme();
    drawRoundedBox(m_bounds.inset(6.0f), 20.0f, theme.card);

    const Bounds inner = m_bounds.inset(16.0f);
    drawAlignedText("Sound studio", box(inner.left(), inner.top(), inner.width(), 32.0f), 28.0f, Bounds::tl, Bounds::tl);

    // Big rpm readout
    const SoundStudio::Mode mode = (SoundStudio::Mode)m_state->studioMode;
    const Bounds readout = m_rpmBox.inset(4.0f);
    const float big = std::min(84.0f, m_rpmBox.height() * 0.7f);
    const std::string rpmText = std::to_string((int)std::round(m_state->rpm));
    drawAlignedText(rpmText, readout, big, Bounds::lm, Bounds::lm);
    drawTextColored(
        "rpm",
        readout.inset(0.0f).move({ textWidth(rpmText, big) + 12.0f, -big * 0.18f }),
        22.0f, Bounds::lm, Bounds::lm, theme.textMuted);

    // Status pill: green when steady
    std::string status;
    ysVector statusColor = theme.control;
    ysVector statusText = theme.text;
    if (mode == SoundStudio::Mode::Off) {
        status = "Driving";
    }
    else if (m_state->studioSteady) {
        status = "Steady";
        statusColor = theme.success;
        statusText = theme.accentText;
    }
    else {
        status = "Settling";
        statusColor = theme.warning;
        statusText = ysColor::srgbiToLinear(0x171A20);
    }
    const float pillW = textWidth(status, 18.0f) + 32.0f;
    const Bounds pill(pillW, 34.0f, readout.getPosition(Bounds::tr), Bounds::tr);
    drawRoundedBox(pill, 17.0f, statusColor);
    drawCenteredTextColored(status, pill, 18.0f, statusText);

    std::string target;
    if (SoundStudio::usesTargetRpm(mode)) {
        target = "Target " + std::to_string((int)std::round(m_state->studioTargetRpm)) + " rpm";
    }
    else if (mode == SoundStudio::Mode::Redline || mode == SoundStudio::Mode::Limiter) {
        target = "Redline " + std::to_string((int)std::round(m_state->redlineRpm)) + " rpm";
    }
    if (!target.empty()) {
        drawTextColored(target, readout, 18.0f, Bounds::br, Bounds::br, theme.textMuted);
    }

    drawTextColored("Mode", m_modeLabel, 18.0f, Bounds::lm, Bounds::lm, theme.textMuted);
    drawTextColored(
        SoundStudio::usesTargetRpm(mode) ? "Target rpm" : "Target rpm (Cruise, Wide open, Overrun)",
        m_targetLabel, 18.0f, Bounds::lm, Bounds::lm, theme.textMuted);
    drawTextColored(
        mode == SoundStudio::Mode::Cruise ? "Load (throttle)" : "Load (Cruise only)",
        m_loadLabel, 18.0f, Bounds::lm, Bounds::lm, theme.textMuted);

    // Description, last file and a hint
    float y = m_infoBox.top();
    drawAlignedText(SoundStudio::modeDescription(mode), box(m_infoBox.left(), y, m_infoBox.width(), 24.0f), 16.0f, Bounds::tl, Bounds::tl);
    y -= 28.0f;
    if (!m_state->lastRecording.empty()) {
        drawTextColored("Saved: " + m_state->lastRecording, box(m_infoBox.left(), y, m_infoBox.width(), 24.0f), 16.0f, Bounds::tl, Bounds::tl, theme.success);
        y -= 28.0f;
    }
    drawTextColored("WAV files go in the recordings folder next to bin.", box(m_infoBox.left(), y, m_infoBox.width(), 24.0f), 16.0f, Bounds::tl, Bounds::tl, theme.textMuted);

    UiElement::render();
}

// ============================================================================
// SimpleDashboard

SimpleDashboard::SimpleDashboard() {
    m_state = nullptr;
}

SimpleDashboard::~SimpleDashboard() {
    /* void */
}

void SimpleDashboard::renderStat(
    const Bounds &b,
    const std::string &label,
    const std::string &value,
    const std::string &unit)
{
    const UiTheme &theme = m_app->getTheme();
    drawRoundedBox(b, 16.0f, theme.control);

    const Bounds inner = b.inset(16.0f);
    const float valueSize = std::min(40.0f, b.height() * 0.42f);
    drawTextColored(label, inner, 16.0f, Bounds::tl, Bounds::tl, theme.textMuted);
    drawAlignedText(value, inner, valueSize, Bounds::bl, Bounds::bl);
    if (!unit.empty()) {
        drawTextColored(
            unit,
            Bounds(inner).move({ textWidth(value, valueSize) + 8.0f, 0.0f }),
            16.0f, Bounds::bl, Bounds::bl, theme.textMuted);
    }
}

void SimpleDashboard::render() {
    if (m_state == nullptr) return;

    constexpr float Pi = 3.14159265f;
    const UiTheme &theme = m_app->getTheme();
    const Bounds card = m_bounds.inset(6.0f);
    drawRoundedBox(card, 20.0f, theme.card);

    const Bounds inner = card.inset(20.0f);
    const float w = inner.width();
    const float h = inner.height();

    // Engine name and status
    drawAlignedText(
        m_state->engineName.empty() ? "No engine" : m_state->engineName,
        box(inner.left(), inner.top(), w, 30.0f), 26.0f, Bounds::tl, Bounds::tl);
    std::string status = m_state->ignitionOn ? "Ignition on" : "Ignition off";
    if (m_state->dynoOn) status += "   |   Dyno on";
    drawTextColored(status, box(inner.left(), inner.top() - 36.0f, w, 22.0f), 17.0f, Bounds::tl, Bounds::tl, theme.textMuted);

    // Arc gauge: 270 degrees, clockwise from bottom-left
    const double redline = std::max(1000.0, m_state->redlineRpm);
    const double maxRpm = std::ceil(redline * 1.15 / 1000.0) * 1000.0;
    const float radius = std::max(40.0f, std::min(w * 0.40f, h * 0.24f));
    const Point center(inner.center_h(), inner.top() - 70.0f - radius);
    const float thickness = std::max(6.0f, radius * 0.07f);
    const float a0 = 1.25f * Pi;   // 225 degrees (start, 0 rpm)
    const float a1 = -0.25f * Pi;  // -45 degrees (end, max rpm)
    auto angleOf = [&](double rpm) {
        const double v = std::min(1.0, std::max(0.0, rpm / maxRpm));
        return (float)(a0 - v * (a0 - a1));
    };

    drawArc(center, radius - thickness, radius, a1, a0, theme.control);
    drawArc(center, radius - thickness, radius, a1, angleOf(redline), mix(theme.control, theme.danger, 0.55f));

    const double rpm = std::max(0.0, m_state->rpm);
    if (rpm > 1.0) {
        const ysVector valueColor = (rpm >= redline) ? theme.danger : theme.accent;
        drawArc(center, radius - thickness, radius, angleOf(rpm), a0, valueColor);
    }

    // Big number in the middle
    const float big = std::min(96.0f, radius * 0.62f);
    const std::string rpmText = std::to_string((int)std::round(rpm / 10.0) * 10);
    drawAlignedText(rpmText, Bounds(1.0f, 1.0f, center + Point(0.0f, big * 0.12f), Bounds::center), big, Bounds::center, Bounds::center);
    drawTextColored("rpm", Bounds(1.0f, 1.0f, center + Point(0.0f, -big * 0.55f), Bounds::center), 20.0f, Bounds::center, Bounds::center, theme.textMuted);

    // Gear row: N 1 2 3 4 5 6
    const int gears = std::max(1, std::min(10, m_state->gearCount));
    const float gearY = center.y - radius * 0.95f;
    const float gearStep = std::min(44.0f, w / (gears + 2));
    const float gearStart = inner.center_h() - gearStep * gears / 2.0f;
    for (int g = -1; g < gears; ++g) {
        const std::string label = (g < 0) ? "N" : std::to_string(g + 1);
        const bool current = (g == m_state->gear);
        const Point p(gearStart + (g + 1) * gearStep, gearY);
        if (current) {
            drawCircle(p, 19.0f, theme.accent);
        }
        drawTextColored(
            label, Bounds(1.0f, 1.0f, p, Bounds::center), 22.0f, Bounds::center, Bounds::center,
            current ? theme.accentText : theme.textMuted);
    }

    // Stat tiles, two by two, under the gauge
    const float tilesTop = gearY - 44.0f;
    const float tileGap = 12.0f;
    const float tileH = std::max(60.0f, std::min(110.0f, (tilesTop - inner.bottom() - tileGap) / 2));
    const float tileW = (w - tileGap) / 2;
    if (tilesTop - inner.bottom() > tileH) {
        std::ostringstream speed, throttle, power, torque;
        speed << (int)std::round(std::abs(m_state->speedMph));
        throttle << (int)std::round(m_state->throttleActual * 100.0);
        if (m_state->dynoOn) {
            power << (int)std::round(m_state->powerHp);
            torque << (int)std::round(m_state->torqueLbFt);
        }

        renderStat(box(inner.left(), tilesTop, tileW, tileH), "Speed", speed.str(), "mph");
        renderStat(box(inner.left() + tileW + tileGap, tilesTop, tileW, tileH), "Throttle", throttle.str(), "%");
        if (tilesTop - inner.bottom() > 2 * tileH + tileGap) {
            const float row2 = tilesTop - tileH - tileGap;
            renderStat(box(inner.left(), row2, tileW, tileH), "Power",
                m_state->dynoOn ? power.str() : "-", m_state->dynoOn ? "hp" : "dyno off");
            renderStat(box(inner.left() + tileW + tileGap, row2, tileW, tileH), "Torque",
                m_state->dynoOn ? torque.str() : "-", m_state->dynoOn ? "lb-ft" : "dyno off");
        }
    }
}
