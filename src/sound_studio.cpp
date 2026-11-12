#include "../include/sound_studio.h"

#include "../include/engine.h"
#include "../include/simulator.h"
#include "../include/transmission.h"
#include "../include/units.h"

#include <algorithm>
#include <cmath>

#undef min
#undef max

SoundStudio::SoundStudio() {
    m_mode = Mode::Off;
    m_targetRpm = 3000.0;
    m_load = 0.3;
    m_throttle = 0.0;
    m_starterTime = 0.0;
    m_steadyTime = 0.0;
}

void SoundStudio::setMode(Mode mode) {
    if (mode != m_mode) {
        m_steadyTime = 0.0;
        m_starterTime = 0.0;
    }

    m_mode = mode;
}

bool SoundStudio::usesTargetRpm(Mode mode) {
    return mode == Mode::Cruise || mode == Mode::FullLoad || mode == Mode::Overrun;
}

double SoundStudio::effectiveTargetRpm(const Engine *engine) const {
    const double redline = (engine != nullptr) ? units::toRpm(engine->getRedline()) : 6000.0;

    switch (m_mode) {
    case Mode::Redline:
    case Mode::Limiter:
        return redline;
    case Mode::Cruise:
    case Mode::FullLoad:
    case Mode::Overrun:
        return std::max(minTargetRpm(), std::min(redline, m_targetRpm));
    default:
        return 0.0;
    }
}

void SoundStudio::release(Simulator *simulator) {
    m_throttle = 0.0;
    m_steadyTime = 0.0;
    m_starterTime = 0.0;

    if (simulator != nullptr) {
        simulator->m_dyno.m_enabled = false;
        simulator->m_dyno.m_hold = false;
        simulator->m_dyno.m_rotationSpeed = 0.0;
    }
}

void SoundStudio::apply(double dt, Simulator *simulator, Engine *engine, Transmission *transmission) {
    if (m_mode == Mode::Off || simulator == nullptr || engine == nullptr) return;

    const double rpm = engine->getRpm();
    const double redline = units::toRpm(engine->getRedline());

    engine->getIgnitionModule()->m_enabled = true;

    // Sounds are made with the gearbox in neutral so the car doesn't move
    if (transmission != nullptr && transmission->getGear() != -1) {
        transmission->changeGear(-1);
    }

    double targetThrottle = 0.0;
    bool useDyno = false;
    switch (m_mode) {
    case Mode::Idle: targetThrottle = 0.0; break;
    case Mode::Cruise: targetThrottle = std::max(0.0, std::min(1.0, m_load)); useDyno = true; break;
    case Mode::FullLoad: targetThrottle = 1.0; useDyno = true; break;
    case Mode::Overrun: targetThrottle = 0.0; useDyno = true; break;
    case Mode::Redline: targetThrottle = 1.0; useDyno = true; break;
    case Mode::Limiter: targetThrottle = 1.0; break;
    default: break;
    }

    // Smooth throttle changes so switching modes doesn't make a click
    m_throttle += (targetThrottle - m_throttle) * std::min(1.0, dt * 8.0);
    engine->setSpeedControl(m_throttle);

    simulator->m_dyno.m_enabled = useDyno;
    simulator->m_dyno.m_hold = useDyno;
    simulator->m_dyno.m_rotationSpeed = useDyno ? units::rpm(effectiveTargetRpm(engine)) : 0.0;

    // Modes without the dyno need the engine running on its own; crank it
    // for up to 4 seconds if it has stopped.
    if (!useDyno) {
        if (rpm < 250.0 && m_starterTime < 4.0) {
            simulator->m_starterMotor.m_enabled = true;
            m_starterTime += dt;
        }
        else if (rpm > 400.0) {
            m_starterTime = 0.0;
        }
    }

    // Steady detection
    bool steadyNow = false;
    if (useDyno) {
        const double target = effectiveTargetRpm(engine);
        steadyNow = std::abs(rpm - target) < std::max(30.0, target * 0.015);
    }
    else if (m_mode == Mode::Idle) {
        steadyNow = rpm > 350.0 && rpm < 2500.0;
    }
    else if (m_mode == Mode::Limiter) {
        steadyNow = rpm > redline * 0.9;
    }

    m_steadyTime = steadyNow ? m_steadyTime + dt : 0.0;
}

std::string SoundStudio::modeName(Mode mode) {
    switch (mode) {
    case Mode::Off: return "Drive";
    case Mode::Idle: return "Idle";
    case Mode::Cruise: return "Cruise";
    case Mode::FullLoad: return "Wide open";
    case Mode::Overrun: return "Overrun";
    case Mode::Redline: return "Redline";
    case Mode::Limiter: return "Rev limiter";
    }

    return "";
}

std::string SoundStudio::modeDescription(Mode mode) {
    switch (mode) {
    case Mode::Off: return "Normal driving. Pick a mode to hold the engine in one sound.";
    case Mode::Idle: return "Throttle closed, no load. The engine idles on its own.";
    case Mode::Cruise: return "Held at the target rpm with part throttle (set LOAD).";
    case Mode::FullLoad: return "Held at the target rpm with the throttle wide open.";
    case Mode::Overrun: return "Driven at the target rpm with the throttle closed (lift-off).";
    case Mode::Redline: return "Held at the redline with the throttle wide open.";
    case Mode::Limiter: return "Throttle wide open with no load, bouncing off the rev limiter.";
    }

    return "";
}
