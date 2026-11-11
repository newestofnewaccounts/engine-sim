#ifndef ATG_ENGINE_SIM_ENGINE_LIBRARY_H
#define ATG_ENGINE_SIM_ENGINE_LIBRARY_H

#include <string>
#include <vector>

// Simple Edition: finds the engine scripts that ship with the app (and the
// ones made with the engine builder) and gives them friendly names.
struct EngineEntry {
    std::string relativePath; // relative to the assets folder, e.g. "engines/bmw/M52B28.mr"
    std::string name;         // friendly name shown in the picker
    bool custom = false;      // made with the engine builder
};

class EngineLibrary {
public:
    static constexpr const char *CustomFolder = "engines/custom";
    static constexpr const char *SessionFile = "simple_session.mr";
    static constexpr const char *DefaultTheme = "themes/default.mr";

    static std::vector<EngineEntry> scan(const std::string &assetsDir);
    static std::string friendlyName(const std::string &relativePath, const std::string &scriptName);
    static std::string readScriptName(const std::string &absolutePath);

    // Writes <assets>/simple_session.mr which imports the theme and the engine
    // and runs it. Returns the absolute path of the session file.
    static std::string writeSessionScript(
        const std::string &assetsDir,
        const std::string &engineRelativePath,
        const std::string &themeRelativePath,
        std::string *error);

    // Saves a generated engine into engines/custom and returns its relative path.
    static std::string saveCustomEngine(
        const std::string &assetsDir,
        const std::string &fileStem,
        const std::string &script,
        std::string *error);

    static std::string readFile(const std::string &path);
    static std::string firstLine(const std::string &path);

    // Turns the script compiler's error log into one short, readable message.
    static std::string summarizeErrorLog(const std::string &logPath);
};

#endif /* ATG_ENGINE_SIM_ENGINE_LIBRARY_H */
