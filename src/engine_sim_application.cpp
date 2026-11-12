#include "../include/engine_sim_application.h"

#include "../include/piston_object.h"
#include "../include/connecting_rod_object.h"
#include "../include/constants.h"
#include "../include/units.h"
#include "../include/crankshaft_object.h"
#include "../include/cylinder_bank_object.h"
#include "../include/cylinder_head_object.h"
#include "../include/ui_button.h"
#include "../include/combustion_chamber_object.h"
#include "../include/csv_io.h"
#include "../include/exhaust_system.h"
#include "../include/feedback_comb_filter.h"
#include "../include/utilities.h"
#include "../include/engine_builder.h"
#include "../include/engine_library.h"

#include "../scripting/include/compiler.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <stdlib.h>
#include <shellapi.h>
#include <sstream>

#undef min
#undef max

#if ATG_ENGINE_SIM_DISCORD_ENABLED
#include "../discord/Discord.h"
#endif

std::string EngineSimApplication::s_buildVersion = "0.1.12a";

EngineSimApplication::EngineSimApplication() {
    m_assetPath = "";

    m_geometryVertexBuffer = nullptr;
    m_geometryIndexBuffer = nullptr;

    m_paused = false;
    m_recording = false;
    m_screenResolutionIndex = 0;
    for (int i = 0; i < ScreenResolutionHistoryLength; ++i) {
        m_screenResolution[i][0] = m_screenResolution[i][1] = 0;
    }

    m_background = ysColor::srgbiToLinear(0x0E1012);
    m_foreground = ysColor::srgbiToLinear(0xFFFFFF);
    m_shadow = ysColor::srgbiToLinear(0x0E1012);
    m_highlight1 = ysColor::srgbiToLinear(0xEF4545);
    m_highlight2 = ysColor::srgbiToLinear(0xFFFFFF);
    m_pink = ysColor::srgbiToLinear(0xF394BE);
    m_red = ysColor::srgbiToLinear(0xEE4445);
    m_orange = ysColor::srgbiToLinear(0xF4802A);
    m_yellow = ysColor::srgbiToLinear(0xFDBD2E);
    m_blue = ysColor::srgbiToLinear(0x77CEE0);
    m_green = ysColor::srgbiToLinear(0xBDD869);

    m_displayHeight = (float)units::distance(2.0, units::foot);
    m_outputAudioBuffer = nullptr;
    m_audioSource = nullptr;

    m_torque = 0;
    m_dynoSpeed = 0;

    m_simulator = nullptr;
    m_engineView = nullptr;
    m_rightGaugeCluster = nullptr;
    m_temperatureGauge = nullptr;
    m_oscCluster = nullptr;
    m_performanceCluster = nullptr;
    m_loadSimulationCluster = nullptr;
    m_mixerCluster = nullptr;
    m_infoCluster = nullptr;
    m_iceEngine = nullptr;
    m_mainRenderTarget = nullptr;

    m_vehicle = nullptr;
    m_transmission = nullptr;

    m_oscillatorSampleOffset = 0;
    m_gameWindowHeight = 256;
    m_screenWidth = 256;
    m_screenHeight = 256;
    m_screen = 0;
    m_viewParameters.Layer0 = 0;
    m_viewParameters.Layer1 = 0;

    m_displayAngle = 0.0f;

    m_controlBar = nullptr;
    m_banner = nullptr;
    m_overlay = nullptr;
    m_studioPanel = nullptr;
}

EngineSimApplication::~EngineSimApplication() {
    /* void */
}

void EngineSimApplication::initialize(void *instance, ysContextObject::DeviceAPI api) {
    dbasic::Path modulePath = dbasic::GetModulePath();
    dbasic::Path confPath = modulePath.Append("delta.conf");

    std::string enginePath = "../dependencies/submodules/delta-studio/engines/basic";
    m_assetPath = "../assets";
    if (confPath.Exists()) {
        std::fstream confFile(confPath.ToString(), std::ios::in);

        std::getline(confFile, enginePath);
        std::getline(confFile, m_assetPath);
        enginePath = modulePath.Append(enginePath).ToString();
        m_assetPath = modulePath.Append(m_assetPath).ToString();

        confFile.close();
    }

    // Simple Edition: remembered choices (last engine, volume, view, ...)
    m_settings.load(modulePath.Append("simple_settings.ini").ToString());
    m_simple.volume = std::min(1.0, std::max(0.0, m_settings.getDouble("volume", 1.0)));
    m_simple.viewMode = std::min(2, std::max(0, m_settings.getInt("view", 2)));
    m_screen = m_simple.viewMode;
    m_simple.studioOpen = m_settings.getBool("studio_open", false);
    EngineBuilder::deserialize(m_settings.get("builder", ""), &m_simple.builder);
    if (!m_settings.getBool("help_seen", false)) {
        m_simple.overlay = SimpleOverlay::Help;
    }

    m_engine.GetConsole()->SetDefaultFontDirectory(enginePath + "/fonts/");

    const std::string shaderPath = enginePath + "/shaders/";
    const std::string winTitle = "Engine Simulator - Simple Edition | based on Engine Sim by AngeTheGreat | v" + s_buildVersion;
    dbasic::DeltaEngine::GameEngineSettings settings;
    settings.API = api;
    settings.DepthBuffer = false;
    settings.Instance = instance;
    settings.ShaderDirectory = shaderPath.c_str();
    settings.WindowTitle = winTitle.c_str();
    settings.WindowPositionX = 0;
    settings.WindowPositionY = 0;
    settings.WindowStyle = ysWindow::WindowStyle::Windowed;
    settings.WindowWidth = 1920;
    settings.WindowHeight = 1080;

    m_engine.CreateGameWindow(settings);

    m_engine.GetDevice()->CreateSubRenderTarget(
        &m_mainRenderTarget,
        m_engine.GetScreenRenderTarget(),
        0,
        0,
        0,
        0);

    m_engine.InitializeShaderSet(&m_shaderSet);
    m_shaders.Initialize(
        &m_shaderSet,
        m_mainRenderTarget,
        m_engine.GetScreenRenderTarget(),
        m_engine.GetDefaultShaderProgram(),
        m_engine.GetDefaultInputLayout());
    m_engine.InitializeConsoleShaders(&m_shaderSet);
    m_engine.SetShaderSet(&m_shaderSet);

    m_shaders.SetClearColor(ysColor::srgbiToLinear(0x34, 0x98, 0xdb));

    m_assetManager.SetEngine(&m_engine);

    m_engine.GetDevice()->CreateIndexBuffer(
        &m_geometryIndexBuffer, sizeof(unsigned short) * 200000, nullptr);
    m_engine.GetDevice()->CreateVertexBuffer(
        &m_geometryVertexBuffer, sizeof(dbasic::Vertex) * 100000, nullptr);

    m_geometryGenerator.initialize(100000, 200000);

    initialize();
}

void EngineSimApplication::initialize() {
    m_shaders.SetClearColor(ysColor::srgbiToLinear(0x34, 0x98, 0xdb));
    m_assetManager.CompileInterchangeFile((m_assetPath + "/assets").c_str(), 1.0f, true);
    m_assetManager.LoadSceneFile((m_assetPath + "/assets").c_str(), true);

    m_textRenderer.SetEngine(&m_engine);
    m_textRenderer.SetRenderer(m_engine.GetUiRenderer());
    m_textRenderer.SetFont(m_engine.GetConsole()->GetFont());

    loadStartupEngine();

    m_audioBuffer.initialize(44100, 44100);
    m_audioBuffer.m_writePointer = (int)(44100 * 0.1);

    ysAudioParameters params;
    params.m_bitsPerSample = 16;
    params.m_channelCount = 1;
    params.m_sampleRate = 44100;
    m_outputAudioBuffer =
        m_engine.GetAudioDevice()->CreateBuffer(&params, 44100);

    m_audioSource = m_engine.GetAudioDevice()->CreateSource(m_outputAudioBuffer);
    m_audioSource->SetMode((m_simulator != nullptr && m_simulator->getEngine() != nullptr)
        ? ysAudioSource::Mode::Loop
        : ysAudioSource::Mode::Stop);
    m_audioSource->SetPan(0.0f);
    m_audioSource->SetVolume(1.0f);

#ifdef ATG_ENGINE_SIM_DISCORD_ENABLED
    // Create a global instance of discord-rpc
    CDiscord::CreateInstance();

    // Enable it, this needs to be set via a config file of some sort. 
    GetDiscordManager()->SetUseDiscord(true);
    DiscordRichPresence passMe = { 0 };

    std::string engineName = (m_iceEngine != nullptr)
        ? m_iceEngine->getName()
        : "Broken Engine";

    GetDiscordManager()->SetStatus(passMe, engineName, s_buildVersion);
#endif /* ATG_ENGINE_SIM_DISCORD_ENABLED */
}

