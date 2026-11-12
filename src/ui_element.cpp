#include "../include/ui_element.h"

#include "../include/engine_sim_application.h"

#include <assert.h>

UiElement::UiElement() {
    m_app = nullptr;
    m_parent = nullptr;
    m_signalTarget = nullptr;
    m_checkMouse = false;
    m_disabled = false;
    m_index = -1;

    m_draggable = false;
    m_mouseOver = false;
    m_mouseHeld = false;
    m_visible = true;
}

UiElement::~UiElement() {
    /* void */
}

void UiElement::initialize(EngineSimApplication *app) {
    m_app = app;
}

void UiElement::destroy() {
    for (UiElement *child : m_children) {
        child->destroy();
        delete child;
    }

    m_children.clear();
}

void UiElement::update(float dt) {
    for (UiElement *child : m_children) {
        child->update(dt);
    }
}

void UiElement::render() {
    for (UiElement *child : m_children) {
        if (child->isVisible()) child->render();
    }
}

void UiElement::signal(UiElement *element, Event event) {
    /* void */
}

void UiElement::onMouseDown(const Point &mouseLocal) {
    m_mouseHeld = true;
}

void UiElement::onMouseUp(const Point &mouseLocal) {
    m_mouseHeld = false;
}

void UiElement::onMouseClick(const Point &mouseLocal) {
    signal(Event::Clicked);
}

void UiElement::onDrag(const Point &p0, const Point &mouse0, const Point &mouse) {
    if (m_draggable) {
        m_localPosition = p0 + (mouse - mouse0);
    }
}

void UiElement::onMouseOver(const Point &mouseLocal) {
    m_mouseOver = true;
}

void UiElement::onMouseLeave() {
    m_mouseOver = false;
}

void UiElement::onMouseScroll(int mouseScroll) {
    /* void */
}

UiElement *UiElement::mouseOver(const Point &mouseLocal) {
    // Hidden elements must not catch clicks meant for what is on screen
    if (m_disabled || !m_visible) return nullptr;

    const int n = (int)getChildCount();
    for (int i = n - 1; i >= 0; --i) {
        UiElement *child = m_children[i];
        UiElement *clickedElement = child->mouseOver(mouseLocal - child->m_localPosition);
        if (clickedElement != nullptr) {
            return clickedElement;
        }
    }

    return (m_checkMouse && m_mouseBounds.overlaps(mouseLocal))
        ? this
        : nullptr;
}

Point UiElement::getWorldPosition() const {
    return (m_parent != nullptr)
        ? m_parent->getWorldPosition() + m_localPosition
        : m_localPosition;
}

void UiElement::setLocalPosition(const Point &p, const Point &ref) {
    const Point current = m_bounds.getPosition(ref) + m_localPosition;
    m_localPosition += (p - current);
}

void UiElement::bringToFront(UiElement *element) {
    assert(element->m_parent == this);

    m_children.erase(m_children.begin() + element->m_index);
    m_children.push_back(element);

    int i = 0;
    for (UiElement *element : m_children) {
        element->m_index = i++;
    }
}

void UiElement::activate() {
    if (m_parent != nullptr) {
        m_parent->bringToFront(this);
        m_parent->activate();
    }
}

void UiElement::signal(Event event) {
    if (m_signalTarget == nullptr) return;

    m_signalTarget->signal(this, event);
}

float UiElement::pixelsToUnits(float length) const {
    return length;// m_app->pixelsToUnits(length);
}

Point UiElement::pixelsToUnits(const Point &p) const {
    return { pixelsToUnits(p.x), pixelsToUnits(p.y) };
}

float UiElement::unitsToPixels(float x) const {
    return x; // m_app->unitsToPixels(x);
}

Point UiElement::unitsToPixels(const Point &p) const {
    return { unitsToPixels(p.x), unitsToPixels(p.y) };
}

Point UiElement::getRenderPoint(const Point &p) const {
    const Point offset(
            -(float)m_app->getScreenWidth() / 2,
            -(float)m_app->getScreenHeight() / 2);
    const Point posPixels = localToWorld(p) + offset;

    return pixelsToUnits(posPixels);
}

Bounds UiElement::getRenderBounds(const Bounds &b) const {
    return { getRenderPoint(b.m0), getRenderPoint(b.m1) };
}

Bounds UiElement::unitsToPixels(const Bounds &b) const {
    return { unitsToPixels(b.m0), unitsToPixels(b.m1) };
}

void UiElement::resetShader() {
    m_app->getShaders()->ResetBaseColor();
    m_app->getShaders()->SetObjectTransform(ysMath::LoadIdentity());
}

