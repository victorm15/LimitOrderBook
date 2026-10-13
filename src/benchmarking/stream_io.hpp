#ifndef STREAM_IO_H
#define STREAM_IO_H

#include <vector>
#include <fstream>
#include <sstream>
#include <utility>
#include <stdexcept>
#include <string>
#include <filesystem>
#include "../core/events.hpp"
#include "../core/types.hpp"





inline void write_meta(const std::string& path, const StreamConfig& cfg, const StreamMeasurements& m) {
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());
    std::ofstream f(path);
    if (!f) throw std::runtime_error("cannot open " + path);
    f << "{\n";
    f << "  \"config\": {\n";
    f << "    \"seed\": "                 << cfg.seed                 << ",\n";
    f << "    \"n_events\": "             << cfg.n_events             << ",\n";
    f << "    \"p_add\": "                << cfg.p_add                << ",\n";
    f << "    \"p_cancel\": "             << cfg.p_cancel             << ",\n";
    f << "    \"p_reduce\": "             << cfg.p_reduce             << ",\n";
    f << "    \"add_dist_lambda\": "      << cfg.add_dist_lambda      << ",\n";
    f << "    \"cancel_dist_lambda\": "   << cfg.cancel_dist_lambda   << ",\n";
    f << "    \"dist_scale_factor\": "    << cfg.dist_scale_factor    << ",\n";
    f << "    \"occupied_level_range\": " << cfg.occupied_level_range << ",\n";
    f << "    \"p_join\": "               << cfg.p_join               << ",\n";
    f << "    \"p_improve\": "            << cfg.p_improve            << ",\n";
    f << "    \"p_behind\": "             << cfg.p_behind             << "\n";
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
    f << "    \"highest_price\": "                << m.highest_price                << ",\n";
    f << "    \"bbos_deleted\": "                 << m.bbos_deleted                 << "\n";
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











inline std::string field(const std::string& text, const std::string& key,
                         const std::string& path) {
    const std::string pat = "\"" + key + "\"";
    size_t k = text.find(pat);
    if (k == std::string::npos)
        throw std::runtime_error("missing key \"" + key + "\" in " + path);
 
    size_t colon = text.find(':', k + pat.size());
    if (colon == std::string::npos)
        throw std::runtime_error("malformed entry for \"" + key + "\" in " + path);
 
    size_t end = text.find_first_of(",}\n", colon + 1);
    if (end == std::string::npos) end = text.size();
 
    std::string v = text.substr(colon + 1, end - colon - 1);
    // trim whitespace
    size_t a = v.find_first_not_of(" \t\r\n");
    size_t b = v.find_last_not_of(" \t\r\n");
    if (a == std::string::npos)
        throw std::runtime_error("empty value for \"" + key + "\" in " + path);
    return v.substr(a, b - a + 1);
}


template <typename T>
T number(const std::string& text, const std::string& key, const std::string& path) {
    const std::string v = field(text, key, path);
    std::istringstream is(v);
    T out{};
    is >> out;
    if (!is || is.peek() != std::char_traits<char>::eof())
        throw std::runtime_error("bad number for \"" + key + "\": \"" + v + "\" in " + path);
    return out;
}
 

inline std::pair<StreamConfig, StreamMeasurements> read_meta(const std::string& path) {
    std::ifstream f(path);
    if (!f) throw std::runtime_error("cannot open " + path);
    std::stringstream ss;
    ss << f.rdbuf();
    const std::string text = ss.str();
 

    StreamConfig cfg{};
    cfg.seed                 = number<uint64_t>(text, "seed",                 path);
    cfg.n_events             = number<size_t>  (text, "n_events",             path);
    cfg.p_add                = number<int>     (text, "p_add",                path);
    cfg.p_cancel             = number<int>     (text, "p_cancel",             path);
    cfg.p_reduce             = number<int>     (text, "p_reduce",             path);
    cfg.add_dist_lambda      = number<double>  (text, "add_dist_lambda",      path);
    cfg.cancel_dist_lambda   = number<double>  (text, "cancel_dist_lambda",   path);
    cfg.dist_scale_factor    = number<int>     (text, "dist_scale_factor",    path);
    cfg.occupied_level_range = number<int>     (text, "occupied_level_range", path);
 
    StreamMeasurements m{};
    m.level_creations             = number<uint32_t>(text, "level_creations",          path);
    m.level_deletions             = number<uint32_t>(text, "level_deletions",          path);
    m.fills_per_add               = number<double>  (text, "fills_per_add",            path);
    m.average_gap                 = number<Price>   (text, "median_gap",               path);
    m.average_occupied_levels     = number<size_t>  (text, "median_occupied_levels",   path);
    m.avg_level_construction_dist = number<Price>   (text, "median_creation_dist",     path);
    m.avg_level_destruction_dist  = number<Price>   (text, "median_destruction_dist",  path);
    m.max_live_orders             = number<size_t>  (text, "max_live_orders",          path);
    m.lowest_price                = number<Price>   (text, "lowest_price",             path);
    m.highest_price               = number<Price>   (text, "highest_price",            path);
 
    return {cfg, m};
}

void write_csv_header(std::ofstream& out) {
    out << "Stream Name,StreamConfig,seed,n_events,p_add,p_cancel,p_reduce,"
       "add_dist_lambda,cancel_dist_lambda,dist_scale_factor,occupied_level_range,"
       "p_join,p_behind,p_improve,p_aggressive,StreamMeasurements,"
       "ArrayBook,P50,P90,P99,P99.9,"
       "BitmapBook,P50,P90,P99,P99.9,"
       "HashBook,P50,P90,P99,P99.9,"
       "SortedVecBook,P50,P90,P99,P99.9\n";
}

void write_stream_cfg(StreamConfig& cfg, std::ofstream& out) {
    out << ','
    << cfg.seed << ','
    << cfg.n_events << ','
    << cfg.p_add << ','
    << cfg.p_cancel << ','
    << cfg.p_reduce << ','
    << cfg.add_dist_lambda << ','
    << cfg.cancel_dist_lambda << ','
    << cfg.dist_scale_factor << ','
    << cfg.occupied_level_range << ','
    << cfg.p_join << ','
    << cfg.p_behind << ','
    << cfg.p_improve << ','
    << 100 - cfg.p_join - cfg.p_behind - cfg.p_improve << ',';
}

void write_stream_measurements(StreamMeasurements& m, std::ofstream& out) {
    out << ','
    << m.level_deletions << ','
    << m.level_creations << ','
    << m.average_gap << ','
    << m.fills_per_add << ','
    << m.average_occupied_levels << ','
    << m.max_live_orders << ','
    << m.lowest_price << ','
    << m.highest_price << ','
    << m.avg_level_destruction_dist << ','
    << m.avg_level_construction_dist << ','
    << m.bbos_deleted << ',';
}



#endif