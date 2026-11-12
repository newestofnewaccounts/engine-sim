#include "../include/engine_builder.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <locale>
#include <map>
#include <sstream>

namespace {
    constexpr double Pi = 3.14159265358979323846;

    // Reference engine the scaling rules are calibrated against (GM LS V8).
    constexpr double RefDisplacementL = 5.7;
    constexpr double RefCylinderL = 5.7 / 8;
    constexpr double RefBoreMm = 96.0;
    constexpr double RefRedline = 6500.0;

    double clampd(double v, double lo, double hi) {
        return std::max(lo, std::min(hi, v));
    }

    std::string num(double v, int precision = 4) {
        std::ostringstream ss;
        ss.imbue(std::locale::classic());
        ss << std::fixed << std::setprecision(precision) << v;
        return ss.str();
    }

    double wrap360(double deg) {
        double r = std::fmod(deg, 360.0);
        if (r < 0) r += 360.0;
        return r;
    }

    struct Cylinder {
        int number = 1;          // 1-based cylinder number
        int bank = 0;            // 0 or 1
        double firingAngle = 0;  // degrees within the 720 degree cycle
        double journalAngle = 0; // degrees
    };
}

std::vector<int> EngineBuilder::allowedCylinders(Settings::Layout layout) {
    switch (layout) {
    case Settings::Layout::Inline: return { 1, 2, 3, 4, 5, 6, 8 };
    case Settings::Layout::V: return { 6, 8, 10, 12 };
    case Settings::Layout::Flat: return { 2, 4, 6 };
    }

    return { 4 };
}

double EngineBuilder::minDisplacement(const Settings &s) {
    return std::max(0.1, std::ceil(s.cylinders * 0.1 * 10.0 - 1e-6) / 10.0);
}

double EngineBuilder::maxDisplacement(const Settings &s) {
    return std::min(9.0, std::floor(s.cylinders * 1.1 * 10.0 + 1e-6) / 10.0);
}

void EngineBuilder::sanitize(Settings *s) {
    const std::vector<int> allowed = allowedCylinders(s->layout);
    if (std::find(allowed.begin(), allowed.end(), s->cylinders) == allowed.end()) {
        // Pick the closest allowed count
        int best = allowed[0];
        for (int c : allowed) {
            if (std::abs(c - s->cylinders) < std::abs(best - s->cylinders)) best = c;
        }

        s->cylinders = best;
    }

    s->displacementLiters = std::round(s->displacementLiters * 10.0) / 10.0;
    s->displacementLiters =
        clampd(s->displacementLiters, minDisplacement(*s), maxDisplacement(*s));

    s->redlineRpm = (int)clampd(s->redlineRpm, 3000, 12000);
    s->redlineRpm = (s->redlineRpm / 250) * 250;
}

std::string EngineBuilder::layoutName(Settings::Layout layout) {
    switch (layout) {
    case Settings::Layout::Inline: return "Inline";
    case Settings::Layout::V: return "V";
    case Settings::Layout::Flat: return "Flat (boxer)";
    }

    return "";
}

std::string EngineBuilder::characterName(Settings::Character c) {
    switch (c) {
    case Settings::Character::Torquey: return "Torquey (long stroke)";
    case Settings::Character::Balanced: return "Balanced";
    case Settings::Character::RevHappy: return "Rev-happy (short stroke)";
    }

    return "";
}

std::string EngineBuilder::camName(Settings::Cam c) {
    switch (c) {
    case Settings::Cam::Mild: return "Mild (smooth idle)";
    case Settings::Cam::Sport: return "Sport";
    case Settings::Cam::Race: return "Race (lumpy idle)";
    }

    return "";
}

std::string EngineBuilder::exhaustName(Settings::Exhaust e) {
    switch (e) {
    case Settings::Exhaust::Quiet: return "Quiet";
    case Settings::Exhaust::Sporty: return "Sporty";
    case Settings::Exhaust::Loud: return "Loud (straight pipe)";
    }

    return "";
}

std::string EngineBuilder::engineName(const Settings &s) {
    std::string prefix;
    switch (s.layout) {
    case Settings::Layout::Inline: prefix = "I"; break;
    case Settings::Layout::V: prefix = "V"; break;
    case Settings::Layout::Flat: prefix = "Flat-"; break;
    }

    return "Custom " + prefix + std::to_string(s.cylinders) + " " + num(s.displacementLiters, 1) + "L";
}

