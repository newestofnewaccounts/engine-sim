#ifndef ATG_ENGINE_SIM_SOUND_STUDIO_H
#define ATG_ENGINE_SIM_SOUND_STUDIO_H

#include <string>

class Engine;
class Simulator;
class Transmission;

// Simple Edition: holds the engine in a steady state so a specific sound can
// be heard and recorded (idle, cruise at an rpm, overrun, wide open, redline,
// bouncing off the rev limiter). Uses the dynamometer, which can both brake
// the engine (load) and drive it (overrun) at an exact speed.
class SoundStudio {
public:
    enum class Mode {
        Off,        // normal driving, nothing is overridden
        Idle,       // throttle closed, no load
        Cruise,     // held at the target rpm with part throttle
        FullLoad,   // held at the target rpm with the throttle wide open
        Overrun,    // driven at the target rpm with the throttle closed
        Redline,    // held at the redline with the throttle wide open
        Limiter     // no load, throttle wide open, bouncing off the limiter
    };

    static constexpr int ModeCount = 7;

    SoundStudio();

    void setMode(Mode mode);
    Mode getMode() const { return m_mode; }
    bool isActive() const { return m_mode != Mode::Off; }

    void setTargetRpm(double rpm) { m_targetRpm = rpm; }
    double getTargetRpm() const { return m_targetRpm; }

    void setLoad(double load) { m_load = load; }
    double getLoad() const { return m_load; }

    // Applies the mode to the simulation. Call once per frame after the
    // normal controls have been processed, so the mode wins.
    void apply(double dt, Simulator *simulator, Engine *engine, Transmission *transmission);

    // Releases the dynamometer etc. when leaving a mode.
    void release(Simulator *simulator);

    // The rpm the current mode is aiming for (redline for Redline, etc.)
    double effectiveTargetRpm(const Engine *engine) const;
    double minTargetRpm() const { return 600.0; }

    // True when the engine speed has been close to the target for a moment
    bool isSteady() const { return m_steadyTime > 0.75; }

    static std::string modeName(Mode mode);
    static std::string modeDescription(Mode mode);
    static bool usesTargetRpm(Mode mode);

private:
    Mode m_mode;
    double m_targetRpm;
    double m_load;
    double m_throttle;
    double m_starterTime;
    double m_steadyTime;
};

#endif /* ATG_ENGINE_SIM_SOUND_STUDIO_H */
