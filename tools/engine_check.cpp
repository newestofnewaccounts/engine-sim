// Simple Edition: automated engine checker used by the cloud build.
//
// Run from the packaged "bin" folder:
//     engine-sim-check.exe [summary.md]
//
// 1. Compiles every engine that ships with the app.
// 2. Generates a range of engine-builder engines, then simulates each one:
//    starter, idle and full throttle, and checks that it starts, idles and
//    revs. Results go to stdout and to the optional markdown summary file.

#define SDL_MAIN_HANDLED

#include "../include/engine_builder.h"
#include "../include/engine_library.h"

#include "../include/engine.h"
#include "../include/exhaust_system.h"
#include "../include/impulse_response.h"
#include "../include/simulator.h"
#include "../include/transmission.h"
#include "../include/units.h"
#include "../include/vehicle.h"
#include "../include/delta.h"

#include "../scripting/include/compiler.h"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

#undef min
#undef max

namespace fs = std::filesystem;

namespace {
    struct LoadResult {
        Engine *engine = nullptr;
        Vehicle *vehicle = nullptr;
        Transmission *transmission = nullptr;
        std::string error;
    };

    LoadResult loadEngineFile(const std::string &assetsDir, const std::string &relativePath) {
        LoadResult result;

        std::string error;
        const std::string session = EngineLibrary::writeSessionScript(
            assetsDir, relativePath, EngineLibrary::DefaultTheme, &error);
        if (session.empty()) {
            result.error = error;
            return result;
        }

        *es_script::Compiler::output() = es_script::Compiler::Output();

        es_script::Compiler compiler;
        compiler.initialize((fs::path(assetsDir) / ".." / "es").string() + "/");
        const bool compiled = compiler.compile(session.c_str());
        if (compiled) {
            const es_script::Compiler::Output output = compiler.execute();
            result.engine = output.engine;
            result.vehicle = output.vehicle;
            result.transmission = output.transmission;
        }
        compiler.destroy();

        if (!compiled) {
            result.error = EngineLibrary::summarizeErrorLog("error_log.log");
        }
        else if (result.engine == nullptr) {
            result.error = "Script ran but did not create an engine";
        }

        return result;
    }

    struct SimResult {
        bool ok = false;
        double idleRpm = 0;
        double idleMin = 0;
        double idleMax = 0;
        double maxRpm = 0;
        double idleRms = 0;
        double fullRms = 0;
        double crankMax = 0;   // highest rpm while the starter is on
        double catchMax = 0;   // highest rpm in the seconds after the starter
        double wallSeconds = 0;
        std::string problem;
    };

    void loadImpulseResponses(Engine *engine, Simulator *simulator) {
        for (int i = 0; i < engine->getExhaustSystemCount(); ++i) {
            ImpulseResponse *response = engine->getExhaustSystem(i)->getImpulseResponse();

            ysWindowsAudioWaveFile waveFile;
            waveFile.OpenFile(response->getFilename().c_str());
            waveFile.InitializeInternalBuffer(waveFile.GetSampleCount());
            waveFile.FillBuffer(0);
            waveFile.CloseFile();

            simulator->synthesizer().initializeImpulseResponse(
                reinterpret_cast<const int16_t *>(waveFile.GetBuffer()),
                waveFile.GetSampleCount(),
                response->getVolume(),
                i
            );

            waveFile.DestroyInternalBuffer();
        }
    }

    SimResult simulate(const LoadResult &loaded) {
        SimResult r;
        const auto wall0 = std::chrono::steady_clock::now();

        Engine *engine = loaded.engine;
        Simulator *sim = engine->createSimulator(loaded.vehicle, loaded.transmission);
        engine->calculateDisplacement();
        sim->setSimulationFrequency((int)engine->getSimulationFrequency());
        loadImpulseResponses(engine, sim);
        sim->startAudioRenderingThread();

        engine->getIgnitionModule()->m_enabled = true;

        const double dt = 1 / 60.0;
        const double redlineRpm = units::toRpm(engine->getRedline());
        double simTime = 0;
        double idleSum = 0, idleSamples = 0;
        r.idleMin = 1e9;
        double idleSq = 0, idleSqN = 0, fullSq = 0, fullSqN = 0;

        std::vector<int16_t> audio(8192);
        int frames = 0;
        while (simTime < 10.0 && frames < 60 * 60) {
            ++frames;

            const bool starter = simTime < 3.0;
            const bool fullThrottle = simTime >= 7.0;
            sim->m_starterMotor.m_enabled = starter;
            engine->setSpeedControl(fullThrottle ? 1.0 : 0.0);
            loaded.transmission->setClutchPressure(1.0);

            sim->startFrame(dt);
            while (sim->simulateStep()) { /* void */ }
            simTime += sim->simulationSteps() * sim->getTimestep();
            sim->endFrame();

            const int n = sim->readAudioOutput((int)audio.size(), audio.data());

            const double rpm = engine->getRpm();
            if (!std::isfinite(rpm)) {
                r.problem = "engine speed became NaN";
                break;
            }

            if (starter) r.crankMax = std::max(r.crankMax, rpm);
            else if (simTime < 4.5) r.catchMax = std::max(r.catchMax, rpm);

            if (simTime >= 4.5 && simTime < 7.0) {
                idleSum += rpm;
                idleSamples += 1;
                r.idleMin = std::min(r.idleMin, rpm);
                r.idleMax = std::max(r.idleMax, rpm);
                for (int i = 0; i < n; ++i) { idleSq += (double)audio[i] * audio[i]; idleSqN += 1; }
            }
            else if (simTime >= 8.0) {
                r.maxRpm = std::max(r.maxRpm, rpm);
                for (int i = 0; i < n; ++i) { fullSq += (double)audio[i] * audio[i]; fullSqN += 1; }
            }

            // Don't spend forever on very slow configurations
            const double wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - wall0).count();
            if (wall > 90.0) {
                r.problem = "simulation too slow (stopped after 90 s)";
                break;
            }
        }