std::string EngineBuilder::fileStem(const Settings &s) {
    std::string prefix;
    switch (s.layout) {
    case Settings::Layout::Inline: prefix = "i"; break;
    case Settings::Layout::V: prefix = "v"; break;
    case Settings::Layout::Flat: prefix = "flat"; break;
    }

    std::string size = num(s.displacementLiters, 1);
    std::replace(size.begin(), size.end(), '.', '_');

    return "custom_" + prefix + std::to_string(s.cylinders) + "_" + size + "l_" + std::to_string(s.redlineRpm);
}

std::vector<int> EngineBuilder::firingOrder(Settings::Layout layout, int n) {
    if (layout == Settings::Layout::Inline) {
        switch (n) {
        case 1: return { 1 };
        case 2: return { 1, 2 };
        case 3: return { 1, 2, 3 };
        case 4: return { 1, 3, 4, 2 };
        case 5: return { 1, 2, 4, 5, 3 };
        case 6: return { 1, 5, 3, 6, 2, 4 };
        case 8: return { 1, 6, 2, 5, 8, 3, 7, 4 };
        }
    }
    else if (layout == Settings::Layout::V) {
        // Bank 0 holds cylinders 1..n/2, bank 1 holds the rest
        switch (n) {
        case 6: return { 1, 4, 2, 5, 3, 6 };
        case 8: return { 1, 5, 4, 8, 6, 3, 7, 2 };
        case 10: return { 1, 6, 5, 10, 2, 7, 3, 8, 4, 9 };
        case 12: return { 1, 7, 5, 11, 3, 9, 6, 12, 2, 8, 4, 10 };
        }
    }
    else if (layout == Settings::Layout::Flat) {
        switch (n) {
        case 2: return { 1, 2 };
        case 4: return { 1, 2, 3, 4 };
        case 6: return { 1, 6, 2, 4, 3, 5 };
        }
    }

    std::vector<int> order;
    for (int i = 1; i <= n; ++i) order.push_back(i);
    return order;
}

void EngineBuilder::boreStrokeMm(const Settings &s, double *bore, double *stroke) {
    double strokeRatio = 1.0;
    switch (s.character) {
    case Settings::Character::Torquey: strokeRatio = 1.15; break;
    case Settings::Character::Balanced: strokeRatio = 1.0; break;
    case Settings::Character::RevHappy: strokeRatio = 0.82; break;
    }

    const double cylinderMm3 = (s.displacementLiters / s.cylinders) * 1.0e6;
    *bore = std::cbrt(4.0 * cylinderMm3 / (Pi * strokeRatio));
    *stroke = *bore * strokeRatio;
}

void EngineBuilder::adjust(Settings *s, int row, int direction) {
    const int d = (direction >= 0) ? 1 : -1;
    auto cycle3 = [d](int v) { return (v + d + 3) % 3; };

    switch (row) {
    case 0:
    {
        const double perCylinder = s->displacementLiters / s->cylinders;
        s->layout = (Settings::Layout)cycle3((int)s->layout);
        const std::vector<int> allowed = allowedCylinders(s->layout);
        if (std::find(allowed.begin(), allowed.end(), s->cylinders) == allowed.end()) {
            s->cylinders = (s->layout == Settings::Layout::V) ? 8 : 4;
            s->displacementLiters = perCylinder * s->cylinders;
        }
        break;
    }
    case 1:
    {
        const double perCylinder = s->displacementLiters / s->cylinders;
        const std::vector<int> allowed = allowedCylinders(s->layout);
        auto it = std::find(allowed.begin(), allowed.end(), s->cylinders);
        int index = (it == allowed.end()) ? 0 : (int)(it - allowed.begin());
        index = std::max(0, std::min((int)allowed.size() - 1, index + d));
        s->cylinders = allowed[index];
        // Keep the size of each cylinder, so the engine grows/shrinks naturally
        s->displacementLiters = perCylinder * s->cylinders;
        break;
    }
    case 2:
    {
        const double step = (s->displacementLiters >= 3.0) ? 0.2 : 0.1;
        s->displacementLiters += d * step;
        break;
    }
    case 3:
        s->redlineRpm += d * 250;
        break;
    case 4:
        s->character = (Settings::Character)cycle3((int)s->character);
        break;
    case 5:
        s->cam = (Settings::Cam)cycle3((int)s->cam);
        break;
    case 6:
        s->exhaust = (Settings::Exhaust)cycle3((int)s->exhaust);
        break;
    }

    sanitize(s);
}

