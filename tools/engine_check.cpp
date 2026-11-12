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
#include "../include/sound_studio.h"
#include "../include/wav_recorder.h"

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

    std::string fmt(double v, int p = 0);

    // Sound Studio: put the engine through every mode, record each to WAV,
    // and check it holds the right speed.
    int checkStudio(const LoadResult &loaded, const std::string &label, std::stringstream &report) {
        int failures = 0;
        Engine *engine = loaded.engine;
        Simulator *sim = engine->createSimulator(loaded.vehicle, loaded.transmission);
        engine->calculateDisplacement();
        sim->setSimulationFrequency((int)engine->getSimulationFrequency());
        loadImpulseResponses(engine, sim);
        sim->startAudioRenderingThread();

        const double redline = units::toRpm(engine->getRedline());
        const std::string folder = fs::absolute("../recordings_check").string();

        struct Step { SoundStudio::Mode mode; double target; double load; };
        const Step steps[] = {
            { SoundStudio::Mode::Idle, 0, 0 },
            { SoundStudio::Mode::Cruise, 3000, 0.3 },
            { SoundStudio::Mode::FullLoad, 4000, 1.0 },
            { SoundStudio::Mode::Overrun, 3000, 0.0 },
            { SoundStudio::Mode::Redline, 0, 0 },
            { SoundStudio::Mode::Limiter, 0, 0 },
            { SoundStudio::Mode::Idle, 0, 0 }
        };

        SoundStudio studio;
        std::vector<int16_t> audio(8192);
        const double dt = 1 / 60.0;

        report << "\n### Sound Studio: " << label << "\n\n| Mode | Target | Avg rpm (min-max) | Steady | WAV | Result |\n|---|---|---|---|---|---|\n";

        for (const Step &step : steps) {
            studio.setMode(step.mode);
            studio.setTargetRpm(step.target);
            studio.setLoad(step.load);

            WavRecorder recorder;
            const std::string name = label + " - " + SoundStudio::modeName(step.mode);
            recorder.start(WavRecorder::uniquePath(folder, name), 44100);

            const double duration = (step.mode == SoundStudio::Mode::Idle) ? 6.0 : 3.5;
            double t = 0, sum = 0, n = 0, lo = 1e9, hi = 0;
            int frames = 0;
            while (t < duration && frames < 60 * 30) {
                ++frames;
                sim->m_starterMotor.m_enabled = false;
                loaded.transmission->setClutchPressure(1.0);
                studio.apply(dt, sim, engine, loaded.transmission);

                sim->startFrame(dt);
                while (sim->simulateStep()) { /* void */ }
                t += sim->simulationSteps() * sim->getTimestep();
                sim->endFrame();

                const int count = sim->readAudioOutput((int)audio.size(), audio.data());
                recorder.addSamples(audio.data(), count);

                const double rpm = engine->getRpm();
                if (t > duration - 1.0) {
                    sum += rpm; n += 1;
                    lo = std::min(lo, rpm);
                    hi = std::max(hi, rpm);
                }
            }

            const double seconds = recorder.getSeconds();
            const std::string path = recorder.stop();
            std::error_code ec;
            const auto bytes = fs::file_size(path, ec);
            const bool wavOk = !ec && bytes == 44 + (uintmax_t)std::llround(seconds * 44100) * 2 && seconds > 0.5;

            const double avg = (n > 0) ? sum / n : 0;
            const double target = studio.effectiveTargetRpm(engine);
            std::string problem;
            switch (step.mode) {
            case SoundStudio::Mode::Idle:
                if (avg < 350 || avg > 2500) problem = "not idling";
                break;
            case SoundStudio::Mode::Limiter:
                if (hi < redline * 0.95) problem = "never reaches the limiter";
                break;
            default:
                if (std::abs(avg - target) > target * 0.03) problem = "does not hold the target rpm";
                break;
            }
            if (!wavOk) problem += (problem.empty() ? "" : ", ") + std::string("bad WAV file");

            if (!problem.empty()) ++failures;
            report << "| " << SoundStudio::modeName(step.mode) << " | " << fmt(target) << " | "
                << fmt(avg) << " (" << fmt(lo) << "-" << fmt(hi) << ") | " << (studio.isSteady() ? "yes" : "no") << " | "
                << fmt(seconds, 1) << " s, " << bytes << " bytes | " << (problem.empty() ? "OK" : "FAILED: " + problem) << " |\n";
            std::cout << (problem.empty() ? "[ OK ] " : "[FAIL] ") << "Studio " << label << " " << SoundStudio::modeName(step.mode)
                << " target=" << fmt(target) << " avg=" << fmt(avg) << " range=" << fmt(lo) << "-" << fmt(hi)
                << " wav=" << fmt(seconds, 1) << "s " << problem << std::endl;
        }

        studio.release(sim);
        sim->endAudioRenderingThread();
        sim->releaseSimulation();
        delete sim;

        std::error_code ec;
        fs::remove_all(folder, ec);
        return failures;
    }

    std::string fmt(double v, int p) {
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

    // 3. Sound Studio modes + WAV recording
    int studioFailures = 0;
    {
        const LoadResult ls = loadEngineFile(assetsDir, "engines/atg-video-2/07_gm_ls.mr");
        if (ls.engine != nullptr) studioFailures += checkStudio(ls, "GM LS", report);
        else { ++studioFailures; report << "\nCould not load GM LS for the Sound Studio check\n"; }

        S v8;
        v8.layout = S::Layout::V; v8.cylinders = 8; v8.displacementLiters = 5.0; v8.redlineRpm = 7000;
        const std::string rel = EngineLibrary::saveCustomEngine(assetsDir, "check_studio_v8", EngineBuilder::generateScript(v8), nullptr);
        const LoadResult custom = loadEngineFile(assetsDir, rel);
        if (custom.engine != nullptr) studioFailures += checkStudio(custom, "Custom V8", report);
        else { ++studioFailures; report << "\nCould not load the custom V8 for the Sound Studio check\n"; }
    }
    report << "\n**Sound Studio failures: " << studioFailures << "**\n";
    failures += studioFailures;

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