void EngineSimApplication::process(float frame_dt) {
    if (m_simulator == nullptr || m_iceEngine == nullptr) return;

    frame_dt = static_cast<float>(clamp(frame_dt, 1 / 200.0f, 1 / 30.0f));

    double speed = 1.0 / 1.0;
    if (m_engine.IsKeyDown(ysKey::Code::N1)) {
        speed = 1 / 10.0;
    }
    else if (m_engine.IsKeyDown(ysKey::Code::N2)) {
        speed = 1 / 100.0;
    }
    else if (m_engine.IsKeyDown(ysKey::Code::N3)) {
        speed = 1 / 200.0;
    }
    else if (m_engine.IsKeyDown(ysKey::Code::N4)) {
        speed = 1 / 500.0;
    }
    else if (m_engine.IsKeyDown(ysKey::Code::N5)) {
        speed = 1 / 1000.0;
    }

    // Simple Edition: F1 is now Help, so display rotation moved to F2-F4
    if (m_engine.IsKeyDown(ysKey::Code::F2)) {
        m_displayAngle += frame_dt * 1.0f;
    }
    else if (m_engine.IsKeyDown(ysKey::Code::F3)) {
        m_displayAngle -= frame_dt * 1.0f;
    }
    else if (m_engine.ProcessKeyDown(ysKey::Code::F4)) {
        m_displayAngle = 0.0f;
    }

    m_simulator->setSimulationSpeed(speed);

    const double avgFramerate = clamp(m_engine.GetAverageFramerate(), 30.0f, 1000.0f);
    m_simulator->startFrame(1 / avgFramerate);

    auto proc_t0 = std::chrono::steady_clock::now();
    const int iterationCount = m_simulator->getFrameIterationCount();
    while (m_simulator->simulateStep()) {
        m_oscCluster->sample();
    }

    auto proc_t1 = std::chrono::steady_clock::now();

    m_simulator->endFrame();

    auto duration = proc_t1 - proc_t0;
    if (iterationCount > 0) {
        m_performanceCluster->addTimePerTimestepSample(
            (duration.count() / 1E9) / iterationCount);
    }

    const SampleOffset safeWritePosition = m_audioSource->GetCurrentWritePosition();
    const SampleOffset writePosition = m_audioBuffer.m_writePointer;

    SampleOffset targetWritePosition =
        m_audioBuffer.getBufferIndex(safeWritePosition, (int)(44100 * 0.1));
    SampleOffset maxWrite = m_audioBuffer.offsetDelta(writePosition, targetWritePosition);

    SampleOffset currentLead = m_audioBuffer.offsetDelta(safeWritePosition, writePosition);
    SampleOffset newLead = m_audioBuffer.offsetDelta(safeWritePosition, targetWritePosition);

    if (currentLead > 44100 * 0.5) {
        m_audioBuffer.m_writePointer = m_audioBuffer.getBufferIndex(safeWritePosition, (int)(44100 * 0.05));
        currentLead = m_audioBuffer.offsetDelta(safeWritePosition, m_audioBuffer.m_writePointer);
        maxWrite = m_audioBuffer.offsetDelta(m_audioBuffer.m_writePointer, targetWritePosition);
    }

    if (currentLead > newLead) {
        maxWrite = 0;
    }

    int16_t *samples = new int16_t[maxWrite];
    const int readSamples = m_simulator->readAudioOutput(maxWrite, samples);

    for (SampleOffset i = 0; i < (SampleOffset)readSamples && i < maxWrite; ++i) {
        const int16_t sample = samples[i];
        if (m_oscillatorSampleOffset % 4 == 0) {
            m_oscCluster->getAudioWaveformOscilloscope()->addDataPoint(
                m_oscillatorSampleOffset,
                sample / (float)(INT16_MAX));
        }

        m_audioBuffer.writeSample(sample, m_audioBuffer.m_writePointer, (int)i);

        m_oscillatorSampleOffset = (m_oscillatorSampleOffset + 1) % (44100 / 10);
    }

    // Simple Edition: Sound Studio WAV recording gets exactly what is played
    if (m_recorder.isRecording() && readSamples > 0) {
        m_recorder.addSamples(samples, std::min(readSamples, (int)maxWrite));
    }

    delete[] samples;

    if (readSamples > 0) {
        SampleOffset size0, size1;
        void *data0, *data1;
        m_audioSource->LockBufferSegment(
            m_audioBuffer.m_writePointer, readSamples, &data0, &size0, &data1, &size1);

        m_audioBuffer.copyBuffer(
            reinterpret_cast<int16_t *>(data0), m_audioBuffer.m_writePointer, size0);
        m_audioBuffer.copyBuffer(
            reinterpret_cast<int16_t *>(data1),
            m_audioBuffer.getBufferIndex(m_audioBuffer.m_writePointer, size0),
            size1);

        m_audioSource->UnlockBufferSegments(data0, size0, data1, size1);
        m_audioBuffer.commitBlock(readSamples);
    }

    m_performanceCluster->addInputBufferUsageSample(
        (double)m_simulator->getSynthesizerInputLatency() / m_simulator->getSynthesizerInputLatencyTarget());
    m_performanceCluster->addAudioLatencySample(
        m_audioBuffer.offsetDelta(m_audioSource->GetCurrentWritePosition(), m_audioBuffer.m_writePointer) / (44100 * 0.1));
}

void EngineSimApplication::render() {
    // Simple Edition: the engine picture is hidden while a full screen
    // panel (help, engine list, builder) is open.
    if (!isOverlayOpen()) {
        for (SimulationObject *object : m_objects) {
            object->generateGeometry();
        }

        m_viewParameters.Sublayer = 0;
        for (SimulationObject *object : m_objects) {
            object->render(&getViewParameters());
        }

        m_viewParameters.Sublayer = 1;
        for (SimulationObject *object : m_objects) {
            object->render(&getViewParameters());
        }

        m_viewParameters.Sublayer = 2;
        for (SimulationObject *object : m_objects) {
            object->render(&getViewParameters());
        }
    }

    m_uiManager.render();
}

float EngineSimApplication::pixelsToUnits(float pixels) const {
    const float f = m_displayHeight / m_engineView->m_bounds.height();
    return pixels * f;
}

float EngineSimApplication::unitsToPixels(float units) const {
    const float f = m_engineView->m_bounds.height() / m_displayHeight;
    return units * f;
}

void EngineSimApplication::run() {
    while (true) {
        m_engine.StartFrame();

        if (!m_engine.IsOpen()) break;
        if (m_engine.ProcessKeyDown(ysKey::Code::Escape)) {
            // Simple Edition: Escape first closes an open panel
            if (isOverlayOpen()) {
                m_simple.push(SimpleCommand::Type::CloseOverlay);
            }
            else {
                break;
            }
        }

        if (m_engine.ProcessKeyDown(ysKey::Code::Return)) {
            reloadCurrentEngine();
        }

        if (m_engine.ProcessKeyDown(ysKey::Code::F1)) {
            m_simple.push(isOverlayOpen() && m_simple.overlay == SimpleOverlay::Help
                ? SimpleCommand::Type::CloseOverlay
                : SimpleCommand::Type::OpenHelp);
        }

        if (m_engine.ProcessKeyDown(ysKey::Code::Tab)) {
            m_simple.push(SimpleCommand::Type::CycleView);
        }

        if (m_engine.ProcessKeyDown(ysKey::Code::F)) {
            if (m_engine.GetGameWindow()->GetWindowStyle() != ysWindow::WindowStyle::Fullscreen) {
                m_engine.GetGameWindow()->SetWindowStyle(ysWindow::WindowStyle::Fullscreen);
                m_infoCluster->setLogMessage("Entered fullscreen mode");
            }
            else {
                m_engine.GetGameWindow()->SetWindowStyle(ysWindow::WindowStyle::Windowed);
                m_infoCluster->setLogMessage("Exited fullscreen mode");
            }
        }

        m_gameWindowHeight = m_engine.GetGameWindow()->GetGameHeight();
        m_screenHeight = m_engine.GetGameWindow()->GetScreenHeight();
        m_screenWidth = m_engine.GetGameWindow()->GetScreenWidth();

        updateScreenSizeStability();

        processEngineInput();

        if (m_engine.ProcessKeyDown(ysKey::Code::Insert) &&
            m_engine.GetGameWindow()->IsActive()) {
            if (!isRecording() && readyToRecord()) {
                startRecording();
            }
            else if (isRecording()) {
                stopRecording();
            }
        }

        if (isRecording() && !readyToRecord()) {
            stopRecording();
        }

        if (!m_paused || m_engine.ProcessKeyDown(ysKey::Code::Right)) {
            process(m_engine.GetFrameLength());
        }

        updateSimpleState();
        m_uiManager.update(m_engine.GetFrameLength());

        // Button clicks are queued and handled here, outside of the UI code,
        // because loading an engine rebuilds the whole UI.
        processSimpleCommands();

        renderScene();

        m_engine.EndFrame();

        if (isRecording()) {
            recordFrame();
        }
    }

    if (isRecording()) {
        stopRecording();
    }

    stopWavRecording(false);
    m_settings.save();

    if (m_simulator != nullptr) {
        m_simulator->endAudioRenderingThread();
    }
}

