#ifndef STREAM_IO_H
#define STREAM_IO_H

#include <vector>
#include <fstream>
#include <stdexcept>
#include <string>
#include "../core/events.hpp"
#include "../core/types.hpp"





inline void write_meta(const std::string& path, const StreamConfig& cfg, const StreamMeasurements& m) {
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());
    std::ofstream f(path);
    if (!f) throw std::runtime_error("cannot open " + path);
    f << "{\n";
    f << "  \"config\": {\n";
    f << "    \"seed\": "                 << cfg.seed              << ",\n";
    f << "    \"n_events\": "             << cfg.n_events          << ",\n";
    f << "    \"p_add\": "                << cfg.p_add             << ",\n";
    f << "    \"p_cancel\": "             << cfg.p_cancel          << ",\n";
    f << "    \"p_reduce\": "             << cfg.p_reduce          << ",\n";
    f << "    \"add_dist_lambda\": "      << cfg.add_dist_lambda   << ",\n";
    f << "    \"cancel_dist_lambda\": "   << cfg.cancel_dist_lambda<< ",\n";
    f << "    \"dist_scale_factor\": "    << cfg.dist_scale_factor << ",\n";
    f << "    \"occupied_level_range\": " << cfg.occupied_level_range << "\n";
    f << "  },\n";
    f << "  \"measured\": {\n";
    f << "    \"level_creations\": "              << m.level_creations              << ",\n";
    f << "    \"level_deletions\": "              << m.level_deletions              << ",\n";
    f << "    \"fills_per_add\": "                << m.fills_per_add                << ",\n";
    f << "    \"median_gap\": "                   << m.average_gap                  << ",\n";
    f << "    \"median_occupied_levels\": "       << m.average_occupied_levels      << ",\n";
    f << "    \"median_creation_dist\": "         << m.avg_level_construction_dist  << ",\n";
    f << "    \"median_destruction_dist\": "      << m.avg_level_destruction_dist   << ",\n";
    f << "    \"max_live_orders\": "              << m.max_live_orders              << ",\n";
    f << "    \"lowest_price\": "                 << m.lowest_price                 << ",\n";
    f << "    \"highest_price\": "                << m.highest_price                << "\n";
    f << "  }\n";
    f << "}\n";
    f.close();
    if (!f) throw std::runtime_error("close failed: " + path);
}

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
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());
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