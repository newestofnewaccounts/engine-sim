#include "../include/app_settings_store.h"

#include <fstream>
#include <locale>
#include <sstream>

void AppSettingsStore::load(const std::string &path) {
    m_path = path;
    m_values.clear();

    std::ifstream file(path);
    if (!file.is_open()) return;

    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;

        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;

        m_values[line.substr(0, eq)] = line.substr(eq + 1);
    }
}

bool AppSettingsStore::save() const {
    if (m_path.empty()) return false;

    std::ofstream file(m_path, std::ios::out | std::ios::trunc);
    if (!file.is_open()) return false;

    file << "# Engine Simulator Simple Edition settings\n";
    for (const auto &kv : m_values) {
        file << kv.first << "=" << kv.second << "\n";
    }

    return true;
}

std::string AppSettingsStore::get(const std::string &key, const std::string &fallback) const {
    auto it = m_values.find(key);
    return (it != m_values.end()) ? it->second : fallback;
}

int AppSettingsStore::getInt(const std::string &key, int fallback) const {
    try {
        auto it = m_values.find(key);
        return (it != m_values.end()) ? std::stoi(it->second) : fallback;
    }
    catch (...) {
        return fallback;
    }
}

double AppSettingsStore::getDouble(const std::string &key, double fallback) const {
    auto it = m_values.find(key);
    if (it == m_values.end()) return fallback;

    std::istringstream ss(it->second);
    ss.imbue(std::locale::classic());
    double value = fallback;
    if (!(ss >> value)) return fallback;
    return value;
}

bool AppSettingsStore::getBool(const std::string &key, bool fallback) const {
    auto it = m_values.find(key);
    if (it == m_values.end()) return fallback;
    return it->second == "1" || it->second == "true";
}

void AppSettingsStore::set(const std::string &key, const std::string &value) {
    m_values[key] = value;
}

void AppSettingsStore::setInt(const std::string &key, int value) {
    m_values[key] = std::to_string(value);
}

void AppSettingsStore::setDouble(const std::string &key, double value) {
    std::ostringstream ss;
    ss.imbue(std::locale::classic());
    ss << value;
    m_values[key] = ss.str();
}

void AppSettingsStore::setBool(const std::string &key, bool value) {
    m_values[key] = value ? "1" : "0";
}