void EngineSimApplication::destroy() {
    m_shaderSet.Destroy();

    m_engine.GetDevice()->DestroyGPUBuffer(m_geometryVertexBuffer);
    m_engine.GetDevice()->DestroyGPUBuffer(m_geometryIndexBuffer);

    m_assetManager.Destroy();
    m_engine.Destroy();

    if (m_simulator != nullptr) {
        m_simulator->destroy();
    }
    m_audioBuffer.destroy();
}

void EngineSimApplication::loadEngine(
    Engine *engine,
    Vehicle *vehicle,
    Transmission *transmission)
{
    destroyObjects();

    if (m_simulator != nullptr) {
        m_simulator->releaseSimulation();
        delete m_simulator;
        m_simulator = nullptr;
    }

    if (m_vehicle != nullptr) {
        delete m_vehicle;
        m_vehicle = nullptr;
    }

    if (m_transmission != nullptr) {
        delete m_transmission;
        m_transmission = nullptr;
    }

    if (m_iceEngine != nullptr) {
        m_iceEngine->destroy();
        delete m_iceEngine;
    }

    m_iceEngine = engine;
    m_vehicle = vehicle;
    m_transmission = transmission;

    // Simple Edition: check for a missing engine *before* using it (the
    // original code crashed here when an engine script had a mistake).
    if (engine == nullptr || vehicle == nullptr || transmission == nullptr) {
        m_iceEngine = nullptr;
        m_simulator = nullptr;
        m_viewParameters.Layer1 = 0;

        return;
    }

    m_simulator = engine->createSimulator(vehicle, transmission);

    createObjects(engine);

    m_viewParameters.Layer1 = engine->getMaxDepth();
    engine->calculateDisplacement();

    m_simulator->setSimulationFrequency(engine->getSimulationFrequency());

    Synthesizer::AudioParameters audioParams = m_simulator->synthesizer().getAudioParameters();
    audioParams.inputSampleNoise = static_cast<float>(engine->getInitialJitter());
    audioParams.airNoise = static_cast<float>(engine->getInitialNoise());
    audioParams.dF_F_mix = static_cast<float>(engine->getInitialHighFrequencyGain());
    m_simulator->synthesizer().setAudioParameters(audioParams);

    for (int i = 0; i < engine->getExhaustSystemCount(); ++i) {
        ImpulseResponse *response = engine->getExhaustSystem(i)->getImpulseResponse();

        ysWindowsAudioWaveFile waveFile;
        waveFile.OpenFile(response->getFilename().c_str());
        waveFile.InitializeInternalBuffer(waveFile.GetSampleCount());
        waveFile.FillBuffer(0);
        waveFile.CloseFile();

        m_simulator->synthesizer().initializeImpulseResponse(
            reinterpret_cast<const int16_t *>(waveFile.GetBuffer()),
            waveFile.GetSampleCount(),
            response->getVolume(),
            i
        );

        waveFile.DestroyInternalBuffer();
    }

    m_simulator->startAudioRenderingThread();
}

void EngineSimApplication::drawGenerated(
    const GeometryGenerator::GeometryIndices &indices,
    int layer)
{
    drawGenerated(indices, layer, m_shaders.GetRegularFlags());
}

void EngineSimApplication::drawGeneratedUi(
    const GeometryGenerator::GeometryIndices &indices,
    int layer)
{
    drawGenerated(indices, layer, m_shaders.GetUiFlags());
}

void EngineSimApplication::drawGenerated(
    const GeometryGenerator::GeometryIndices &indices,
    int layer,
    dbasic::StageEnableFlags flags)
{
    m_engine.DrawGeneric(
        flags,
        m_geometryIndexBuffer,
        m_geometryVertexBuffer,
        sizeof(dbasic::Vertex),
        indices.BaseIndex,
        indices.BaseVertex,
        indices.FaceCount,
        false,
        layer);
}

void EngineSimApplication::configure(const ApplicationSettings &settings) {
    m_applicationSettings = settings;

    if (settings.startFullscreen) {
        m_engine.GetGameWindow()->SetWindowStyle(ysWindow::WindowStyle::Fullscreen);
    }

    m_background = ysColor::srgbiToLinear(m_applicationSettings.colorBackground);
    m_foreground = ysColor::srgbiToLinear(m_applicationSettings.colorForeground);
    m_shadow = ysColor::srgbiToLinear(m_applicationSettings.colorShadow);
    m_highlight1 = ysColor::srgbiToLinear(m_applicationSettings.colorHighlight1);
    m_highlight2 = ysColor::srgbiToLinear(m_applicationSettings.colorHighlight2);
    m_pink = ysColor::srgbiToLinear(m_applicationSettings.colorPink);
    m_red = ysColor::srgbiToLinear(m_applicationSettings.colorRed);
    m_orange = ysColor::srgbiToLinear(m_applicationSettings.colorOrange);
    m_yellow = ysColor::srgbiToLinear(m_applicationSettings.colorYellow);
    m_blue = ysColor::srgbiToLinear(m_applicationSettings.colorBlue);
    m_green = ysColor::srgbiToLinear(m_applicationSettings.colorGreen);
}

void EngineSimApplication::createObjects(Engine *engine) {
    for (int i = 0; i < engine->getCylinderCount(); ++i) {
        ConnectingRodObject *rodObject = new ConnectingRodObject;
        rodObject->initialize(this);
        rodObject->m_connectingRod = engine->getConnectingRod(i);
        m_objects.push_back(rodObject);

        PistonObject *pistonObject = new PistonObject;
        pistonObject->initialize(this);
        pistonObject->m_piston = engine->getPiston(i);
        m_objects.push_back(pistonObject);

        CombustionChamberObject *ccObject = new CombustionChamberObject;
        ccObject->initialize(this);
        ccObject->m_chamber = m_iceEngine->getChamber(i);
        m_objects.push_back(ccObject);
    }

    for (int i = 0; i < engine->getCrankshaftCount(); ++i) {
        CrankshaftObject *crankshaftObject = new CrankshaftObject;
        crankshaftObject->initialize(this);
        crankshaftObject->m_crankshaft = engine->getCrankshaft(i);
        m_objects.push_back(crankshaftObject);
    }

    for (int i = 0; i < engine->getCylinderBankCount(); ++i) {
        CylinderBankObject *cbObject = new CylinderBankObject;
        cbObject->initialize(this);
        cbObject->m_bank = engine->getCylinderBank(i);
        cbObject->m_head = engine->getHead(i);
        m_objects.push_back(cbObject);

        CylinderHeadObject *chObject = new CylinderHeadObject;
        chObject->initialize(this);
        chObject->m_head = engine->getHead(i);
        chObject->m_engine = engine;
        m_objects.push_back(chObject);
    }
}

void EngineSimApplication::destroyObjects() {
    for (SimulationObject *object : m_objects) {
        object->destroy();
        delete object;
    }

    m_objects.clear();
}

const SimulationObject::ViewParameters &
    EngineSimApplication::getViewParameters() const
{
    return m_viewParameters;
}

void EngineSimApplication::loadScript() {
    loadMainScript(true);
}

// ============================================================================
// Simple Edition: engine loading