std::string EngineBuilder::serialize(const Settings &s) {
    std::ostringstream ss;
    ss.imbue(std::locale::classic());
    ss << "// engine-builder:"
        << " layout=" << (int)s.layout
        << " cylinders=" << s.cylinders
        << " liters=" << num(s.displacementLiters, 1)
        << " redline=" << s.redlineRpm
        << " character=" << (int)s.character
        << " cam=" << (int)s.cam
        << " exhaust=" << (int)s.exhaust;
    return ss.str();
}

bool EngineBuilder::deserialize(const std::string &line, Settings *s) {
    const std::string tag = "// engine-builder:";
    if (line.compare(0, tag.size(), tag) != 0) return false;

    std::istringstream ss(line.substr(tag.size()));
    ss.imbue(std::locale::classic());
    std::string token;
    Settings result;
    while (ss >> token) {
        const size_t eq = token.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = token.substr(0, eq);
        const std::string value = token.substr(eq + 1);

        try {
            if (key == "layout") result.layout = (Settings::Layout)(int)clampd(std::stoi(value), 0, 2);
            else if (key == "cylinders") result.cylinders = std::stoi(value);
            else if (key == "liters") result.displacementLiters = std::stod(value);
            else if (key == "redline") result.redlineRpm = std::stoi(value);
            else if (key == "character") result.character = (Settings::Character)(int)clampd(std::stoi(value), 0, 2);
            else if (key == "cam") result.cam = (Settings::Cam)(int)clampd(std::stoi(value), 0, 2);
            else if (key == "exhaust") result.exhaust = (Settings::Exhaust)(int)clampd(std::stoi(value), 0, 2);
        }
        catch (...) {
            return false;
        }
    }

    sanitize(&result);
    *s = result;
    return true;
}

