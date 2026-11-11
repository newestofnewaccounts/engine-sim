#ifndef ATG_ENGINE_SIM_ENGINE_BUILDER_H
#define ATG_ENGINE_SIM_ENGINE_BUILDER_H

#include <string>
#include <vector>

// Simple Edition: turns a handful of friendly choices into a complete
// engine script (.mr) that the regular script compiler can load.
struct EngineBuilderSettings {
    enum class Layout { Inline, V, Flat };
    enum class Character { Torquey, Balanced, RevHappy };
    enum class Cam { Mild, Sport, Race };
    enum class Exhaust { Quiet, Sporty, Loud };

    Layout layout = Layout::V;
    int cylinders = 8;
    double displacementLiters = 5.0;
    int redlineRpm = 6500;
    Character character = Character::Balanced;
    Cam cam = Cam::Sport;
    Exhaust exhaust = Exhaust::Sporty;
};

class EngineBuilder {
public:
    using Settings = EngineBuilderSettings;

    static std::vector<int> allowedCylinders(Settings::Layout layout);

    // Clamps every value into a range the generator can handle.
    static void sanitize(Settings *s);

    static double minDisplacement(const Settings &s);
    static double maxDisplacement(const Settings &s);

    static std::string layoutName(Settings::Layout layout);
    static std::string characterName(Settings::Character c);
    static std::string camName(Settings::Cam c);
    static std::string exhaustName(Settings::Exhaust e);

    // e.g. "Custom V8 5.0L"
    static std::string engineName(const Settings &s);
    // e.g. "custom_v8_5_0l"
    static std::string fileStem(const Settings &s);

    static std::string generateScript(const Settings &s);

    // First line of every generated file, so a saved build can be re-opened
    // in the builder.
    static std::string serialize(const Settings &s);
    static bool deserialize(const std::string &line, Settings *s);

    // Firing order for the layout/cylinder count (1-based cylinder numbers).
    static std::vector<int> firingOrder(Settings::Layout layout, int cylinders);

    static void boreStrokeMm(const Settings &s, double *bore, double *stroke);

    // Used by the builder screen: change one setting up or down.
    // Rows: 0 layout, 1 cylinders, 2 size, 3 redline, 4 character, 5 cam, 6 exhaust
    static void adjust(Settings *s, int row, int direction);
};

#endif /* ATG_ENGINE_SIM_ENGINE_BUILDER_H */