bool EngineSimApplication::compileEngineScript(
    const std::string &path,
    Engine **engine,
    Vehicle **vehicle,
    Transmission **transmission,
    std::string *error)
{
    *engine = nullptr;
    *vehicle = nullptr;
    *transmission = nullptr;

#ifdef ATG_ENGINE_SIM_PIRANHA_ENABLED
    // The compiler keeps its output between runs; clear it so a broken
    // script can't pick up the previous engine.
    *es_script::Compiler::output() = es_script::Compiler::Output();

    es_script::Compiler compiler;
    compiler.initialize(m_assetPath + "/../es/");
    const bool compiled = compiler.compile(path.c_str());
    bool ok = false;
    if (compiled) {
        const es_script::Compiler::Output output = compiler.execute();
        if (output.engine != nullptr) {
            configure(output.applicationSettings);

            *engine = output.engine;
            *vehicle = output.vehicle;
            *transmission = output.transmission;
            ok = true;
        }
        else if (error != nullptr) {
            *error = "the script does not create an engine (it needs a 'main' that calls set_engine).";
        }
    }
    else if (error != nullptr) {
        *error = EngineLibrary::summarizeErrorLog("error_log.log");
    }

    compiler.destroy();
    return ok;
#else
    if (error != nullptr) *error = "scripting is disabled in this build.";
    return false;
#endif /* ATG_ENGINE_SIM_PIRANHA_ENABLED */
}

void EngineSimApplication::installEngine(Engine *engine, Vehicle *vehicle, Transmission *transmission) {
    if (m_audioSource != nullptr) {
        m_audioSource->SetMode(ysAudioSource::Mode::Stop);
    }

    // A recording is named after the engine, so finish it before switching
    stopWavRecording(true);

    ensureVehicleAndTransmission(&vehicle, &transmission);
    loadEngine(engine, vehicle, transmission);
    refreshUserInterface();
    applyVolume();

    m_simple.throttle = 0.0;
    m_targetSpeedSetting = 0.0;
    m_speedSetting = 0.0;

    if (m_audioSource != nullptr && m_simulator != nullptr && m_simulator->getEngine() != nullptr) {
        m_audioSource->SetMode(ysAudioSource::Mode::Loop);
    }
}

bool EngineSimApplication::loadEngineFromFile(const std::string &relativePath, bool showErrors) {
    std::string error;
    const std::string session = EngineLibrary::writeSessionScript(
        m_assetPath, relativePath, EngineLibrary::DefaultTheme, &error);
    if (session.empty()) {
        if (showErrors) m_simple.showMessage(error, true, 10.0f);
        return false;
    }

    Engine *engine = nullptr;
    Vehicle *vehicle = nullptr;
    Transmission *transmission = nullptr;
    if (!compileEngineScript(session, &engine, &vehicle, &transmission, &error)) {
        if (showErrors) {
            m_simple.showMessage("Could not load that engine: " + error, true, 12.0f);
        }
        return false;
    }

    installEngine(engine, vehicle, transmission);

    m_simple.currentEnginePath = relativePath;
    m_settings.set("last_engine", relativePath);
    m_settings.save();

    m_simple.showMessage("Loaded " + engine->getName() + ".  Hold START to start it.", false, 6.0f);
    return true;
}

bool EngineSimApplication::loadMainScript(bool showErrors) {
    Engine *engine = nullptr;
    Vehicle *vehicle = nullptr;
    Transmission *transmission = nullptr;
    std::string error;

    if (!compileEngineScript(m_assetPath + "/main.mr", &engine, &vehicle, &transmission, &error)) {
        if (showErrors) m_simple.showMessage("Could not load main.mr: " + error, true, 12.0f);
        return false;
    }

    installEngine(engine, vehicle, transmission);
    m_simple.currentEnginePath = "";
    return true;
}

void EngineSimApplication::loadStartupEngine() {
    const std::string last = m_settings.get("last_engine", "");
    if (!last.empty() && loadEngineFromFile(last, true)) return;
    if (loadMainScript(false)) return;

    // Last resort: build a default engine so there is always something to play with
    EngineBuilderSettings defaults;
    std::string error;
    const std::string relative = EngineLibrary::saveCustomEngine(
        m_assetPath, EngineBuilder::fileStem(defaults), EngineBuilder::generateScript(defaults), &error);
    if (!relative.empty() && loadEngineFromFile(relative, true)) return;

    // Nothing could be loaded; keep the app running with an empty screen
    loadEngine(nullptr, nullptr, nullptr);
    refreshUserInterface();
    m_simple.showMessage("No engine could be loaded. Check that the assets and es folders are next to bin.", true, 30.0f);
}

void EngineSimApplication::reloadCurrentEngine() {
    if (!m_simple.currentEnginePath.empty()) {
        loadEngineFromFile(m_simple.currentEnginePath, true);
    }
    else {
        loadMainScript(true);
    }
}

void EngineSimApplication::buildCustomEngine() {
    EngineBuilder::sanitize(&m_simple.builder);
    m_settings.set("builder", EngineBuilder::serialize(m_simple.builder));

    std::string error;
    const std::string relative = EngineLibrary::saveCustomEngine(
        m_assetPath,
        EngineBuilder::fileStem(m_simple.builder),
        EngineBuilder::generateScript(m_simple.builder),
        &error);

    if (relative.empty()) {
        m_simple.showMessage(error, true, 10.0f);
        return;
    }

    m_simple.overlay = SimpleOverlay::None;
    if (loadEngineFromFile(relative, true)) {
        m_simple.showMessage(
            "Built your " + EngineBuilder::engineName(m_simple.builder) + "!  Hold START to start it.", false, 8.0f);
    }
}

void EngineSimApplication::openEnginePicker() {
    m_simple.engines = EngineLibrary::scan(m_assetPath);
    m_simple.enginesPage = 0;
    for (size_t i = 0; i < m_simple.engines.size(); ++i) {
        if (m_simple.engines[i].relativePath == m_simple.currentEnginePath) {
            m_simple.enginesPage = (int)i / 12;
            break;
        }
    }

    m_simple.overlay = SimpleOverlay::Engines;
}

void EngineSimApplication::setViewMode(int mode) {
    m_screen = mode;
    m_simple.viewMode = mode;
    m_settings.setInt("view", mode);
    m_settings.save();
}

void EngineSimApplication::applyVolume() {
    if (m_simulator == nullptr) return;

    Synthesizer::AudioParameters audioParams = m_simulator->synthesizer().getAudioParameters();
    audioParams.volume = static_cast<float>(m_simple.volume);
    m_simulator->synthesizer().setAudioParameters(audioParams);
}

void EngineSimApplication::toggleIgnition() {
    if (m_simulator == nullptr || m_simulator->getEngine() == nullptr) return;

    IgnitionModule *ignition = m_simulator->getEngine()->getIgnitionModule();
    ignition->m_enabled = !ignition->m_enabled;
    m_infoCluster->setLogMessage(ignition->m_enabled ? "IGNITION ENABLED" : "IGNITION DISABLED");
}

void EngineSimApplication::toggleDyno() {
    if (m_simulator == nullptr) return;

    m_simulator->m_dyno.m_enabled = !m_simulator->m_dyno.m_enabled;
    m_infoCluster->setLogMessage(m_simulator->m_dyno.m_enabled ? "DYNOMOMETER ENABLED" : "DYNOMOMETER DISABLED");
}

void EngineSimApplication::shiftUp() {
    if (m_simulator == nullptr) return;

    m_simulator->getTransmission()->changeGear(m_simulator->getTransmission()->getGear() + 1);
    m_infoCluster->setLogMessage(
        "UPSHIFTED TO " + std::to_string(m_simulator->getTransmission()->getGear() + 1));
}

void EngineSimApplication::shiftDown() {
    if (m_simulator == nullptr) return;

    m_simulator->getTransmission()->changeGear(m_simulator->getTransmission()->getGear() - 1);
    if (m_simulator->getTransmission()->getGear() != -1) {
        m_infoCluster->setLogMessage(
            "DOWNSHIFTED TO " + std::to_string(m_simulator->getTransmission()->getGear() + 1));
    }
    else {
        m_infoCluster->setLogMessage("SHIFTED TO NEUTRAL");
    }
}

