#ifndef ATG_ENGINE_SIM_APP_SETTINGS_STORE_H
#define ATG_ENGINE_SIM_APP_SETTINGS_STORE_H

#include <map>
#include <string>

// Simple Edition: remembers a few choices between sessions in a small
// "key=value" text file next to the program.
class AppSettingsStore {
public:
    void load(const std::string &path);
    bool save() const;

    std::string get(const std::string &key, const std::string &fallback = "") const;
    int getInt(const std::string &key, int fallback) const;
    double getDouble(const std::string &key, double fallback) const;
    bool getBool(const std::string &key, bool fallback) const;

    void set(const std::string &key, const std::string &value);
    void setInt(const std::string &key, int value);
    void setDouble(const std::string &key, double value);
    void setBool(const std::string &key, bool value);

private:
    std::string m_path;
    std::map<std::string, std::string> m_values;
};

#endif /* ATG_ENGINE_SIM_APP_SETTINGS_STORE_H */
