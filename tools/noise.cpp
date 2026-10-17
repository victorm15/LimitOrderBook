#include "../src/benchmarking/benchmark.hpp"
#include "../src/benchmarking/stream_io.hpp"
#include "../src/books/array_book.hpp"
#include "../src/books/sorted_vec_book.hpp"
#include "../src/books/hash_book.hpp"
#include "../src/books/bitmap_book.hpp"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

struct Spread {
    uint32_t min = 0, max = 0, median = 0;
    double   noise_pct = 0.0;
};

static Spread spread_of(std::vector<BenchTimestamps>& ts, uint32_t BenchTimestamps::* field) {
    Spread s;
    if (ts.empty()) return s;
    std::vector<uint32_t> v;
    v.reserve(ts.size());
    for (const BenchTimestamps& t : ts) v.push_back(t.*field);
    std::sort(v.begin(), v.end());
    s.min    = v.front();
    s.max    = v.back();
    s.median = v[v.size() / 2];
    s.noise_pct = s.median ? 100.0 * double(s.max - s.min) / double(s.median) : 0.0;
    return s;
}

static void report_stat(const char* label, const std::vector<BenchTimestamps>& ts,
                        uint32_t BenchTimestamps::* field) {
    Spread s = spread_of(ts, field);
    std::cout << "  " << std::left << std::setw(7) << label << std::right
              << "  median=" << std::setw(7) << s.median
              << "  min="    << std::setw(7) << s.min
              << "  max="    << std::setw(7) << s.max
              << "  noise="  << std::fixed << std::setprecision(2)
              << std::setw(6) << s.noise_pct << "%\n";
}

static void report_book(const std::string& name, const std::vector<BenchTimestamps>& ts) {
    std::cout << "\n" << name << "  (" << ts.size() << " runs)\n";
    report_stat("mean",  ts, &BenchTimestamps::mean);
    report_stat("p50",   ts, &BenchTimestamps::p50);
    report_stat("p99",   ts, &BenchTimestamps::p99);
    report_stat("p99.9", ts, &BenchTimestamps::p999);
    report_stat("max",   ts, &BenchTimestamps::max);
}

int main(int argc, char** argv) {
    const int         trials      = argc > 1 ? std::atoi(argv[1]) : 100;
    const std::string stream_name = argc > 2 ? argv[2] : "stream0";
    const std::string path        = "streams/" + stream_name;

    std::vector<EventRecord> events = read_stream(path + ".bin");
    prefault(events);
    auto [stream_cfg, stream_m] = read_meta(path + ".json");

    BookConfig book_cfg{
        .base_price       = stream_m.lowest_price,
        .band_ticks       = stream_m.highest_price - stream_m.lowest_price + 1,
        .arena_size       = stream_m.max_live_orders + 50,
        .number_of_levels = size_t(stream_m.highest_price - stream_m.lowest_price)
    };

    PmcCycles pmc;
    pmc.open();
    const uint32_t timer_overhead = measure_timer_overhead(pmc);

    std::vector<uint32_t> book_timings(stream_cfg.n_events - N_WARM, 0);

    std::vector<BenchTimestamps> array_ts, sorted_vec_ts, hash_ts, bitmap_ts;
    array_ts.reserve(trials); sorted_vec_ts.reserve(trials);
    hash_ts.reserve(trials);  bitmap_ts.reserve(trials);

    for (int i = 0; i < trials; ++i) {
        std::cout << "run " << i + 1 << "/" << trials << "\n" << std::flush;
        
        array_ts.push_back(
            benchmark<ArrayBook>(book_cfg, events, book_timings, stream_cfg.n_events, timer_overhead, pmc, 1));
        sorted_vec_ts.push_back(
            benchmark<SortedVecBook>(book_cfg, events, book_timings, stream_cfg.n_events, timer_overhead, pmc, 1));
        hash_ts.push_back(
            benchmark<HashBook>(book_cfg, events, book_timings, stream_cfg.n_events, timer_overhead, pmc, 1));
        bitmap_ts.push_back(
            benchmark<BitmapBook>(book_cfg, events, book_timings, stream_cfg.n_events, timer_overhead, pmc, 1));
    }

    report_book("ArrayBook",     array_ts);
    report_book("SortedVecBook", sorted_vec_ts);
    report_book("HashBook",      hash_ts);
    report_book("BitmapBook",    bitmap_ts);

    return 0;
}