void EngineSimApplication::updateSimpleState() {
    const bool hasEngine = m_simulator != nullptr && m_iceEngine != nullptr;

    m_simple.ignitionOn = hasEngine && m_iceEngine->getIgnitionModule()->m_enabled;
    m_simple.dynoOn = hasEngine && m_simulator->m_dyno.m_enabled;
    m_simple.gear = hasEngine ? m_simulator->getTransmission()->getGear() : -1;
    m_simple.rpm = hasEngine ? m_iceEngine->getRpm() : 0.0;
    m_simple.redlineRpm = hasEngine ? units::toRpm(m_iceEngine->getRedline()) : 0.0;
    m_simple.engineName = hasEngine ? m_iceEngine->getName() : "";
    m_simple.viewMode = m_screen;

    // Sound Studio: turn the slider position into a target rpm (steps of 50)
    {
        const double redline = (m_simple.redlineRpm > 0) ? m_simple.redlineRpm : 7000.0;
        const double lo = m_studio.minTargetRpm();
        const double norm = std::max(0.0, std::min(1.0, m_simple.studioTargetNorm));
        m_simple.studioTargetRpm = std::round((lo + norm * (redline - lo)) / 50.0) * 50.0;
        m_simple.studioTargetRpm = std::min(m_simple.studioTargetRpm, redline);
        m_studio.setTargetRpm(m_simple.studioTargetRpm);
        m_studio.setLoad(m_simple.studioLoad);
        m_simple.studioMode = (int)m_studio.getMode();
        m_simple.studioSteady = m_studio.isSteady();
        m_simple.recording = m_recorder.isRecording();
        m_simple.recordSeconds = m_recorder.getSeconds();
    }

    // In the simple view the info panel is hidden, so show its messages in
    // the banner instead (except the constant throttle updates).
    if (m_infoCluster != nullptr) {
        const std::string log = m_infoCluster->getLogMessage();
        if (log != m_lastInfoMessage) {
            m_lastInfoMessage = log;
            if (m_screen != 0 && log.rfind("Throttle", 0) != 0) {
                m_simple.showMessage(log, false, 2.5f);
            }
        }
    }
}

std::string EngineSimApplication::recordingsFolder() const {
    return (std::filesystem::path(m_assetPath) / ".." / "recordings").lexically_normal().string();
}

void EngineSimApplication::setStudioMode(SoundStudio::Mode mode) {
    if (mode == SoundStudio::Mode::Off) {
        m_studio.release(m_simulator);
        m_dynoSpeed = 0;
        if (m_simulator != nullptr) {
            m_simulator->m_dyno.m_enabled = false;
            m_simulator->m_dyno.m_hold = false;
        }
    }
    else {
        // Make sure the Sound Studio panel is showing
        m_simple.studioOpen = true;
        m_simple.throttle = 0.0;
    }

    m_studio.setMode(mode);
    m_simple.studioMode = (int)mode;
}

void EngineSimApplication::toggleWavRecording() {
    if (m_recorder.isRecording()) {
        stopWavRecording(true);
        return;
    }

    if (m_iceEngine == nullptr) {
        m_simple.showMessage("Load an engine before recording.", true, 4.0f);
        return;
    }

    // e.g. "GM LS - Overrun - 3000rpm"
    const SoundStudio::Mode mode = m_studio.getMode();
    std::string name = m_iceEngine->getName() + " - " + SoundStudio::modeName(mode);
    if (SoundStudio::usesTargetRpm(mode)) {
        name += " - " + std::to_string((int)std::round(m_simple.studioTargetRpm)) + "rpm";
        if (mode == SoundStudio::Mode::Cruise) {
            name += " - load " + std::to_string((int)std::round(m_simple.studioLoad * 100)) + "pct";
        }
    }
    else if (mode == SoundStudio::Mode::Redline || mode == SoundStudio::Mode::Limiter) {
        name += " - " + std::to_string((int)std::round(m_simple.redlineRpm)) + "rpm";
    }

    const std::string path = WavRecorder::uniquePath(recordingsFolder(), name);
    if (m_recorder.start(path, 44100)) {
        m_simple.showMessage("Recording...  press STOP when you have enough.", false, 3.0f);
    }
    else {
        m_simple.showMessage("Could not create " + path, true, 8.0f);
    }
}

void EngineSimApplication::stopWavRecording(bool announce) {
    if (!m_recorder.isRecording()) return;

    const double seconds = m_recorder.getSeconds();
    const std::string path = m_recorder.stop();
    m_simple.lastRecording = std::filesystem::path(path).filename().string();
    m_simple.recording = false;

    if (announce) {
        std::ostringstream ss;
        ss << "Saved " << m_simple.lastRecording << " (" << std::fixed << std::setprecision(1) << seconds << " s)";
        m_simple.showMessage(ss.str(), false, 6.0f);
    }
}

void EngineSimApplication::processSimpleCommands() {
    std::vector<SimpleCommand> commands;
    commands.swap(m_simple.commands);

    using T = SimpleCommand::Type;
    for (const SimpleCommand &command : commands) {
        switch (command.type) {
        case T::ToggleIgnition: toggleIgnition(); break;
        case T::GearUp: shiftUp(); break;
        case T::GearDown: shiftDown(); break;
        case T::ToggleDyno: toggleDyno(); break;
        case T::OpenHelp: m_simple.overlay = SimpleOverlay::Help; break;
        case T::OpenEngines: openEnginePicker(); break;
        case T::OpenBuilder: m_simple.overlay = SimpleOverlay::Builder; break;
        case T::CloseOverlay:
            if (m_simple.overlay == SimpleOverlay::Help) {
                m_settings.setBool("help_seen", true);
                m_settings.save();
            }
            m_simple.overlay = SimpleOverlay::None;
            break;
        case T::CycleView:
            // Simple -> Detailed -> Engine only -> Simple
            if (m_screen == 2) setViewMode(0);
            else if (m_screen == 0) setViewMode(1);
            else setViewMode(2);
            break;
        case T::LoadEngine:
            if (command.a >= 0 && command.a < (int)m_simple.engines.size()) {
                m_simple.overlay = SimpleOverlay::None;
                loadEngineFromFile(m_simple.engines[command.a].relativePath, true);
            }
            break;
        case T::EditCustomEngine:
            if (command.a >= 0 && command.a < (int)m_simple.engines.size()) {
                const std::string path = m_assetPath + "/" + m_simple.engines[command.a].relativePath;
                if (EngineBuilder::deserialize(EngineLibrary::firstLine(path), &m_simple.builder)) {
                    m_simple.overlay = SimpleOverlay::Builder;
                }
                else {
                    m_simple.showMessage("That engine was not made with the builder, so it can't be edited here.", true, 6.0f);
                }
            }
            break;
        case T::BuilderAdjust:
            EngineBuilder::adjust(&m_simple.builder, command.a, command.b);
            m_settings.set("builder", EngineBuilder::serialize(m_simple.builder));
            break;
        case T::BuildEngine: buildCustomEngine(); break;
        case T::EnginesPage: m_simple.enginesPage += command.a; break;
        case T::ReloadEngine: reloadCurrentEngine(); break;
        case T::ToggleStudio:
            m_simple.studioOpen = !m_simple.studioOpen;
            if (!m_simple.studioOpen) {
                stopWavRecording(true);
                setStudioMode(SoundStudio::Mode::Off);
            }
            m_settings.setBool("studio_open", m_simple.studioOpen);
            m_settings.save();
            break;
        case T::StudioMode:
            if (command.a >= 0 && command.a < SoundStudio::ModeCount) {
                setStudioMode((SoundStudio::Mode)command.a);
            }
            break;
        case T::StudioRpmStep:
        {
            const double redline = m_simple.redlineRpm > 0 ? m_simple.redlineRpm : 7000.0;
            const double lo = m_studio.minTargetRpm();
            const double rpm = std::max(lo, std::min(redline, m_simple.studioTargetRpm + command.a));
            m_simple.studioTargetNorm = (redline > lo) ? (rpm - lo) / (redline - lo) : 0.0;
            break;
        }
        case T::ToggleRecord: toggleWavRecording(); break;
        case T::OpenRecordings:
        {
            const std::string folder = recordingsFolder();
            std::error_code ec;
            std::filesystem::create_directories(folder, ec);
            ShellExecuteA(NULL, "open", folder.c_str(), NULL, NULL, SW_SHOWNORMAL);
            break;
        }
        }
    }
}

