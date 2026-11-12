#ifndef ATG_ENGINE_SIM_SIMPLE_UI_H
#define ATG_ENGINE_SIM_SIMPLE_UI_H

#include "ui_element.h"
#include "engine_builder.h"
#include "engine_library.h"

#include <functional>
#include <string>
#include <vector>

// Simple Edition: clickable controls, help screen, engine picker and engine
// builder screen.

enum class SimpleOverlay { None, Help, Engines, Builder };

struct SimpleCommand {
    enum class Type {
        ToggleIgnition,
        GearUp,
        GearDown,
        ToggleDyno,
        OpenHelp,
        OpenEngines,
        OpenBuilder,
        CloseOverlay,
        CycleView,
        LoadEngine,       // a = index into SimpleUiState::engines
        EditCustomEngine, // a = index into SimpleUiState::engines
        BuilderAdjust,    // a = row, b = +1 / -1
        BuildEngine,
        EnginesPage,      // a = +1 / -1
        ReloadEngine,
        ToggleStudio,
        StudioMode,       // a = SoundStudio::Mode
        StudioRpmStep,    // a = rpm change (+/-)
        ToggleRecord,
        OpenRecordings
    };

    Type type;
    int a = 0;
    int b = 0;
};

// Everything the panels need lives here, owned by the application, because
// the panels themselves are destroyed and re-created whenever an engine is
// loaded.
struct SimpleUiState {
    static constexpr int BuilderRows = 7;

    SimpleOverlay overlay = SimpleOverlay::None;
    std::vector<SimpleCommand> commands;

    // Written by the panels
    double throttle = 0.0;
    double volume = 1.0;
    bool volumeChanged = false;
    bool starterHeld = false;

    // Written by the application every frame
    bool ignitionOn = false;
    bool dynoOn = false;
    int gear = -1;
    double rpm = 0;
    double redlineRpm = 0;
    int viewMode = 2;
    std::string engineName;

    std::string message;
    bool messageIsError = false;
    float messageTimer = 0.0f;

    std::vector<EngineEntry> engines;
    int enginesPage = 0;
    std::string currentEnginePath;

    EngineBuilderSettings builder;

    // Sound Studio
    bool studioOpen = false;
    int studioMode = 0;            // SoundStudio::Mode
    double studioTargetNorm = 0.4; // target rpm slider, 0..1 between min and redline
    double studioTargetRpm = 3000; // written by the application
    double studioLoad = 0.3;       // cruise throttle, 0..1
    bool studioSteady = false;
    bool recording = false;
    double recordSeconds = 0;
    std::string lastRecording;

    void push(SimpleCommand::Type type, int a = 0, int b = 0) {
        SimpleCommand c;
        c.type = type;
        c.a = a;
        c.b = b;
        commands.push_back(c);
    }

    void showMessage(const std::string &text, bool error = false, float seconds = 5.0f) {
        message = text;
        messageIsError = error;
        messageTimer = seconds;
    }
};

class SimpleButton : public UiElement {
public:
    SimpleButton();
    virtual ~SimpleButton();

    virtual void update(float dt);
    virtual void render();
    virtual void onMouseClick(const Point &mouseLocal);

    std::function<void()> m_onClick;
    std::string m_text;
    float m_fontSize;
    bool m_active;
    bool m_enabled;
    ysVector m_activeColor;
    bool m_useActiveColor;
};

class SimpleSlider : public UiElement {
public:
    SimpleSlider();
    virtual ~SimpleSlider();

    virtual void update(float dt);
    virtual void render();
    virtual void onMouseDown(const Point &mouseLocal);
    virtual void onDrag(const Point &p0, const Point &mouse0, const Point &mouse);
    virtual void onMouseScroll(int scroll);

    double *m_value;
    bool *m_changed;
    std::string m_label;
    // Optional: turns the 0..1 value into the text shown (default: percent)
    std::function<std::string(double)> m_format;
    float m_fontSize;
    ysVector m_color;

protected:
    void setFromX(float x);
};

class SimpleControlBar : public UiElement {
public:
    SimpleControlBar();
    virtual ~SimpleControlBar();

    void build(SimpleUiState *state);

    virtual void update(float dt);
    virtual void render();

protected:
    SimpleUiState *m_state;

    SimpleButton *m_start;
    SimpleButton *m_ignition;
    SimpleButton *m_gearDown;
    SimpleButton *m_gearUp;
    SimpleSlider *m_throttle;
    SimpleButton *m_dyno;
    SimpleSlider *m_volume;
    SimpleButton *m_engines;
    SimpleButton *m_builder;
    SimpleButton *m_view;
    SimpleButton *m_help;
    SimpleButton *m_studio;

    Bounds m_gearBounds;
};

class SimpleStudioPanel : public UiElement {
public:
    static constexpr int ModeButtons = 7;

    SimpleStudioPanel();
    virtual ~SimpleStudioPanel();

    void build(SimpleUiState *state);

    virtual void update(float dt);
    virtual void render();

protected:
    SimpleUiState *m_state;

    SimpleButton *m_modes[ModeButtons];
    SimpleButton *m_rpmDown;
    SimpleButton *m_rpmUp;
    SimpleSlider *m_target;
    SimpleSlider *m_load;
    SimpleButton *m_record;
    SimpleButton *m_openFolder;

    // Where render() draws text, worked out in update()
    Bounds m_rpmBox;
    Bounds m_modeLabel;
    Bounds m_targetLabel;
    Bounds m_loadLabel;
    Bounds m_infoBox;
};

class SimpleMessageBanner : public UiElement {
public:
    SimpleMessageBanner();
    virtual ~SimpleMessageBanner();

    void setState(SimpleUiState *state) { m_state = state; }

    virtual void update(float dt);
    virtual void render();

protected:
    SimpleUiState *m_state;
};

class SimpleOverlayPanel : public UiElement {
public:
    SimpleOverlayPanel();
    virtual ~SimpleOverlayPanel();

    void build(SimpleUiState *state);

    virtual void update(float dt);
    virtual void render();

protected:
    Bounds panelBounds() const;
    void layoutHelp(const Bounds &panel);
    void layoutEngines(const Bounds &panel);
    void layoutBuilder(const Bounds &panel);
    void renderHelp(const Bounds &panel);
    void renderEngines(const Bounds &panel);
    void renderBuilder(const Bounds &panel);
    void hideAll();

    std::string builderRowLabel(int row) const;
    std::string builderRowValue(int row) const;

    static constexpr int EngineRows = 12;

    SimpleUiState *m_state;

    SimpleButton *m_close;

    // Engine picker
    SimpleButton *m_engineRows[EngineRows];
    SimpleButton *m_editRows[EngineRows];
    SimpleButton *m_prevPage;
    SimpleButton *m_nextPage;
    SimpleButton *m_openBuilder;

    // Builder
    SimpleButton *m_builderLess[SimpleUiState::BuilderRows];
    SimpleButton *m_builderMore[SimpleUiState::BuilderRows];
    SimpleButton *m_build;
    SimpleButton *m_cancel;
};

#endif /* ATG_ENGINE_SIM_SIMPLE_UI_H */