void UiElement::drawModel(
    dbasic::ModelAsset *model,
    const ysVector &color,
    const Point &p,
    const Point &s)
{
    resetShader();

    const Point p_render = getRenderPoint(p);
    const Point s_render = pixelsToUnits(s);

    m_app->getShaders()->SetObjectTransform(
        ysMath::MatMult(
            ysMath::TranslationTransform(ysMath::LoadVector(p_render.x, p_render.y, 0.0)),
            ysMath::ScaleTransform(ysMath::LoadVector(s_render.x, s_render.y, 0.0))
        )
    );

    m_app->getShaders()->SetBaseColor(color);
    m_app->getEngine()->DrawModel(m_app->getShaders()->GetUiFlags(), model, 0x11);
}

void UiElement::drawFrame(
        const Bounds &bounds,
        float thickness,
        const ysVector &frameColor,
        const ysVector &fillColor,
        bool fill)
{
    GeometryGenerator *generator = m_app->getGeometryGenerator();

    const Bounds worldBounds = getRenderBounds(bounds);
    const Point position = worldBounds.getPosition(Bounds::center);

    GeometryGenerator::FrameParameters params;
    params.frameWidth = worldBounds.width();
    params.frameHeight = worldBounds.height();
    params.lineWidth = pixelsToUnits(thickness);
    params.x = position.x;
    params.y = position.y;

    GeometryGenerator::Line2dParameters lineParams;
    lineParams.lineWidth = worldBounds.height();
    lineParams.y0 = lineParams.y1 = worldBounds.getPosition(Bounds::center).y;
    lineParams.x0 = worldBounds.left();
    lineParams.x1 = worldBounds.right();

    GeometryGenerator::GeometryIndices frame, body;

    resetShader();
    if (fill) {
        generator->startShape();
        generator->generateLine2d(lineParams);
        generator->endShape(&body);

        m_app->getShaders()->SetBaseColor(fillColor);
        m_app->drawGenerated(body, 0x11, m_app->getShaders()->GetUiFlags());
    }

    generator->startShape();
    generator->generateFrame(params);
    generator->endShape(&frame);

    m_app->getShaders()->SetBaseColor(frameColor);
    m_app->drawGenerated(frame, 0x11, m_app->getShaders()->GetUiFlags());
}

void UiElement::drawBox(const Bounds &bounds, const ysVector &fillColor) {
    GeometryGenerator *generator = m_app->getGeometryGenerator();
    const Bounds worldBounds = getRenderBounds(bounds);

    GeometryGenerator::Line2dParameters lineParams;
    lineParams.lineWidth = worldBounds.height();
    lineParams.y0 = lineParams.y1 = worldBounds.getPosition(Bounds::center).y;
    lineParams.x0 = worldBounds.left();
    lineParams.x1 = worldBounds.right();

    GeometryGenerator::GeometryIndices body;
    generator->startShape();
    generator->generateLine2d(lineParams);
    generator->endShape(&body);

    resetShader();
    m_app->getShaders()->SetBaseColor(fillColor);
    m_app->drawGenerated(body, 0x11, m_app->getShaders()->GetUiFlags());
}

void UiElement::drawText(
        const std::string &s,
        const Bounds &bounds,
        float height,
        const Point &ref)
{
    const Bounds renderBounds = unitsToPixels(getRenderBounds(bounds));
    const Point origin = renderBounds.getPosition(ref);

    m_app->getTextRenderer()->RenderText(
            s, origin.x, origin.y - height / 4, height);
}

void UiElement::drawAlignedText(
        const std::string &s,
        const Bounds &bounds,
        float height,
        const Point &ref,
        const Point &refText)
{
    const Bounds renderBounds = unitsToPixels(getRenderBounds(bounds));
    const Point origin = renderBounds.getPosition(ref);

    const float textWidth = m_app->getTextRenderer()->CalculateWidth(s, height);
    const float textHeight = height;

    const Bounds textBounds(
        textWidth,
        textHeight,
        { 0.0f, textHeight - textHeight * 0.25f },
        Bounds::tl);
    const Point r = textBounds.getPosition(refText);

    m_app->getTextRenderer()->RenderText(
        s, origin.x - r.x, origin.y - r.y, height);
}