void EngineSimApplication::ensureVehicleAndTransmission(Vehicle **vehicleOut, Transmission **transmissionOut) {
    Vehicle *vehicle = *vehicleOut;
    Transmission *transmission = *transmissionOut;

    if (vehicle == nullptr) {
        Vehicle::Parameters vehParams;
        vehParams.mass = units::mass(1597, units::kg);
        vehParams.diffRatio = 3.42;
        vehParams.tireRadius = units::distance(10, units::inch);
        vehParams.dragCoefficient = 0.25;
        vehParams.crossSectionArea = units::distance(6.0, units::foot) * units::distance(6.0, units::foot);
        vehParams.rollingResistance = 2000.0;
        vehicle = new Vehicle;
        vehicle->initialize(vehParams);
    }

    if (transmission == nullptr) {
        const double gearRatios[] = { 2.97, 2.07, 1.43, 1.00, 0.84, 0.56 };
        Transmission::Parameters tParams;
        tParams.GearCount = 6;
        tParams.GearRatios = gearRatios;
        tParams.MaxClutchTorque = units::torque(1000.0, units::ft_lb);
        transmission = new Transmission;
        transmission->initialize(tParams);
    }

    *vehicleOut = vehicle;
    *transmissionOut = transmission;
}

void EngineSimApplication::processEngineInput() {
    if (m_iceEngine == nullptr) {
        return;
    }

    const float dt = m_engine.GetFrameLength();
    const bool fineControlMode = m_engine.IsKeyDown(ysKey::Code::Space);

    const int mouseWheel = m_engine.GetMouseWheel();
    const int mouseWheelDelta = mouseWheel - m_lastMouseWheel;
    m_lastMouseWheel = mouseWheel;

    bool fineControlInUse = false;
    if (m_engine.IsKeyDown(ysKey::Code::Z)) {
        const double rate = fineControlMode
            ? 0.001
            : 0.01;

        Synthesizer::AudioParameters audioParams = m_simulator->synthesizer().getAudioParameters();
        audioParams.volume = clamp(audioParams.volume + mouseWheelDelta * rate * dt);

        m_simulator->synthesizer().setAudioParameters(audioParams);
        fineControlInUse = true;

        m_simple.volume = audioParams.volume;
        m_settings.setDouble("volume", m_simple.volume);
        m_infoCluster->setLogMessage("[Z] - Set volume to " + std::to_string((int)std::round(audioParams.volume * 100)) + "%");
    }
    else if (m_engine.IsKeyDown(ysKey::Code::X)) {
        const double rate = fineControlMode
            ? 0.001
            : 0.01;

        Synthesizer::AudioParameters audioParams = m_simulator->synthesizer().getAudioParameters();
        audioParams.convolution = clamp(audioParams.convolution + mouseWheelDelta * rate * dt);

        m_simulator->synthesizer().setAudioParameters(audioParams);
        fineControlInUse = true;

        m_infoCluster->setLogMessage("[X] - Set convolution level to " + std::to_string(audioParams.convolution));
    }
    else if (m_engine.IsKeyDown(ysKey::Code::C)) {
        const double rate = fineControlMode
            ? 0.00001
            : 0.001;

        Synthesizer::AudioParameters audioParams = m_simulator->synthesizer().getAudioParameters();
        audioParams.dF_F_mix = clamp(audioParams.dF_F_mix + mouseWheelDelta * rate * dt);

        m_simulator->synthesizer().setAudioParameters(audioParams);
        fineControlInUse = true;

        m_infoCluster->setLogMessage("[C] - Set high freq. gain to " + std::to_string(audioParams.dF_F_mix));
    }
    else if (m_engine.IsKeyDown(ysKey::Code::V)) {
        const double rate = fineControlMode
            ? 0.001
            : 0.01;

        Synthesizer::AudioParameters audioParams = m_simulator->synthesizer().getAudioParameters();
        audioParams.airNoise = clamp(audioParams.airNoise + mouseWheelDelta * rate * dt);

        m_simulator->synthesizer().setAudioParameters(audioParams);
        fineControlInUse = true;

        m_infoCluster->setLogMessage("[V] - Set low freq. noise to " + std::to_string(audioParams.airNoise));
    }
    else if (m_engine.IsKeyDown(ysKey::Code::B)) {
        const double rate = fineControlMode
            ? 0.001
            : 0.01;

        Synthesizer::AudioParameters audioParams = m_simulator->synthesizer().getAudioParameters();
        audioParams.inputSampleNoise = clamp(audioParams.inputSampleNoise + mouseWheelDelta * rate * dt);

        m_simulator->synthesizer().setAudioParameters(audioParams);
        fineControlInUse = true;

        m_infoCluster->setLogMessage("[B] - Set high freq. noise to " + std::to_string(audioParams.inputSampleNoise));
    }
    else if (m_engine.IsKeyDown(ysKey::Code::N)) {
        const double rate = fineControlMode
            ? 10.0
            : 100.0;

        const double newSimulationFrequency = clamp(
            m_simulator->getSimulationFrequency() + mouseWheelDelta * rate * dt,
            400.0, 400000.0);

        m_simulator->setSimulationFrequency(newSimulationFrequency);
        fineControlInUse = true;

        m_infoCluster->setLogMessage("[N] - Set simulation freq to " + std::to_string(m_simulator->getSimulationFrequency()));
    }
    else if (m_engine.IsKeyDown(ysKey::Code::G) && m_simulator->m_dyno.m_hold) {
        if (mouseWheelDelta > 0) {
            m_dynoSpeed += m_iceEngine->getDynoHoldStep();
        }
        else if (mouseWheelDelta < 0) {
            m_dynoSpeed -= m_iceEngine->getDynoHoldStep();
        }

        m_dynoSpeed = clamp(m_dynoSpeed, m_iceEngine->getDynoMinSpeed(), m_iceEngine->getDynoMaxSpeed());

        m_infoCluster->setLogMessage("[G] - Set dyno speed to " + std::to_string(units::toRpm(m_dynoSpeed)));
        fineControlInUse = true;
    }

    // Simple Edition: the on-screen volume slider
    if (m_simple.volumeChanged) {
        m_simple.volumeChanged = false;
        applyVolume();
        m_settings.setDouble("volume", m_simple.volume);
    }

    // Simple Edition: the on-screen throttle slider sets the resting throttle;
    // the Q/W/E/R keys still override it while held.
    const double prevTargetThrottle = m_targetSpeedSetting;
    m_targetSpeedSetting = fineControlMode ? m_targetSpeedSetting : m_simple.throttle;
    if (m_engine.IsKeyDown(ysKey::Code::Q)) {
        m_targetSpeedSetting = 0.01;
    }
    else if (m_engine.IsKeyDown(ysKey::Code::W)) {
        m_targetSpeedSetting = 0.1;
    }
    else if (m_engine.IsKeyDown(ysKey::Code::E)) {
        m_targetSpeedSetting = 0.2;
    }
    else if (m_engine.IsKeyDown(ysKey::Code::R)) {
        m_targetSpeedSetting = 1.0;
    }
    else if (fineControlMode && !fineControlInUse) {
        m_targetSpeedSetting = clamp(m_targetSpeedSetting + mouseWheelDelta * 0.0001);
    }

    if (prevTargetThrottle != m_targetSpeedSetting) {
        m_infoCluster->setLogMessage(
            "Throttle " + std::to_string((int)std::round(m_targetSpeedSetting * 100)) + "%");
    }

    m_speedSetting = m_targetSpeedSetting * 0.5 + 0.5 * m_speedSetting;

    m_iceEngine->setSpeedControl(m_speedSetting);
    if (m_engine.ProcessKeyDown(ysKey::Code::M)) {
        const int currentLayer = getViewParameters().Layer0;
        if (currentLayer + 1 < m_iceEngine->getMaxDepth()) {
            setViewLayer(currentLayer + 1);
        }

        m_infoCluster->setLogMessage("[M] - Set render layer to " + std::to_string(getViewParameters().Layer0));
    }

    if (m_engine.ProcessKeyDown(ysKey::Code::OEM_Comma)) {
        if (getViewParameters().Layer0 - 1 >= 0)
            setViewLayer(getViewParameters().Layer0 - 1);

        m_infoCluster->setLogMessage("[,] - Set render layer to " + std::to_string(getViewParameters().Layer0));
    }

    if (m_engine.ProcessKeyDown(ysKey::Code::D)) {
        m_simulator->m_dyno.m_enabled = !m_simulator->m_dyno.m_enabled;

        const std::string msg = m_simulator->m_dyno.m_enabled
            ? "DYNOMOMETER ENABLED"
            : "DYNOMOMETER DISABLED";
        m_infoCluster->setLogMessage(msg);
    }

    if (m_engine.ProcessKeyDown(ysKey::Code::H)) {
        m_simulator->m_dyno.m_hold = !m_simulator->m_dyno.m_hold;

        const std::string msg = m_simulator->m_dyno.m_hold
            ? m_simulator->m_dyno.m_enabled ? "HOLD ENABLED" : "HOLD ON STANDBY [ENABLE DYNO. FOR HOLD]"
            : "HOLD DISABLED";
        m_infoCluster->setLogMessage(msg);
    }

    if (m_simulator->m_dyno.m_enabled) {
        if (!m_simulator->m_dyno.m_hold) {
            if (m_simulator->getFilteredDynoTorque() > units::torque(1.0, units::ft_lb)) {
                m_dynoSpeed += units::rpm(500) * dt;
            }
            else {
                m_dynoSpeed *= (1 / (1 + dt));
            }

            if (m_dynoSpeed > m_iceEngine->getRedline()) {
                m_simulator->m_dyno.m_enabled = false;
                m_dynoSpeed = units::rpm(0);
            }
        }
    }
    else {
        if (!m_simulator->m_dyno.m_hold) {
            m_dynoSpeed = units::rpm(0);
        }
    }

    m_dynoSpeed = clamp(m_dynoSpeed, m_iceEngine->getDynoMinSpeed(), m_iceEngine->getDynoMaxSpeed());
    m_simulator->m_dyno.m_rotationSpeed = m_dynoSpeed;

    const bool prevStarterEnabled = m_simulator->m_starterMotor.m_enabled;
    if (m_engine.IsKeyDown(ysKey::Code::S) || m_simple.starterHeld) {
        m_simulator->m_starterMotor.m_enabled = true;

        // Simple Edition: the START button also switches the ignition on
        if (m_simple.starterHeld && !m_iceEngine->getIgnitionModule()->m_enabled) {
            m_iceEngine->getIgnitionModule()->m_enabled = true;
        }
    }
    else {
        m_simulator->m_starterMotor.m_enabled = false;
    }

    if (prevStarterEnabled != m_simulator->m_starterMotor.m_enabled) {
        const std::string msg = m_simulator->m_starterMotor.m_enabled
            ? "STARTER ENABLED"
            : "STARTER DISABLED";
        m_infoCluster->setLogMessage(msg);
    }

    if (m_engine.ProcessKeyDown(ysKey::Code::A)) {
        m_simulator->getEngine()->getIgnitionModule()->m_enabled =
            !m_simulator->getEngine()->getIgnitionModule()->m_enabled;

        const std::string msg = m_simulator->getEngine()->getIgnitionModule()->m_enabled
            ? "IGNITION ENABLED"
            : "IGNITION DISABLED";
        m_infoCluster->setLogMessage(msg);
    }

    if (m_engine.ProcessKeyDown(ysKey::Code::Up)) {
        m_simulator->getTransmission()->changeGear(m_simulator->getTransmission()->getGear() + 1);

        m_infoCluster->setLogMessage(
            "UPSHIFTED TO " + std::to_string(m_simulator->getTransmission()->getGear() + 1));
    }
    else if (m_engine.ProcessKeyDown(ysKey::Code::Down)) {
        m_simulator->getTransmission()->changeGear(m_simulator->getTransmission()->getGear() - 1);

        if (m_simulator->getTransmission()->getGear() != -1) {
            m_infoCluster->setLogMessage(
                "DOWNSHIFTED TO " + std::to_string(m_simulator->getTransmission()->getGear() + 1));
        }
        else {
            m_infoCluster->setLogMessage("SHIFTED TO NEUTRAL");
        }
    }

    if (m_engine.IsKeyDown(ysKey::Code::T)) {
        m_targetClutchPressure -= 0.2 * dt;
    }
    else if (m_engine.IsKeyDown(ysKey::Code::U)) {
        m_targetClutchPressure += 0.2 * dt;
    }
    else if (m_engine.IsKeyDown(ysKey::Code::Shift)) {
        m_targetClutchPressure = 0.0;
        m_infoCluster->setLogMessage("CLUTCH DEPRESSED");
    }
    else if (!m_engine.IsKeyDown(ysKey::Code::Y)) {
        m_targetClutchPressure = 1.0;
    }

    m_targetClutchPressure = clamp(m_targetClutchPressure);

    double clutchRC = 0.001;
    if (m_engine.IsKeyDown(ysKey::Code::Space)) {
        clutchRC = 1.0;
    }

    const double clutch_s = dt / (dt + clutchRC);
    m_clutchPressure = m_clutchPressure * (1 - clutch_s) + m_targetClutchPressure * clutch_s;
    m_simulator->getTransmission()->setClutchPressure(m_clutchPressure);

    // Simple Edition: a Sound Studio mode overrides throttle, dyno, gear and
    // starter so the engine stays in one steady sound.
    if (m_studio.isActive()) {
        m_studio.apply(dt, m_simulator, m_iceEngine, m_simulator->getTransmission());
        m_dynoSpeed = m_simulator->m_dyno.m_rotationSpeed;
    }
}