std::string EngineBuilder::generateScript(const Settings &input) {
    Settings s = input;
    sanitize(&s);

    const int n = s.cylinders;
    const bool twoBanks = (s.layout != Settings::Layout::Inline) && n >= 2;
    const int bankCount = twoBanks ? 2 : 1;

    // ---------------------------------------------------------------
    // Geometry
    double bankAngle[2] = { 0.0, 0.0 };
    if (s.layout == Settings::Layout::V) {
        double v = 90.0;
        if (n == 6 || n == 12) v = 60.0;
        else if (n == 10) v = 72.0;
        bankAngle[0] = -v / 2;
        bankAngle[1] = v / 2;
    }
    else if (s.layout == Settings::Layout::Flat) {
        bankAngle[0] = 90.0;
        bankAngle[1] = -90.0;
    }

    const double tdc = 90.0 + bankAngle[0];

    const double cylinderL = s.displacementLiters / n;
    double strokeRatio = 1.0;
    switch (s.character) {
    case Settings::Character::Torquey: strokeRatio = 1.15; break;
    case Settings::Character::Balanced: strokeRatio = 1.0; break;
    case Settings::Character::RevHappy: strokeRatio = 0.82; break;
    }

    const double cylinderMm3 = cylinderL * 1.0e6;
    const double boreMm = std::cbrt(4.0 * cylinderMm3 / (Pi * strokeRatio));
    const double strokeMm = boreMm * strokeRatio;
    const double rodMm = strokeMm * 1.7;
    const double compressionHeightMm = boreMm * 0.27;

    const double boreRatio = boreMm / RefBoreMm;
    const double sizeRatio = s.displacementLiters / RefDisplacementL;
    const double cylRatio = cylinderL / RefCylinderL;
    const double rpmRatio = s.redlineRpm / RefRedline;

    // ---------------------------------------------------------------
    // Cylinders, firing order and crank journals
    const std::vector<int> order = firingOrder(s.layout, n);
    std::vector<Cylinder> cylinders(n);
    for (int i = 0; i < n; ++i) {
        cylinders[i].number = i + 1;
        cylinders[i].bank = (twoBanks && i >= n / 2) ? 1 : 0;
    }

    for (size_t k = 0; k < order.size(); ++k) {
        Cylinder &c = cylinders[order[k] - 1];
        c.firingAngle = (720.0 / n) * k;
    }

    for (Cylinder &c : cylinders) {
        // A cylinder on bank b reaches TDC at cycle angle
        // (journal + tdc - bankAngle[b] - 90). Solve for the journal angle.
        c.journalAngle = wrap360(c.firingAngle - bankAngle[0] + bankAngle[c.bank]);
    }

    // ---------------------------------------------------------------
    // Tuning values
    double camDuration = 234, camLiftThou = 551, lobeCenter = 116, compressionRatio = 10.0;
    double idleAirFactor = 1.0;
    switch (s.cam) {
    case Settings::Cam::Mild:
        camDuration = 216; camLiftThou = 450; lobeCenter = 116; compressionRatio = 9.5; idleAirFactor = 0.9;
        break;
    case Settings::Cam::Sport:
        camDuration = 234; camLiftThou = 551; lobeCenter = 114; compressionRatio = 10.0; idleAirFactor = 1.0;
        break;
    case Settings::Cam::Race:
        camDuration = 256; camLiftThou = 620; lobeCenter = 108; compressionRatio = 11.0; idleAirFactor = 1.6;
        break;
    }

    const double liftScale = clampd(boreRatio, 0.5, 1.3);
    const double flowScale = boreRatio * boreRatio;

    const double chamberCc = (cylinderL * 1000.0) / (compressionRatio - 1.0);
    const double intakeRunnerCc = 0.21 * cylinderL * 1000.0;
    const double exhaustRunnerCc = 0.07 * cylinderL * 1000.0;
    const double intakeRunnerInch = 2.2 * boreRatio;
    const double exhaustRunnerInch = 1.75 * boreRatio;

    const double intakeCfm = clampd(700.0 * sizeRatio * rpmRatio, 40.0, 2500.0);
    const double runnerCfm = clampd(250.0 * cylRatio * rpmRatio, 40.0, 500.0);
    const double outletCfm = clampd(1000.0 * sizeRatio * rpmRatio * 2.0 / bankCount, 300.0, 2500.0);
    const double primaryCfm = clampd(400.0 * cylRatio * rpmRatio, 100.0, 1000.0);
    const double plenumL = clampd(0.8 + 0.15 * s.displacementLiters, 0.5, 2.5);

    // Engines under 0.5 L: shrink the moving parts and friction with the
    // engine and give a little more idle air, otherwise tiny engines are too
    // heavy and draggy to keep themselves running.
    const double smallScale = std::min(1.0, s.displacementLiters / 0.5);
    idleAirFactor *= 1.0 + 0.6 * (1.0 - smallScale);
    const double idlePlate = 1.0 - clampd(0.004 * idleAirFactor / rpmRatio, 0.001, 0.03);

    const double starterTorque = clampd(200.0 * sizeRatio, 40.0, 250.0);
    const double frictionTorque = clampd(2.5 * s.displacementLiters, 0.25, 20.0);
    const double crankMassLb = clampd(60.0 * sizeRatio, std::max(2.0, 8.0 * smallScale), 80.0);
    const double flywheelMassLb =
        clampd(10.0 + 4.0 * s.displacementLiters + (n <= 2 ? 6.0 : 0.0), 10.0, 40.0) * std::max(0.3, smallScale);
    const double flywheelRadiusIn = clampd(5.5 + 0.4 * s.displacementLiters, 5.5, 8.0);
    const double pistonMassG = clampd(100.0 * std::pow(boreRatio, 3.0), 30.0, 250.0);
    const double rodMassG = clampd(50.0 * std::pow(boreRatio, 3.0), 20.0, 150.0);
    const double clutchTorque = clampd(100.0 * s.displacementLiters, 100.0, 1000.0);

    double frequencyBudget = 80000.0 * std::max(1.0, s.redlineRpm / 8000.0);
    frequencyBudget = std::min(frequencyBudget, 110000.0);
    const int simulationFrequency = (int)clampd(std::round(frequencyBudget / n / 500.0) * 500.0, 5000.0, 40000.0);

    std::string irName = "mild_exhaust_0";
    double audioVolume = 1.0;
    switch (s.exhaust) {
    case Settings::Exhaust::Quiet: irName = "mild_exhaust_0"; audioVolume = 0.6; break;
    case Settings::Exhaust::Sporty: irName = "mild_exhaust_0_reverb"; audioVolume = 1.0; break;
    case Settings::Exhaust::Loud: irName = "minimal_muffling_01"; audioVolume = 1.0; break;
    }
    // Fewer cylinders sharing an exhaust means each pulse is louder relative
    // to a V8; keep things roughly level.
    audioVolume *= clampd(8.0 / n, 0.6, 2.0) * (twoBanks ? 1.0 : 0.8);

    // ---------------------------------------------------------------
    // Script
    std::ostringstream o;
    o.imbue(std::locale::classic());

    o << serialize(s) << "\n";
    o << "// Generated by the Engine Simulator Simple Edition engine builder.\n";
    o << "// You can edit this file by hand; it is a normal engine script.\n\n";
    o << "import \"engine_sim.mr\"\n\n";
    o << "units units()\n";
    o << "constants constants()\n";
    o << "impulse_response_library ir_lib()\n\n";

    // Ignition wires
    o << "private node builder_wires {\n";
    for (int i = 1; i <= n; ++i) {
        o << "    output wire" << i << ": ignition_wire();\n";
    }
    o << "}\n\n";

    // Cylinder head
    o << "private node builder_head {\n"
        "    input intake_camshaft;\n"
        "    input exhaust_camshaft;\n"
        "    input chamber_volume: 90 * units.cc;\n"
        "    input intake_runner_volume: 149.6 * units.cc;\n"
        "    input intake_runner_cross_section_area: 2.2 * units.inch * 2.2 * units.inch;\n"
        "    input exhaust_runner_volume: 50.0 * units.cc;\n"
        "    input exhaust_runner_cross_section_area: 1.75 * units.inch * 1.75 * units.inch;\n"
        "    input flow_attenuation: 1.0;\n"
        "    input lift_scale: 1.0;\n"
        "    input flip_display: false;\n"
        "    alias output __out: head;\n\n"
        "    function intake_flow(50 * lift_scale * units.thou)\n"
        "    intake_flow\n"
        "        .add_flow_sample(0 * lift_scale, 0 * flow_attenuation)\n"
        "        .add_flow_sample(50 * lift_scale, 1 * flow_attenuation)\n"
        "        .add_flow_sample(100 * lift_scale, 103 * flow_attenuation)\n"
        "        .add_flow_sample(150 * lift_scale, 156 * flow_attenuation)\n"
        "        .add_flow_sample(200 * lift_scale, 214 * flow_attenuation)\n"
        "        .add_flow_sample(250 * lift_scale, 249 * flow_attenuation)\n"
        "        .add_flow_sample(300 * lift_scale, 268 * flow_attenuation)\n"
        "        .add_flow_sample(350 * lift_scale, 280 * flow_attenuation)\n"
        "        .add_flow_sample(400 * lift_scale, 280 * flow_attenuation)\n"
        "        .add_flow_sample(450 * lift_scale, 281 * flow_attenuation)\n"
        "        .add_flow_sample(500 * lift_scale, 281 * flow_attenuation)\n"
        "        .add_flow_sample(550 * lift_scale, 281 * flow_attenuation)\n"
        "        .add_flow_sample(600 * lift_scale, 281 * flow_attenuation)\n"
        "        .add_flow_sample(650 * lift_scale, 281 * flow_attenuation)\n\n"
        "    function exhaust_flow(50 * lift_scale * units.thou)\n"
        "    exhaust_flow\n"
        "        .add_flow_sample(0 * lift_scale, 0 * flow_attenuation)\n"
        "        .add_flow_sample(50 * lift_scale, 1 * flow_attenuation)\n"
        "        .add_flow_sample(100 * lift_scale, 72 * flow_attenuation)\n"
        "        .add_flow_sample(150 * lift_scale, 113 * flow_attenuation)\n"
        "        .add_flow_sample(200 * lift_scale, 160 * flow_attenuation)\n"
        "        .add_flow_sample(250 * lift_scale, 196 * flow_attenuation)\n"
        "        .add_flow_sample(300 * lift_scale, 222 * flow_attenuation)\n"
        "        .add_flow_sample(350 * lift_scale, 235 * flow_attenuation)\n"
        "        .add_flow_sample(400 * lift_scale, 245 * flow_attenuation)\n"
        "        .add_flow_sample(450 * lift_scale, 246 * flow_attenuation)\n"
        "        .add_flow_sample(500 * lift_scale, 246 * flow_attenuation)\n"
        "        .add_flow_sample(550 * lift_scale, 246 * flow_attenuation)\n"
        "        .add_flow_sample(600 * lift_scale, 246 * flow_attenuation)\n"
        "        .add_flow_sample(650 * lift_scale, 246 * flow_attenuation)\n\n"
        "    generic_cylinder_head head(\n"
        "        chamber_volume: chamber_volume,\n"
        "        intake_runner_volume: intake_runner_volume,\n"
        "        intake_runner_cross_section_area: intake_runner_cross_section_area,\n"
        "        exhaust_runner_volume: exhaust_runner_volume,\n"
        "        exhaust_runner_cross_section_area: exhaust_runner_cross_section_area,\n\n"
        "        intake_port_flow: intake_flow,\n"
        "        exhaust_port_flow: exhaust_flow,\n"
        "        valvetrain: standard_valvetrain(\n"
        "            intake_camshaft: intake_camshaft,\n"
        "            exhaust_camshaft: exhaust_camshaft\n"
        "        ),\n"
        "        flip_display: flip_display\n"
        "    )\n"
        "}\n\n";

    o << "private node builder_turbulence_to_flame_speed_ratio {\n"
        "    alias output __out:\n"
        "        function(5.0)\n"
        "            .add_sample(0.0, 3.0)\n"
        "            .add_sample(5.0, 1.5 * 5.0)\n"
        "            .add_sample(10.0, 1.75 * 10.0)\n"
        "            .add_sample(15.0, 2.0 * 15.0)\n"
        "            .add_sample(20.0, 2.0 * 20.0)\n"
        "            .add_sample(25.0, 2.0 * 25.0)\n"
        "            .add_sample(30.0, 2.0 * 30.0)\n"
        "            .add_sample(35.0, 2.0 * 35.0)\n"
        "            .add_sample(40.0, 2.0 * 40.0)\n"
        "            .add_sample(45.0, 2.0 * 45.0);\n"
        "}\n\n";

    // Engine
    o << "public node custom_engine {\n";
    o << "    alias output __out: engine;\n\n";
    o << "    engine engine(\n";
    o << "        name: \"" << engineName(s) << "\",\n";
    o << "        starter_torque: " << num(starterTorque, 1) << " * units.lb_ft,\n";
    o << "        starter_speed: 300 * units.rpm,\n";
    o << "        redline: " << s.redlineRpm << " * units.rpm,\n";
    o << "        throttle_gamma: 2.0,\n";
    o << "        fuel: fuel(\n";
    o << "            max_burning_efficiency: 1.0,\n";
    o << "            turbulence_to_flame_speed_ratio: builder_turbulence_to_flame_speed_ratio()\n";
    o << "        ),\n";
    o << "        hf_gain: 0.01,\n";
    o << "        noise: 1.0,\n";
    o << "        jitter: 0.5,\n";
    o << "        simulation_frequency: " << simulationFrequency << "\n";
    o << "    )\n\n";

    o << "    builder_wires wires()\n\n";

    o << "    label stroke(" << num(strokeMm, 3) << " * units.mm)\n";
    o << "    label bore(" << num(boreMm, 3) << " * units.mm)\n";
    o << "    label rod_length(" << num(rodMm, 3) << " * units.mm)\n";
    o << "    label rod_mass(" << num(rodMassG, 1) << " * units.g)\n";
    o << "    label compression_height(" << num(compressionHeightMm, 3) << " * units.mm)\n";
    o << "    label crank_mass(" << num(crankMassLb, 2) << " * units.lb)\n";
    o << "    label flywheel_mass(" << num(flywheelMassLb, 2) << " * units.lb)\n";
    o << "    label flywheel_radius(" << num(flywheelRadiusIn, 2) << " * units.inch)\n\n";

    o << "    label crank_moment(\n"
        "        1.5 * disk_moment_of_inertia(mass: crank_mass, radius: stroke)\n"
        "    )\n"
        "    label flywheel_moment(\n"
        "        disk_moment_of_inertia(mass: flywheel_mass, radius: flywheel_radius)\n"
        "    )\n"
        "    label other_moment(\n"
        "        disk_moment_of_inertia(mass: 1 * units.kg, radius: 1.0 * units.cm)\n"
        "    )\n\n";

    o << "    crankshaft c0(\n";
    o << "        throw: stroke / 2,\n";
    o << "        flywheel_mass: flywheel_mass,\n";
    o << "        mass: crank_mass,\n";
    o << "        friction_torque: " << num(frictionTorque, 2) << " * units.lb_ft,\n";
    o << "        moment_of_inertia: crank_moment + flywheel_moment + other_moment,\n";
    o << "        position_x: 0.0,\n";
    o << "        position_y: 0.0,\n";
    o << "        tdc: " << num(tdc, 3) << " * units.deg\n";
    o << "    )\n\n";

    for (const Cylinder &c : cylinders) {
        o << "    rod_journal rj" << c.number << "(angle: " << num(c.journalAngle, 3) << " * units.deg)\n";
    }
    o << "    c0\n";
    for (const Cylinder &c : cylinders) {
        o << "        .add_rod_journal(rj" << c.number << ")\n";
    }
    o << "\n";

    o << "    piston_parameters piston_params(\n";
    o << "        mass: " << num(pistonMassG, 1) << " * units.g,\n";
    o << "        compression_height: compression_height,\n";
    o << "        wrist_pin_position: 0.0,\n";
    o << "        displacement: 0.0\n";
    o << "    )\n\n";

    o << "    connecting_rod_parameters cr_params(\n"
        "        mass: rod_mass,\n"
        "        moment_of_inertia: rod_moment_of_inertia(\n"
        "            mass: rod_mass,\n"
        "            length: rod_length\n"
        "        ),\n"
        "        center_of_mass: 0.0,\n"
        "        length: rod_length\n"
        "    )\n\n";

    o << "    intake intake(\n";
    o << "        plenum_volume: " << num(plenumL, 3) << " * units.L,\n";
    o << "        plenum_cross_section_area: 20.0 * units.cm2,\n";
    o << "        intake_flow_rate: k_carb(" << num(intakeCfm, 1) << "),\n";
    o << "        runner_flow_rate: k_carb(" << num(runnerCfm, 1) << "),\n";
    o << "        runner_length: 12.0 * units.inch,\n";
    o << "        idle_flow_rate: k_carb(0.0),\n";
    o << "        idle_throttle_plate_position: " << num(idlePlate, 5) << ",\n";
    o << "        velocity_decay: 0.5\n";
    o << "    )\n\n";

    o << "    exhaust_system_parameters es_params(\n";
    o << "        outlet_flow_rate: k_carb(" << num(outletCfm, 1) << "),\n";
    o << "        primary_tube_length: 20.0 * units.inch,\n";
    o << "        primary_flow_rate: k_carb(" << num(primaryCfm, 1) << "),\n";
    o << "        velocity_decay: 1.0\n";
    o << "    )\n\n";

    for (int b = 0; b < bankCount; ++b) {
        o << "    exhaust_system exhaust" << b << "(\n";
        o << "        es_params,\n";
        o << "        audio_volume: " << num(audioVolume, 3) << ",\n";
        o << "        length: " << (b == 0 ? 100 : 124) << " * units.inch,\n";
        o << "        impulse_response: ir_lib." << irName << "\n";
        o << "    )\n";
    }
    o << "\n";

    o << "    cylinder_bank_parameters bank_params(\n"
        "        bore: bore,\n"
        "        deck_height: stroke / 2 + rod_length + compression_height\n"
        "    )\n\n";

    for (int b = 0; b < bankCount; ++b) {
        o << "    cylinder_bank b" << b << "(bank_params, angle: " << num(bankAngle[b], 3) << " * units.deg)\n";
    }

    for (int b = 0; b < bankCount; ++b) {
        o << "    b" << b << "\n";
        int indexInBank = 0;
        for (const Cylinder &c : cylinders) {
            if (c.bank != b) continue;
            o << "        .add_cylinder(\n";
            o << "            piston: piston(piston_params, blowby: k_28inH2O(0.0)),\n";
            o << "            connecting_rod: connecting_rod(cr_params),\n";
            o << "            rod_journal: rj" << c.number << ",\n";
            o << "            intake: intake,\n";
            o << "            exhaust_system: exhaust" << b << ",\n";
            o << "            ignition_wire: wires.wire" << c.number << ",\n";
            o << "            sound_attenuation: 1.0,\n";
            o << "            primary_length: " << num(2.0 + 1.5 * (indexInBank % 4), 2) << " * units.inch\n";
            o << "        )\n";
            ++indexInBank;
        }
        o << "        .set_cylinder_head(\n";
        o << "            builder_head(\n";
        o << "                intake_camshaft: intake_cam_" << b << ",\n";
        o << "                exhaust_camshaft: exhaust_cam_" << b << ",\n";
        o << "                chamber_volume: " << num(chamberCc, 2) << " * units.cc,\n";
        o << "                intake_runner_volume: " << num(intakeRunnerCc, 2) << " * units.cc,\n";
        o << "                intake_runner_cross_section_area: "
            << num(intakeRunnerInch, 3) << " * units.inch * " << num(intakeRunnerInch, 3) << " * units.inch,\n";
        o << "                exhaust_runner_volume: " << num(exhaustRunnerCc, 2) << " * units.cc,\n";
        o << "                exhaust_runner_cross_section_area: "
            << num(exhaustRunnerInch, 3) << " * units.inch * " << num(exhaustRunnerInch, 3) << " * units.inch,\n";
        o << "                flow_attenuation: " << num(flowScale, 4) << ",\n";
        o << "                lift_scale: " << num(liftScale, 4) << ",\n";
        o << "                flip_display: " << (b == 1 ? "true" : "false") << "\n";
        o << "            )\n";
        o << "        )\n";
    }
    o << "\n";

    o << "    engine\n";
    for (int b = 0; b < bankCount; ++b) {
        o << "        .add_cylinder_bank(b" << b << ")\n";
    }
    o << "\n";
    o << "    engine.add_crankshaft(c0)\n\n";

    // Camshafts
    o << "    harmonic_cam_lobe intake_lobe(\n";
    o << "        duration_at_50_thou: " << num(camDuration, 1) << " * units.deg,\n";
    o << "        gamma: 1.1,\n";
    o << "        lift: " << num(camLiftThou * liftScale, 1) << " * units.thou,\n";
    o << "        steps: 256\n";
    o << "    )\n\n";
    o << "    harmonic_cam_lobe exhaust_lobe(\n";
    o << "        duration_at_50_thou: " << num(camDuration + 2.0, 1) << " * units.deg,\n";
    o << "        gamma: 1.1,\n";
    o << "        lift: " << num(camLiftThou * liftScale, 1) << " * units.thou,\n";
    o << "        steps: 256\n";
    o << "    )\n\n";

    o << "    camshaft_parameters cam_params(\n";
    o << "        advance: 0 * units.deg,\n";
    o << "        base_radius: " << num(0.75 * liftScale, 3) << " * units.inch\n";
    o << "    )\n\n";

    for (int b = 0; b < bankCount; ++b) {
        o << "    camshaft intake_cam_" << b << "(cam_params, lobe_profile: intake_lobe)\n";
        o << "    camshaft exhaust_cam_" << b << "(cam_params, lobe_profile: exhaust_lobe)\n";
    }
    o << "\n";

    for (int b = 0; b < bankCount; ++b) {
        o << "    exhaust_cam_" << b << "\n";
        for (const Cylinder &c : cylinders) {
            if (c.bank != b) continue;
            o << "        .add_lobe((360 - " << num(lobeCenter, 1) << " + " << num(c.firingAngle, 3) << ") * units.deg)\n";
        }
        o << "    intake_cam_" << b << "\n";
        for (const Cylinder &c : cylinders) {
            if (c.bank != b) continue;
            o << "        .add_lobe((360 + " << num(lobeCenter, 1) << " + " << num(c.firingAngle, 3) << ") * units.deg)\n";
        }
    }
    o << "\n";

    // Ignition
    const double rpmScale = rpmRatio;
    o << "    function timing_curve(" << num(1000 * rpmScale, 1) << " * units.rpm)\n";
    o << "    timing_curve\n";
    const double timing[] = { 12, 12, 20, 30, 38, 38, 38, 38, 38 };
    for (int i = 0; i < 9; ++i) {
        o << "        .add_sample(" << num(i * 1000 * rpmScale, 1) << " * units.rpm, " << num(timing[i], 1) << " * units.deg)\n";
    }
    o << "\n";

    o << "    ignition_module ignition_module(\n";
    o << "        timing_curve: timing_curve,\n";
    o << "        rev_limit: " << (s.redlineRpm + 300) << " * units.rpm,\n";
    o << "        limiter_duration: " << (n <= 4 ? "0.1" : "0.2") << ")\n";
    o << "    ignition_module\n";
    for (size_t k = 0; k < order.size(); ++k) {
        const Cylinder &c = cylinders[order[k] - 1];
        o << "        .connect_wire(wires.wire" << c.number << ", " << num(c.firingAngle, 3) << " * units.deg)\n";
    }
    o << "\n";
    o << "    engine.add_ignition_module(ignition_module)\n";
    o << "}\n\n";

    // Vehicle & transmission
    o << "private node builder_vehicle {\n"
        "    alias output __out:\n"
        "        vehicle(\n"
        "            mass: 1400 * units.kg,\n"
        "            drag_coefficient: 0.3,\n"
        "            cross_sectional_area: (72 * units.inch) * (50 * units.inch),\n"
        "            diff_ratio: 3.42,\n"
        "            tire_radius: 10 * units.inch,\n"
        "            rolling_resistance: 200 * units.N\n"
        "        );\n"
        "}\n\n";

    o << "private node builder_transmission {\n";
    o << "    alias output __out:\n";
    o << "        transmission(\n";
    o << "            max_clutch_torque: " << num(clutchTorque, 1) << " * units.lb_ft\n";
    o << "        )\n";
    o << "        .add_gear(2.97)\n";
    o << "        .add_gear(2.07)\n";
    o << "        .add_gear(1.43)\n";
    o << "        .add_gear(1.00)\n";
    o << "        .add_gear(0.84)\n";
    o << "        .add_gear(0.56);\n";
    o << "}\n\n";

    o << "public node main {\n";
    o << "    set_engine(custom_engine())\n";
    o << "    set_vehicle(builder_vehicle())\n";
    o << "    set_transmission(builder_transmission())\n";
    o << "}\n";

    return o.str();
}