void UiElement::drawRoundedBox(const Bounds &bounds, float radius, const ysVector &color) {
    const Bounds world = getRenderBounds(bounds);
    const float w = world.width();
    const float h = world.height();
    if (w <= 0 || h <= 0) return;

    const float r = std::fmax(0.0f, std::fmin(radius, std::fmin(w, h) / 2));
    GeometryGenerator *generator = m_app->getGeometryGenerator();

    GeometryGenerator::GeometryIndices indices;
    generator->startShape();

    // Wide band (full width, height minus the corners)
    GeometryGenerator::Line2dParameters band;
    band.x0 = world.left();
    band.x1 = world.right();
    band.y0 = band.y1 = world.center_v();
    band.lineWidth = h - 2 * r;
    if (band.lineWidth > 0) generator->generateLine2d(band);

    // Tall band (full height, width minus the corners)
    band.x0 = world.left() + r;
    band.x1 = world.right() - r;
    band.lineWidth = h;
    if (band.x1 > band.x0) generator->generateLine2d(band);

    // Corners
    if (r > 0.5f) {
        GeometryGenerator::Circle2dParameters corner;
        corner.radius = r;
        corner.maxEdgeLength = std::fmax(1.0f, r * 0.3f);
        const float xs[2] = { world.left() + r, world.right() - r };
        const float ys[2] = { world.bottom() + r, world.top() - r };
        for (float x : xs) {
            for (float y : ys) {
                corner.center_x = x;
                corner.center_y = y;
                generator->generateCircle2d(corner);
            }
        }
    }

    generator->endShape(&indices);

    resetShader();
    m_app->getShaders()->SetBaseColor(color);
    m_app->drawGenerated(indices, 0x11, m_app->getShaders()->GetUiFlags());
}

void UiElement::drawCircle(const Point &center, float radius, const ysVector &color) {
    if (radius <= 0) return;

    const Point c = getRenderPoint(center);
    GeometryGenerator *generator = m_app->getGeometryGenerator();

    GeometryGenerator::Circle2dParameters params;
    params.center_x = c.x;
    params.center_y = c.y;
    params.radius = radius;
    params.maxEdgeLength = std::fmax(1.0f, radius * 0.2f);

    GeometryGenerator::GeometryIndices indices;
    generator->startShape();
    generator->generateCircle2d(params);
    generator->endShape(&indices);

    resetShader();
    m_app->getShaders()->SetBaseColor(color);
    m_app->drawGenerated(indices, 0x11, m_app->getShaders()->GetUiFlags());
}

void UiElement::drawArc(
    const Point &center,
    float innerRadius,
    float outerRadius,
    float startAngle,
    float endAngle,
    const ysVector &color)
{
    if (outerRadius <= innerRadius || endAngle <= startAngle) return;

    const Point c = getRenderPoint(center);
    GeometryGenerator *generator = m_app->getGeometryGenerator();

    GeometryGenerator::Ring2dParameters params;
    params.center_x = c.x;
    params.center_y = c.y;
    params.innerRadius = innerRadius;
    params.outerRadius = outerRadius;
    params.startAngle = startAngle;
    params.endAngle = endAngle;
    params.maxEdgeLength = std::fmax(1.0f, outerRadius * 0.05f);

    GeometryGenerator::GeometryIndices indices;
    generator->startShape();
    generator->generateRing2d(params);
    generator->endShape(&indices);

    resetShader();
    m_app->getShaders()->SetBaseColor(color);
    m_app->drawGenerated(indices, 0x11, m_app->getShaders()->GetUiFlags());
}

void UiElement::drawTextColored(
    const std::string &s,
    const Bounds &bounds,
    float height,
    const Point &ref,
    const Point &refText,
    const ysVector &color)
{
    dbasic::TextRenderer *text = m_app->getTextRenderer();
    text->SetColor(ysColor::linearToSrgb(color));
    drawAlignedText(s, bounds, height, ref, refText);
    text->SetColor(ysColor::linearToSrgb(m_app->getForegroundColor()));
}

void UiElement::drawCenteredTextColored(
    const std::string &s,
    const Bounds &bounds,
    float height,
    const ysVector &color)
{
    dbasic::TextRenderer *text = m_app->getTextRenderer();
    text->SetColor(ysColor::linearToSrgb(color));
    drawAlignedText(s, bounds, height, Bounds::center, Bounds::center);
    text->SetColor(ysColor::linearToSrgb(m_app->getForegroundColor()));
}

float UiElement::textWidth(const std::string &s, float height) const {
    return m_app->getTextRenderer()->CalculateWidth(s, height);
}

void UiElement::drawCenteredText(
        const std::string &s,
        const Bounds &bounds,
        float height,
        const Point &ref)
{
    const Bounds renderBounds = unitsToPixels(getRenderBounds(bounds));
    const Point origin = renderBounds.getPosition(ref);

    const float width = m_app->getTextRenderer()->CalculateWidth(s, height);
    m_app->getTextRenderer()->RenderText(
            s, origin.x - width / 2, origin.y - height / 4, height);
}