void EngineSimApplication::renderScene() {
    getShaders()->ResetBaseColor();
    getShaders()->SetObjectTransform(ysMath::LoadIdentity());

    m_textRenderer.SetColor(ysColor::linearToSrgb(m_foreground));
    m_shaders.SetClearColor(ysColor::linearToSrgb(m_shadow));

    const int screenWidth = m_engine.GetGameWindow()->GetGameWidth();
    const int screenHeight = m_engine.GetGameWindow()->GetGameHeight();
    const float aspectRatio = screenWidth / (float)screenHeight;

    const Point cameraPos = m_engineView->getCameraPosition();
    m_shaders.m_cameraPosition = ysMath::LoadVector(cameraPos.x, cameraPos.y);

    m_shaders.CalculateUiCamera(screenWidth, screenHeight);

    // Simple Edition: reserve a strip at the bottom for the control bar
    const float barHeight = (float)std::max(56, std::min(84, screenHeight / 13));
    const Bounds contentBounds(
        (float)screenWidth, (float)screenHeight - barHeight, { 0, (float)screenHeight });

    if (m_screen == 0) {
        Bounds windowBounds = contentBounds;
        Grid grid;
        grid.v_cells = 2;
        grid.h_cells = 3;
        Grid grid3x3;
        grid3x3.v_cells = 3;
        grid3x3.h_cells = 3;
        m_engineView->setDrawFrame(true);
        m_engineView->setBounds(grid.get(windowBounds, 1, 0, 1, 1));
        m_engineView->setLocalPosition({ 0, 0 });

        m_rightGaugeCluster->m_bounds = grid.get(windowBounds, 2, 0, 1, 2);
        m_oscCluster->m_bounds = grid.get(windowBounds, 1, 1);
        m_performanceCluster->m_bounds = grid3x3.get(windowBounds, 0, 1);
        m_loadSimulationCluster->m_bounds = grid3x3.get(windowBounds, 0, 2);

        Grid grid1x3;
        grid1x3.v_cells = 3;
        grid1x3.h_cells = 1;
        m_mixerCluster->m_bounds = grid1x3.get(grid3x3.get(windowBounds, 0, 0), 0, 2);
        m_infoCluster->m_bounds = grid1x3.get(grid3x3.get(windowBounds, 0, 0), 0, 0, 1, 2);

        m_engineView->setVisible(true);
        m_rightGaugeCluster->setVisible(true);
        m_oscCluster->setVisible(true);
        m_performanceCluster->setVisible(true);
        m_loadSimulationCluster->setVisible(true);
        m_mixerCluster->setVisible(true);
        m_infoCluster->setVisible(true);

        m_oscCluster->activate();
    }
    else if (m_screen == 1) {
        Bounds windowBounds = contentBounds;
        m_engineView->setDrawFrame(false);
        m_engineView->setBounds(windowBounds);
        m_engineView->setLocalPosition({ 0, 0 });
        m_engineView->activate();

        m_engineView->setVisible(true);
        m_rightGaugeCluster->setVisible(false);
        m_oscCluster->setVisible(false);
        m_performanceCluster->setVisible(false);
        m_loadSimulationCluster->setVisible(false);
        m_mixerCluster->setVisible(false);
        m_infoCluster->setVisible(false);
    }
    else if (m_screen == 2) {
        Bounds windowBounds = contentBounds;
        Grid grid;
        grid.v_cells = 1;
        grid.h_cells = 3;
        m_engineView->setDrawFrame(true);
        m_engineView->setBounds(grid.get(windowBounds, 0, 0, 2, 1));
        m_engineView->setLocalPosition({ 0, 0 });
        m_engineView->activate();

        m_rightGaugeCluster->m_bounds = grid.get(windowBounds, 2, 0, 1, 1);

        m_engineView->setVisible(true);
        m_rightGaugeCluster->setVisible(true);
        m_oscCluster->setVisible(false);
        m_performanceCluster->setVisible(false);
        m_loadSimulationCluster->setVisible(false);
        m_mixerCluster->setVisible(false);
        m_infoCluster->setVisible(false);
    }

    // Simple Edition: Sound Studio takes the right third, engine on the left
    if (m_studioPanel != nullptr) {
        const bool studio = m_simple.studioOpen && !isOverlayOpen();
        m_studioPanel->setVisible(studio);
        if (studio) {
            Grid grid;
            grid.v_cells = 1;
            grid.h_cells = 3;
            m_engineView->setDrawFrame(true);
            m_engineView->setBounds(grid.get(contentBounds, 0, 0, 2, 1));
            m_engineView->setLocalPosition({ 0, 0 });
            m_engineView->setVisible(true);
            m_studioPanel->m_bounds = grid.get(contentBounds, 2, 0, 1, 1);
            m_studioPanel->activate();

            m_rightGaugeCluster->setVisible(false);
            m_oscCluster->setVisible(false);
            m_performanceCluster->setVisible(false);
            m_loadSimulationCluster->setVisible(false);
            m_mixerCluster->setVisible(false);
            m_infoCluster->setVisible(false);
        }
    }

    // Simple Edition: control bar, message banner and full screen panels
    if (m_controlBar != nullptr) {
        m_controlBar->m_bounds = Bounds((float)screenWidth, barHeight, { 0.0f, barHeight }, Bounds::tl);
        m_controlBar->setVisible(true);
        m_controlBar->activate();
    }

    if (m_banner != nullptr) {
        m_banner->m_bounds = contentBounds;
        m_banner->activate();
    }

    if (m_overlay != nullptr) {
        m_overlay->m_bounds = contentBounds;
        m_overlay->activate();
    }

    if (isOverlayOpen()) {
        m_engineView->setVisible(false);
        m_rightGaugeCluster->setVisible(false);
        m_oscCluster->setVisible(false);
        m_performanceCluster->setVisible(false);
        m_loadSimulationCluster->setVisible(false);
        m_mixerCluster->setVisible(false);
        m_infoCluster->setVisible(false);
    }

    const float cameraAspectRatio =
        m_engineView->m_bounds.width() / m_engineView->m_bounds.height();
    m_engine.GetDevice()->ResizeRenderTarget(
        m_mainRenderTarget,
        m_engineView->m_bounds.width(),
        m_engineView->m_bounds.height(),
        m_engineView->m_bounds.width(),
        m_engineView->m_bounds.height()
    );
    m_engine.GetDevice()->RepositionRenderTarget(
        m_mainRenderTarget,
        m_engineView->m_bounds.getPosition(Bounds::tl).x,
        screenHeight - m_engineView->m_bounds.getPosition(Bounds::tl).y
    );
    m_shaders.CalculateCamera(
        cameraAspectRatio * m_displayHeight / m_engineView->m_zoom,
        m_displayHeight / m_engineView->m_zoom,
        m_engineView->m_bounds,
        m_screenWidth,
        m_screenHeight,
        m_displayAngle);

    m_geometryGenerator.reset();

    render();

    m_engine.GetDevice()->EditBufferDataRange(
        m_geometryVertexBuffer,
        (char *)m_geometryGenerator.getVertexData(),
        sizeof(dbasic::Vertex) * m_geometryGenerator.getCurrentVertexCount(),
        0);

    m_engine.GetDevice()->EditBufferDataRange(
        m_geometryIndexBuffer,
        (char *)m_geometryGenerator.getIndexData(),
        sizeof(unsigned short) * m_geometryGenerator.getCurrentIndexCount(),
        0);
}

