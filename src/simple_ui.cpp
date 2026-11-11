#include "../include/simple_ui.h"

#include "../include/engine_sim_application.h"
#include "../include/ui_utilities.h"

#include <algorithm>
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
}

SimpleButton::~SimpleButton() {
    /* void */
}

void SimpleButton::update(float dt) {
    m_mouseBounds = m_bounds;
    UiElement::update(dt);
}

void SimpleButton::render() {
    const ysVector background = m_app->getBackgroundColor();
    const ysVector foreground = m_app->getForegroundColor();
    const ysVector accent = m_useActiveColor ? m_activeColor : m_app->getGreen();

    ysVector fill = background;
    if (m_active) {
        fill = mix(background, accent, 0.45f);
    }

    if (m_enabled) {
        if (isMouseHeld()) fill = mix(fill, foreground, 0.25f);
        else if (isMouseOver()) fill = mix(fill, foreground, 0.12f);
    }

    const ysVector frame = m_enabled ? foreground : mix(background, foreground, 0.35f);
    drawFrame(m_bounds, 1.0f, frame, fill);
    drawCenteredText(m_text, m_bounds, m_fontSize, Bounds::center);

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
    const ysVector background = m_app->getBackgroundColor();
    const ysVector foreground = m_app->getForegroundColor();
    const double value = (m_value != nullptr) ? *m_value : 0.0;

    ysVector frameFill = background;
    if (isMouseOver() || isMouseHeld()) frameFill = mix(background, foreground, 0.06f);
    drawFrame(m_bounds, 1.0f, foreground, frameFill);

    const Bounds inner = m_bounds.inset(4.0f);
    if (value > 0.001) {
        Bounds filled = inner;
        filled.m1.x = inner.m0.x + (float)(inner.width() * std::min(1.0, std::max(0.0, value)));
        drawBox(filled, mix(background, m_color, 0.6f));
    }

    drawCenteredText(m_label + " " + percent(value), m_bounds, m_fontSize, Bounds::center);
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
    m_engines = m_builder = m_view = m_help = nullptr;
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
    m_start = button("HOLD TO START", nullptr);
    m_start->m_useActiveColor = true;
    m_start->m_activeColor = m_app->getOrange();
    m_ignition = button("IGNITION", [state]() { state->push(T::ToggleIgnition); });
    m_gearDown = button("GEAR -", [state]() { state->push(T::GearDown); });
    m_gearUp = button("GEAR +", [state]() { state->push(T::GearUp); });

    m_throttle = addElement<SimpleSlider>();
    m_throttle->m_label = "THROTTLE";
    m_throttle->m_value = &state->throttle;
    m_throttle->m_color = m_app->getOrange();

    m_dyno = button("DYNO", [state]() { state->push(T::ToggleDyno); });
    m_dyno->m_useActiveColor = true;
    m_dyno->m_activeColor = m_app->getBlue();

    m_volume = addElement<SimpleSlider>();
    m_volume->m_label = "VOLUME";
    m_volume->m_value = &state->volume;
    m_volume->m_changed = &state->volumeChanged;
    m_volume->m_color = m_app->getBlue();

    m_engines = button("ENGINES", [state]() { state->push(T::OpenEngines); });
    m_builder = button("BUILD ENGINE", [state]() { state->push(T::OpenBuilder); });
    m_view = button("VIEW", [state]() { state->push(T::CycleView); });
    m_help = button("HELP", [state]() { state->push(T::OpenHelp); });
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
        { m_view, 1.4f },
        { m_help, 0.8f }
    };

    float totalWeight = 0;
    for (const Slot &s : slots) totalWeight += s.weight;

    const float padding = 6.0f;
    const Bounds inner = m_bounds.inset(padding);
    const float available = inner.width() - padding * (float)(std::size(slots) - 1);
    const float fontSize = std::max(12.0f, std::min(20.0f, inner.height() * 0.32f));

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

    for (SimpleButton *b : { m_start, m_ignition, m_gearDown, m_gearUp, m_dyno, m_engines, m_builder, m_view, m_help }) {
        b->m_fontSize = fontSize;
    }
    m_throttle->m_fontSize = fontSize;
    m_volume->m_fontSize = fontSize;

    m_state->starterHeld = m_start->isMouseHeld();
    m_start->m_active = m_state->starterHeld;
    m_start->m_text = m_state->starterHeld ? "STARTING..." : "HOLD TO START";

    m_ignition->m_active = m_state->ignitionOn;
    m_ignition->m_text = m_state->ignitionOn ? "IGNITION: ON" : "IGNITION: OFF";

    m_dyno->m_active = m_state->dynoOn;

    switch (m_state->viewMode) {
    case 0: m_view->m_text = "VIEW: DETAILED"; break;
    case 1: m_view->m_text = "VIEW: ENGINE"; break;
    default: m_view->m_text = "VIEW: SIMPLE"; break;
    }

    m_help->m_active = m_state->overlay == SimpleOverlay::Help;
    m_engines->m_active = m_state->overlay == SimpleOverlay::Engines;
    m_builder->m_active = m_state->overlay == SimpleOverlay::Builder;

    UiElement::update(dt);
}

