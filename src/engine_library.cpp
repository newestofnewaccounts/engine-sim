#include "../include/engine_library.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <sstream>

namespace fs = std::filesystem;

namespace {
    std::string toForwardSlashes(std::string s) {
        std::replace(s.begin(), s.end(), '\\', '/');
        return s;
    }

    std::string lower(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
        return s;
    }

    const std::map<std::string, std::string> &knownNames() {
        static const std::map<std::string, std::string> names = {
            { "engines/atg-video-1/01_honda_trx520.mr", "Honda TRX520 ATV (single cylinder)" },
            { "engines/atg-video-1/02_kohler_ch750.mr", "Kohler CH750 (V-twin lawn engine)" },
            { "engines/atg-video-1/03_harley_davidson_shovelhead.mr", "Harley-Davidson Shovelhead (V-twin)" },
            { "engines/atg-video-1/04_hayabusa.mr", "Suzuki Hayabusa (inline-4 motorbike)" },
            { "engines/atg-video-1/05_honda_vtec.mr", "Honda B18C5 VTEC (inline-4)" },
            { "engines/atg-video-1/06_subaru_ej25.mr", "Subaru EJ25 (flat-4, early version)" },
            { "engines/atg-video-1/07_audi_i5.mr", "Audi 2.3 (inline-5)" },
            { "engines/atg-video-1/08_radial_5.mr", "Radial 5 (aircraft)" },
            { "engines/atg-video-2/01_subaru_ej25_eh.mr", "Subaru EJ25 (flat-4, equal headers)" },
            { "engines/atg-video-2/02_subaru_ej25_uh.mr", "Subaru EJ25 (flat-4, unequal headers rumble)" },
            { "engines/atg-video-2/03_2jz.mr", "Toyota 2JZ (inline-6)" },
            { "engines/atg-video-2/04_60_degree_v6.mr", "60-degree V6" },
            { "engines/atg-video-2/05_odd_fire_v6.mr", "Odd-fire V6" },
            { "engines/atg-video-2/06_even_fire_v6.mr", "Even-fire V6" },
            { "engines/atg-video-2/07_gm_ls.mr", "GM LS (V8)" },
            { "engines/atg-video-2/08_ferrari_f136_v8.mr", "Ferrari F136 (flat-plane V8)" },
            { "engines/atg-video-2/09_radial_9.mr", "Radial 9 (aircraft)" },
            { "engines/atg-video-2/10_lfa_v10.mr", "Lexus LFA 1LR-GUE (V10)" },
            { "engines/atg-video-2/11_merlin_v12.mr", "Rolls-Royce Merlin (V12 aircraft)" },
            { "engines/atg-video-2/12_ferrari_412_t2.mr", "Ferrari 412 T2 (Formula 1 V12)" },
            { "engines/audi/i5.mr", "Audi 2.2 (inline-5)" },
            { "engines/bmw/M52B28.mr", "BMW M52B28 (inline-6)" },
            { "engines/chevrolet/chev_truck_454.mr", "Chevrolet 454 (big-block V8)" },
            { "engines/kohler/kohler_ch750.mr", "Kohler CH750 (V-twin, newer version)" }
        };

        return names;
    }
}

std::string EngineLibrary::readFile(const std::string &path) {
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file.is_open()) return "";

    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

std::string EngineLibrary::firstLine(const std::string &path) {
    std::ifstream file(path, std::ios::in);
    std::string line;
    if (file.is_open()) std::getline(file, line);
    if (!line.empty() && line.back() == '\r') line.pop_back();
    return line;
}

std::string EngineLibrary::readScriptName(const std::string &absolutePath) {
    const std::string contents = readFile(absolutePath);
    static const std::regex nameRegex("name\\s*:\\s*\"([^\"]*)\"");

    std::smatch match;
    if (std::regex_search(contents, match, nameRegex)) {
        return match[1].str();
    }

    return "";
}

std::string EngineLibrary::friendlyName(const std::string &relativePath, const std::string &scriptName) {
    const auto &names = knownNames();
    auto it = names.find(relativePath);
    if (it != names.end()) return it->second;

    if (!scriptName.empty()) return scriptName;

    return fs::path(relativePath).stem().string();
}

std::vector<EngineEntry> EngineLibrary::scan(const std::string &assetsDir) {
    std::vector<EngineEntry> builtIn, custom;

    std::error_code ec;
    const fs::path root = fs::path(assetsDir);
    const fs::path enginesDir = root / "engines";
    if (!fs::exists(enginesDir, ec)) return {};

    for (fs::recursive_directory_iterator it(enginesDir, ec), end; it != end; it.increment(ec)) {
        if (ec) break;
        if (!it->is_regular_file(ec)) continue;
        if (lower(it->path().extension().string()) != ".mr") continue;

        const std::string contents = readFile(it->path().string());
        if (contents.find("public node main") == std::string::npos) continue;

        EngineEntry entry;
        entry.relativePath = toForwardSlashes(fs::relative(it->path(), root, ec).string());
        entry.custom = entry.relativePath.rfind(std::string(CustomFolder) + "/", 0) == 0;
        entry.name = friendlyName(entry.relativePath, readScriptName(it->path().string()));

        if (entry.custom) custom.push_back(entry);
        else builtIn.push_back(entry);
    }

    auto byName = [](const EngineEntry &a, const EngineEntry &b) {
        return lower(a.name) < lower(b.name);
    };
    std::sort(builtIn.begin(), builtIn.end(), byName);
    std::sort(custom.begin(), custom.end(), byName);

    std::vector<EngineEntry> result = custom;
    result.insert(result.end(), builtIn.begin(), builtIn.end());
    return result;
}

std::string EngineLibrary::writeSessionScript(
    const std::string &assetsDir,
    const std::string &engineRelativePath,
    const std::string &themeRelativePath,
    std::string *error)
{
    const fs::path sessionPath = fs::path(assetsDir) / SessionFile;

    std::ofstream file(sessionPath, std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        if (error != nullptr) {
            *error = "Could not write " + sessionPath.string() + ". Is the folder read-only?";
        }
        return "";
    }

    file << "// Written by Engine Simulator Simple Edition each time an engine is loaded.\n";
    file << "import \"engine_sim.mr\"\n";
    file << "import \"" << toForwardSlashes(themeRelativePath) << "\"\n";
    file << "import \"" << toForwardSlashes(engineRelativePath) << "\"\n\n";
    file << "use_default_theme()\n";
    file << "main()\n";
    file.close();

    return sessionPath.string();
}

std::string EngineLibrary::saveCustomEngine(
    const std::string &assetsDir,
    const std::string &fileStem,
    const std::string &script,
    std::string *error)
{
    std::error_code ec;
    const fs::path folder = fs::path(assetsDir) / CustomFolder;
    fs::create_directories(folder, ec);

    const fs::path path = folder / (fileStem + ".mr");
    std::ofstream file(path, std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        if (error != nullptr) *error = "Could not save " + path.string();
        return "";
    }

    file << script;
    file.close();

    return std::string(CustomFolder) + "/" + fileStem + ".mr";
}

std::string EngineLibrary::summarizeErrorLog(const std::string &logPath) {
    std::ifstream file(logPath);
    if (!file.is_open()) return "The engine script could not be loaded.";

    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.find_first_not_of(" \t") == std::string::npos) continue;

        // Trim leading whitespace
        line = line.substr(line.find_first_not_of(" \t"));
        if (line.size() > 110) line = line.substr(0, 107) + "...";
        return line;
    }

    return "The engine script did not create an engine.";
}
