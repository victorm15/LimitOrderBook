#include "../src/benchmarking/benchmark.hpp"
#include "../src/benchmarking/stream_io.hpp"
#include "../src/books/array_book.hpp"
#include "../src/books/sorted_vec_book.hpp"
#include "../src/books/hash_book.hpp"
#include "../src/books/bitmap_book.hpp"
#include <string>
#include <numeric>


int main(int argc, char** argv) {
    
    std::string stream_name = argc > 1 ? argv[1] : "stream0";
    size_t n_warm = argc > 2 ? std::atoi(argv[2]) : 100'000;

    std::vector<EventRecord> events = read_stream("streams/" + stream_name + ".bin");
    prefault(events);

    auto [stream_cfg, stream_m] = read_meta("streams/" + stream_name + ".json");
    BookConfig book_cfg{.base_price = stream_m.lowest_price, .band_ticks = stream_m.highest_price - stream_m.lowest_price + 1,
        .arena_size = stream_m.max_live_orders + 50, 
        .number_of_levels = size_t(stream_m.highest_price - stream_m.lowest_price) 
        };
    std::cout << "Benchmarking books on " << stream_name << ":\n";
    std::vector<uint32_t> book_timings(stream_cfg.n_events-n_warm, 0);
    PmcCycles pmc;
    pmc.open();
    uint32_t timer_overhead = measure_timer_overhead(pmc);
    std::cout << "Timer Overhead: " << timer_overhead << "\n";

    ArrayBook array_book(book_cfg);
    benchmark(array_book, "ArrayBook", events, book_timings, n_warm, stream_cfg.n_events, timer_overhead, pmc);
    SortedVecBook sorted_vec_book(book_cfg);
    benchmark(sorted_vec_book, "SortedVecBook", events, book_timings, n_warm, stream_cfg.n_events, timer_overhead, pmc);
    HashBook hash_book(book_cfg);
    benchmark(hash_book, "HashBook", events, book_timings, n_warm, stream_cfg.n_events, timer_overhead, pmc);
    BitmapBook bitmap_book(book_cfg);
    benchmark(bitmap_book, "BitmapBook", events, book_timings, n_warm, stream_cfg.n_events, timer_overhead, pmc);
    
    return 0;
}