void SimpleControlBar::render() {
    drawFrame(m_bounds, 1.0f, m_app->getForegroundColor(), m_app->getBackgroundColor());

    if (m_state != nullptr) {
        const std::string gear = (m_state->gear < 0) ? "N" : std::to_string(m_state->gear + 1);
        const float h = m_gearBounds.height();
        drawCenteredText("GEAR", m_gearBounds, std::max(10.0f, h * 0.2f), Bounds::tm);
        Bounds gearValue = m_gearBounds;
        gearValue.move({ 0.0f, -h * 0.12f });
        drawCenteredText(gear, gearValue, std::max(16.0f, h * 0.45f), Bounds::center);
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

    const float fontSize = 22.0f;
    const float width = std::min(
        m_bounds.width() - 20.0f,
        m_app->getTextRenderer()->CalculateWidth(m_state->message, fontSize) + 40.0f);
    const Bounds b(width, 44.0f, m_bounds.getPosition(Bounds::tm) + Point(0.0f, -10.0f), Bounds::tm);

    const ysVector bg = m_app->getBackgroundColor();
    const ysVector color = m_state->messageIsError ? m_app->getRed() : m_app->getForegroundColor();
    drawFrame(b, 2.0f, color, m_state->messageIsError ? mix(bg, m_app->getRed(), 0.3f) : bg);
    drawCenteredText(m_state->message, b, fontSize, Bounds::center);
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

    m_close = button("CLOSE", [state]() { state->push(T::CloseOverlay); });

    for (int i = 0; i < EngineRows; ++i) {
        m_engineRows[i] = button("", [state, i]() {
            state->push(T::LoadEngine, state->enginesPage * EngineRows + i);
        });
        m_editRows[i] = button("EDIT", [state, i]() {
            state->push(T::EditCustomEngine, state->enginesPage * EngineRows + i);
        });
    }

    m_prevPage = button("< PREVIOUS", [state]() { state->push(T::EnginesPage, -1); });
    m_nextPage = button("NEXT >", [state]() { state->push(T::EnginesPage, 1); });
    m_openBuilder = button("BUILD YOUR OWN ENGINE", [state]() { state->push(T::OpenBuilder); });

    for (int i = 0; i < SimpleUiState::BuilderRows; ++i) {
        m_builderLess[i] = button("<", [state, i]() { state->push(T::BuilderAdjust, i, -1); });
        m_builderMore[i] = button(">", [state, i]() { state->push(T::BuilderAdjust, i, 1); });
    }

    m_build = button("BUILD & START", [state]() { state->push(T::BuildEngine); });
    m_build->m_active = true;
    m_cancel = button("CANCEL", [state]() { state->push(T::CloseOverlay); });

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

    // Dim everything behind the panel
    drawBox(m_bounds, m_app->getBackgroundColor());

    const Bounds panel = panelBounds();
    drawFrame(panel, 2.0f, m_app->getForegroundColor(), m_app->getBackgroundColor());

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

    drawAlignedText("HOW TO USE ENGINE SIMULATOR", box(left, y, 600.0f, 40.0f), 34.0f, Bounds::tl, Bounds::tl);
    y -= 60.0f;

    const char *steps[] = {
        "1.  Hold HOLD TO START until the engine runs (ignition turns on by itself).",
        "2.  Drag THROTTLE to rev the engine. Drag it back to the left to idle.",
        "3.  Use GEAR + and GEAR - to drive. N means neutral.",
        "4.  ENGINES lets you pick a different engine.",
        "5.  BUILD ENGINE lets you design your own engine in a few clicks.",
        "6.  DYNO measures power and torque. VIEW switches simple / detailed screens.",
        "7.  Drag the engine picture to move it, scroll over it to zoom."
    };
    for (const char *s : steps) {
        drawAlignedText(s, box(left, y, panel.width() - 60.0f, 28.0f), 20.0f, Bounds::tl, Bounds::tl);
        y -= 32.0f;
    }

    y -= 20.0f;
    drawAlignedText("KEYBOARD SHORTCUTS", box(left, y, 600.0f, 32.0f), 26.0f, Bounds::tl, Bounds::tl);
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
        drawAlignedText(keys[i][1], box(cx + 230.0f, cy, columnWidth - 240.0f, 26.0f), 18.0f, Bounds::tl, Bounds::tl);
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
        m_engineRows[i]->m_text = (entry.custom ? "[MY ENGINE]  " : "") + entry.name + (current ? "   (running)" : "");
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
    drawAlignedText("CHOOSE AN ENGINE", box(left, panel.top() - 30.0f, 600.0f, 40.0f), 34.0f, Bounds::tl, Bounds::tl);

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

    drawAlignedText("BUILD YOUR OWN ENGINE", box(left, panel.top() - 30.0f, 700.0f, 40.0f), 34.0f, Bounds::tl, Bounds::tl);

    const float top = panel.top() - 100.0f;
    const float rowHeight = 50.0f;
    const float labelWidth = 220.0f;
    const float valueWidth = 440.0f;

    for (int i = 0; i < SimpleUiState::BuilderRows; ++i) {
        const float rowTop = top - i * (rowHeight + 10.0f);
        drawAlignedText(builderRowLabel(i), box(left, rowTop, labelWidth, rowHeight), 22.0f, Bounds::lm, Bounds::lm);

        const Bounds valueBox = box(left + labelWidth + 61.0f, rowTop, valueWidth, rowHeight);
        drawFrame(valueBox, 1.0f, mix(m_app->getBackgroundColor(), m_app->getForegroundColor(), 0.4f), m_app->getBackgroundColor());
        drawCenteredText(builderRowValue(i), valueBox, 22.0f, Bounds::center);
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
    drawAlignedText(
        "Your engine is saved under ENGINES as [MY ENGINE], so you can play it again later.",
        box(left, infoTop - 36.0f, panel.width() - 60.0f, 30.0f), 18.0f, Bounds::tl, Bounds::tl);
}