        sim->endAudioRenderingThread();
        r.wallSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - wall0).count();

        if (idleSamples > 0) r.idleRpm = idleSum / idleSamples;
        if (idleSqN > 0) r.idleRms = std::sqrt(idleSq / idleSqN);
        if (fullSqN > 0) r.fullRms = std::sqrt(fullSq / fullSqN);

        if (r.problem.empty()) {
            if (idleSamples == 0) r.problem = "did not reach the idle phase";
            else if (r.idleMin < 250) r.problem = "stalls at idle";
            else if (r.idleRpm > 2000) r.problem = "idle too high";
            else if (r.maxRpm < 0.75 * redlineRpm) r.problem = "does not rev out at full throttle";
        }

        r.ok = r.problem.empty();

        sim->releaseSimulation();
        delete sim;

        return r;
    }

    std::string fmt(double v, int p = 0) {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(p) << v;
        return ss.str();
    }
}

int main(int argc, char **argv) {
    const std::string summaryPath = (argc > 1) ? argv[1] : "";
    const std::string assetsDir = fs::absolute("../assets").string();

    std::stringstream report;
    int failures = 0;

    // 1. Built-in engines: compile + run the script
    report << "## Built-in engines\n\n| Engine | File | Result |\n|---|---|---|\n";
    const std::vector<EngineEntry> builtIn = EngineLibrary::scan(assetsDir);
    for (const EngineEntry &entry : builtIn) {
        if (entry.custom) continue;
        const LoadResult loaded = loadEngineFile(assetsDir, entry.relativePath);
        const bool ok = loaded.engine != nullptr;
        report << "| " << entry.name << " | `" << entry.relativePath << "` | "
            << (ok ? "loads" : "FAILED: " + loaded.error) << " |\n";
        std::cout << (ok ? "[ OK ] " : "[FAIL] ") << entry.relativePath << " " << loaded.error << std::endl;

        // Reference engines get a full simulation to calibrate the builder against
        if (ok && (entry.relativePath.find("07_gm_ls") != std::string::npos
            || entry.relativePath.find("03_2jz") != std::string::npos))
        {
            const SimResult sim = simulate(loaded);
            report << "| -> simulated | idle " << fmt(sim.idleRpm) << " rpm (" << fmt(sim.idleMin) << "-" << fmt(sim.idleMax)
                << "), max " << fmt(sim.maxRpm) << " rpm | audio rms idle " << fmt(sim.idleRms) << ", full " << fmt(sim.fullRms)
                << ", crank " << fmt(sim.crankMax) << ", after start " << fmt(sim.catchMax)
                << ", " << fmt(sim.wallSeconds, 1) << " s |\n";
        }
    }

    // 2. Engine builder
    using S = EngineBuilderSettings;
    std::vector<S> configs;
    for (S::Layout layout : { S::Layout::Inline, S::Layout::V, S::Layout::Flat }) {
        for (int cylinders : EngineBuilder::allowedCylinders(layout)) {
            S s;
            s.layout = layout;
            s.cylinders = cylinders;
            s.displacementLiters = std::min(6.0, std::max(0.4, 0.5 * cylinders));
            s.redlineRpm = 6500;
            configs.push_back(s);
        }
    }

    auto add = [&](S::Layout l, int c, double L, int rl, S::Character ch, S::Cam cam, S::Exhaust ex) {
        S s; s.layout = l; s.cylinders = c; s.displacementLiters = L; s.redlineRpm = rl;
        s.character = ch; s.cam = cam; s.exhaust = ex;
        configs.push_back(s);
    };
    add(S::Layout::Inline, 1, 0.2, 10000, S::Character::RevHappy, S::Cam::Race, S::Exhaust::Loud);
    add(S::Layout::Inline, 4, 2.0, 8000, S::Character::Torquey, S::Cam::Race, S::Exhaust::Loud);
    add(S::Layout::Inline, 4, 1.6, 9000, S::Character::RevHappy, S::Cam::Sport, S::Exhaust::Sporty);
    add(S::Layout::Inline, 6, 3.0, 7000, S::Character::Balanced, S::Cam::Mild, S::Exhaust::Quiet);
    add(S::Layout::V, 8, 7.0, 5000, S::Character::Torquey, S::Cam::Mild, S::Exhaust::Quiet);
    add(S::Layout::V, 8, 5.0, 12000, S::Character::RevHappy, S::Cam::Race, S::Exhaust::Loud);
    add(S::Layout::V, 12, 6.0, 7000, S::Character::Balanced, S::Cam::Mild, S::Exhaust::Sporty);
    add(S::Layout::Flat, 4, 2.5, 6500, S::Character::Balanced, S::Cam::Sport, S::Exhaust::Loud);

    // Small engines: start from the hardest case and change one setting at a
    // time, to show what stops a tiny single-cylinder from starting.
    add(S::Layout::Inline, 1, 0.2, 10000, S::Character::RevHappy, S::Cam::Sport, S::Exhaust::Loud);
    add(S::Layout::Inline, 1, 0.2, 6500, S::Character::RevHappy, S::Cam::Race, S::Exhaust::Loud);
    add(S::Layout::Inline, 1, 0.5, 10000, S::Character::RevHappy, S::Cam::Race, S::Exhaust::Loud);
    add(S::Layout::Inline, 1, 0.2, 10000, S::Character::Balanced, S::Cam::Race, S::Exhaust::Loud);
    add(S::Layout::Inline, 1, 0.2, 10000, S::Character::RevHappy, S::Cam::Race, S::Exhaust::Sporty);
    add(S::Layout::Inline, 1, 0.2, 6500, S::Character::Balanced, S::Cam::Sport, S::Exhaust::Sporty);
    add(S::Layout::Inline, 1, 0.1, 6500, S::Character::Balanced, S::Cam::Mild, S::Exhaust::Quiet);
    add(S::Layout::Inline, 2, 0.4, 10000, S::Character::RevHappy, S::Cam::Race, S::Exhaust::Loud);

    report << "\n## Engine builder\n\n| Engine | Settings | Result | Idle rpm (min-max) | Max rpm | Audio rms idle / full | Crank / after-start max rpm | Time |\n|---|---|---|---|---|---|---|---|\n";

    for (const S &config : configs) {
        S s = config;
        EngineBuilder::sanitize(&s);

        const std::string script = EngineBuilder::generateScript(s);
        std::string error;
        const std::string rel = EngineLibrary::saveCustomEngine(assetsDir, "check_" + EngineBuilder::fileStem(s), script, &error);

        const LoadResult loaded = loadEngineFile(assetsDir, rel);
        const std::string settings = EngineBuilder::serialize(s).substr(18);

        if (loaded.engine == nullptr) {
            ++failures;
            report << "| " << EngineBuilder::engineName(s) << " | " << settings << " | FAILED to load: " << loaded.error << " | | | | | |\n";
            std::cout << "[FAIL] " << EngineBuilder::engineName(s) << " load: " << loaded.error << std::endl;
            continue;
        }

        const SimResult sim = simulate(loaded);
        if (!sim.ok) ++failures;

        report << "| " << EngineBuilder::engineName(s) << " | " << settings << " | "
            << (sim.ok ? "OK" : "FAILED: " + sim.problem) << " | "
            << fmt(sim.idleRpm) << " (" << fmt(sim.idleMin) << "-" << fmt(sim.idleMax) << ") | "
            << fmt(sim.maxRpm) << " / " << s.redlineRpm << " | "
            << fmt(sim.idleRms) << " / " << fmt(sim.fullRms) << " | "
            << fmt(sim.crankMax) << " / " << fmt(sim.catchMax) << " | "
            << fmt(sim.wallSeconds, 1) << " s |\n";

        std::cout << (sim.ok ? "[ OK ] " : "[FAIL] ") << EngineBuilder::engineName(s) << " " << settings
            << " crank=" << fmt(sim.crankMax) << " catch=" << fmt(sim.catchMax)
            << " idle=" << fmt(sim.idleRpm) << " max=" << fmt(sim.maxRpm) << " " << sim.problem << std::endl;

        std::error_code ec;
        fs::remove(fs::path(assetsDir) / rel, ec);
    }

    report << "\n**Builder failures: " << failures << "**\n";

    // The generated sample of the default engine, for reading in the log
    std::cout << "\n----- Sample generated script (default settings) -----\n"
        << EngineBuilder::generateScript(S()) << std::endl;

    std::cout << report.str() << std::endl;
    if (!summaryPath.empty()) {
        std::ofstream summary(summaryPath, std::ios::app);
        summary << report.str();
    }

    std::error_code ec;
    fs::remove(fs::path(assetsDir) / EngineLibrary::SessionFile, ec);
    fs::remove(fs::path(assetsDir) / EngineLibrary::CustomFolder, ec);

    return failures == 0 ? 0 : 1;
}
