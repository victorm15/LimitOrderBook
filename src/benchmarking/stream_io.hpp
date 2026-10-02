#ifndef STREAM_IO_H
#define STREAM_IO_H

#include <vector>
#include <fstream>
#include <stdexcept>
#include "events.hpp"


inline void prefault(std::vector<EventRecord>& events) {
    volatile uint8_t sink = 0;
    const uint8_t* p = reinterpret_cast<const uint8_t*>(events.data());
    for (size_t i = 0; i < events.size() * sizeof(EventRecord); i += 4096) sink += p[i];
}

static_assert(std::is_trivially_copyable_v<EventRecord>);

struct StreamHeader {
    char     magic[8] = {'L','O','B','S','T','R','M','1'};
    uint32_t record_size = sizeof(EventRecord);
    uint32_t _pad = 0;
    uint64_t count = 0;
};

inline void write_stream(const std::string& path, const std::vector<EventRecord>& evs) {
    std::ofstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("cannot open " + path);
    StreamHeader h;
    h.count = evs.size();
    f.write(reinterpret_cast<const char*>(&h), sizeof(h));
    f.write(reinterpret_cast<const char*>(evs.data()), evs.size() * sizeof(EventRecord));
    if (!f) throw std::runtime_error("write failed: " + path);
}

inline std::vector<EventRecord> read_stream(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("cannot open " + path);

    StreamHeader h;
    f.read(reinterpret_cast<char*>(&h), sizeof(h));
    if (std::memcmp(h.magic, "LOBSTRM1", 8) != 0) throw std::runtime_error("bad magic: " + path);
    if (h.record_size != sizeof(EventRecord))
        throw std::runtime_error("record size mismatch: file " + std::to_string(h.record_size) +
                                 ", build " + std::to_string(sizeof(EventRecord)));

    std::vector<EventRecord> evs(h.count);
    f.read(reinterpret_cast<char*>(evs.data()), h.count * sizeof(EventRecord));
    if (size_t(f.gcount()) != h.count * sizeof(EventRecord)) throw std::runtime_error("truncated: " + path);
    return evs;
}

#endif