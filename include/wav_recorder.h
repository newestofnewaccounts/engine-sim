#ifndef ATG_ENGINE_SIM_WAV_RECORDER_H
#define ATG_ENGINE_SIM_WAV_RECORDER_H

#include <cstdint>
#include <fstream>
#include <string>

// Simple Edition: writes the engine audio straight to a 16-bit mono WAV file.
class WavRecorder {
public:
    WavRecorder();
    ~WavRecorder();

    bool start(const std::string &path, int sampleRate);
    void addSamples(const int16_t *samples, int count);
    // Finishes the file. Returns the path that was written ("" if nothing).
    std::string stop();

    bool isRecording() const { return m_file.is_open(); }
    double getSeconds() const;
    const std::string &getPath() const { return m_path; }

    // Picks "<folder>/<name>.wav", or "<name> (2).wav" etc. if it exists.
    static std::string uniquePath(const std::string &folder, const std::string &name);
    static std::string safeFileName(const std::string &name);

private:
    void writeHeader(uint32_t dataBytes);

    std::ofstream m_file;
    std::string m_path;
    int m_sampleRate;
    uint32_t m_samples;
};

#endif /* ATG_ENGINE_SIM_WAV_RECORDER_H */