void EngineSimApplication::refreshUserInterface() {
    m_uiManager.destroy();
    m_uiManager.initialize(this);

    m_engineView = m_uiManager.getRoot()->addElement<EngineView>();
    m_rightGaugeCluster = m_uiManager.getRoot()->addElement<RightGaugeCluster>();
    m_oscCluster = m_uiManager.getRoot()->addElement<OscilloscopeCluster>();
    m_performanceCluster = m_uiManager.getRoot()->addElement<PerformanceCluster>();
    m_loadSimulationCluster = m_uiManager.getRoot()->addElement<LoadSimulationCluster>();
    m_mixerCluster = m_uiManager.getRoot()->addElement<MixerCluster>();
    m_infoCluster = m_uiManager.getRoot()->addElement<InfoCluster>();

    m_infoCluster->setEngine(m_iceEngine);
    m_rightGaugeCluster->m_simulator = m_simulator;
    m_rightGaugeCluster->setEngine(m_iceEngine);
    m_oscCluster->setSimulator(m_simulator);
    if (m_iceEngine != nullptr) {
        m_oscCluster->setDynoMaxRange(units::toRpm(m_iceEngine->getRedline()));
    }
    m_performanceCluster->setSimulator(m_simulator);
    m_loadSimulationCluster->setSimulator(m_simulator);
    m_mixerCluster->setSimulator(m_simulator);

    // Simple Edition
    m_controlBar = m_uiManager.getRoot()->addElement<SimpleControlBar>();
    m_controlBar->build(&m_simple);

    m_banner = m_uiManager.getRoot()->addElement<SimpleMessageBanner>();
    m_banner->setState(&m_simple);

    m_studioPanel = m_uiManager.getRoot()->addElement<SimpleStudioPanel>();
    m_studioPanel->build(&m_simple);
    m_studioPanel->setVisible(false);

    m_overlay = m_uiManager.getRoot()->addElement<SimpleOverlayPanel>();
    m_overlay->build(&m_simple);

    m_lastInfoMessage = m_infoCluster->getLogMessage();
}

void EngineSimApplication::startRecording() {
    m_recording = true;

#ifdef ATG_ENGINE_SIM_VIDEO_CAPTURE
    atg_dtv::Encoder::VideoSettings settings{};

    // Output filename
    settings.fname = "../workspace/video_capture/engine_sim_video_capture.mp4";
    settings.inputWidth = m_engine.GetScreenWidth();
    settings.inputHeight = m_engine.GetScreenHeight();
    settings.width = settings.inputWidth;
    settings.height = settings.inputHeight;
    settings.hardwareEncoding = true;
    settings.inputAlpha = true;
    settings.bitRate = 40000000;

    m_encoder.run(settings, 2);
#endif /* ATG_ENGINE_SIM_VIDEO_CAPTURE */
}

void EngineSimApplication::updateScreenSizeStability() {
    m_screenResolution[m_screenResolutionIndex][0] = m_engine.GetScreenWidth();
    m_screenResolution[m_screenResolutionIndex][1] = m_engine.GetScreenHeight();

    m_screenResolutionIndex = (m_screenResolutionIndex + 1) % ScreenResolutionHistoryLength;
}

bool EngineSimApplication::readyToRecord() {
    const int w = m_screenResolution[0][0];
    const int h = m_screenResolution[0][1];

    if (w <= 0 && h <= 0) return false;
    if ((w % 2) != 0 || (h % 2) != 0) return false;

    for (int i = 1; i < ScreenResolutionHistoryLength; ++i) {
        if (m_screenResolution[i][0] != w) return false;
        if (m_screenResolution[i][1] != h) return false;
    }

    return true;
}

void EngineSimApplication::stopRecording() {
    m_recording = false;

#ifdef ATG_ENGINE_SIM_VIDEO_CAPTURE
    m_encoder.commit();
    m_encoder.stop();
#endif /* ATG_ENGINE_SIM_VIDEO_CAPTURE */
}

void EngineSimApplication::recordFrame() {
#ifdef ATG_ENGINE_SIM_VIDEO_CAPTURE
    atg_dtv::Frame *frame = m_encoder.newFrame(false);
    if (frame != nullptr && m_encoder.getError() == atg_dtv::Encoder::Error::None) {
        m_engine.GetDevice()->ReadRenderTarget(m_engine.GetScreenRenderTarget(), frame->m_rgb);
    }

    m_encoder.submitFrame();
#endif /* ATG_ENGINE_SIM_VIDEO_CAPTURE */
}
