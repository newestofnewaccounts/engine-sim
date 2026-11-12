#include "../include/wav_recorder.h"

#include <filesystem>

namespace fs = std::filesystem;

namespace {
    void write16(std::ofstream &f, uint16_t v) {
        const char b[2] = { (char)(v & 0xFF), (char)((v >> 8) & 0xFF) };
        f.write(b, 2);
    }

    void write32(std::ofstream &f, uint32_t v) {
        const char b[4] = {
            (char)(v & 0xFF), (char)((v >> 8) & 0xFF),
            (char)((v >> 16) & 0xFF), (char)((v >> 24) & 0xFF) };
        f.write(b, 4);
    }
}

WavRecorder::WavRecorder() {
    m_sampleRate = 44100;
    m_samples = 0;
}

WavRecorder::~WavRecorder() {
    stop();
}

void WavRecorder::writeHeader(uint32_t dataBytes) {
    m_file.write("RIFF", 4);
    write32(m_file, 36 + dataBytes);
    m_file.write("WAVE", 4);
    m_file.write("fmt ", 4);
    write32(m_file, 16);                     // fmt chunk size
    write16(m_file, 1);                      // PCM
    write16(m_file, 1);                      // mono
    write32(m_file, (uint32_t)m_sampleRate);
    write32(m_file, (uint32_t)m_sampleRate * 2); // byte rate
    write16(m_file, 2);                      // block align
    write16(m_file, 16);                     // bits per sample
    m_file.write("data", 4);
    write32(m_file, dataBytes);
}

bool WavRecorder::start(const std::string &path, int sampleRate) {
    stop();

    std::error_code ec;
    fs::create_directories(fs::path(path).parent_path(), ec);

    m_file.open(path, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!m_file.is_open()) return false;

    m_path = path;
    m_sampleRate = sampleRate;
    m_samples = 0;
    writeHeader(0);
    return true;
}

void WavRecorder::addSamples(const int16_t *samples, int count) {
    if (!m_file.is_open() || count <= 0) return;

    for (int i = 0; i < count; ++i) {
        write16(m_file, (uint16_t)samples[i]);
    }

    m_samples += (uint32_t)count;
}

std::string WavRecorder::stop() {
    if (!m_file.is_open()) return "";

    // Go back and fill in the sizes now that the length is known
    m_file.seekp(0, std::ios::beg);
    writeHeader(m_samples * 2);
    m_file.close();

    return m_path;
}

double WavRecorder::getSeconds() const {
    return (m_sampleRate > 0) ? (double)m_samples / m_sampleRate : 0.0;
}

std::string WavRecorder::safeFileName(const std::string &name) {
    std::string out;
    for (char c : name) {
        if (c == '<' || c == '>' || c == ':' || c == '"' || c == '/' || c == '\\'
            || c == '|' || c == '?' || c == '*' || (unsigned char)c < 32)
        {
            out += '-';
        }
        else {
            out += c;
        }
    }

    while (!out.empty() && (out.back() == ' ' || out.back() == '.')) out.pop_back();
    return out.empty() ? "recording" : out;
}

std::string WavRecorder::uniquePath(const std::string &folder, const std::string &name) {
    const std::string base = safeFileName(name);
    std::error_code ec;

    fs::path candidate = fs::path(folder) / (base + ".wav");
    for (int i = 2; fs::exists(candidate, ec) && i < 1000; ++i) {
        candidate = fs::path(folder) / (base + " (" + std::to_string(i) + ").wav");
    }

    return candidate.string();
}
