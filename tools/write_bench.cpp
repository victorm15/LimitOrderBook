#include "../src/benchmarking/benchmark.hpp"
#include "../src/benchmarking/stream_io.hpp"
#include "../src/books/array_book.hpp"
#include "../src/books/sorted_vec_book.hpp"
#include "../src/books/hash_book.hpp"
#include "../src/books/bitmap_book.hpp"
#include <string>
#include <numeric>
#include <fstream>


int main(int argc, char** argv) {
    
    std::string stream_name = argc > 1 ? argv[1] : "stream0";

    if (stream_name == "stream_lambda_sf_grid") {
        std::ofstream out("streams/stream_lambda_sf_grid.csv");
        if (!out) throw std::runtime_error("cannot open stream_lambda_sf_grid.csv");

        write_csv_header(out);
        std::cout << "Benching " << stream_name << "\n";

        size_t n_warm = argc > 2 ? std::atoi(argv[2]) : 100'000;

        std::vector<double> dist_lambdas = {0.0, 0.01, 0.05, 0.1, 0.2, 0.3, 0.4, 0.5};
        std::vector<int> scale_factors = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 20, 50, 100};
        for (size_t r = 0; r < dist_lambdas.size(); ++r) {
            for (size_t i = 0; i < scale_factors.size(); ++i) {
                stream_name = "stream_cdlsf_grid" + std::to_string(int(dist_lambdas[r] * 100)) + 
                "-" + std::to_string(scale_factors[i]);

                std::vector<EventRecord> events = read_stream("streams/" + stream_name + ".bin");
                prefault(events);

                auto [stream_cfg, stream_m] = read_meta("streams/" + stream_name + ".json");
                BookConfig book_cfg{.base_price = stream_m.lowest_price, .band_ticks = stream_m.highest_price - stream_m.lowest_price + 1,
                    .arena_size = stream_m.max_live_orders + 50, 
                    .number_of_levels = size_t(stream_m.highest_price - stream_m.lowest_price) 
                    };
                std::cout << "Benchmarking books on " << stream_name << "\n";
                std::vector<uint32_t> book_timings(stream_cfg.n_events-n_warm, 0);
                PmcCycles pmc;
                pmc.open();
                uint32_t timer_overhead = measure_timer_overhead(pmc);
                out << stream_name << ',';
                write_stream_cfg(stream_cfg, out);
                write_stream_measurements(stream_m, out);
                ArrayBook array_book(book_cfg);
                benchmark(array_book, "ArrayBook", events, book_timings, n_warm, stream_cfg.n_events, timer_overhead, pmc);
                SortedVecBook sorted_vec_book(book_cfg);
                benchmark(sorted_vec_book, "SortedVecBook", events, book_timings, n_warm, stream_cfg.n_events, timer_overhead, pmc);
                HashBook hash_book(book_cfg);
                benchmark(hash_book, "HashBook", events, book_timings, n_warm, stream_cfg.n_events, timer_overhead, pmc);
                BitmapBook bitmap_book(book_cfg);
                benchmark(bitmap_book, "BitmapBook", events, book_timings, n_warm, stream_cfg.n_events, timer_overhead, pmc);
                out << "\n"; 
            }
        }
    }   
    else {
        std::ofstream out("streams/" + stream_name + ".csv");
        if (!out) throw std::runtime_error("cannot open" + stream_name + ".csv");
        write_csv_header(out);
        std::cout << "Benching " << stream_name << "\n";

        size_t n_warm = argc > 2 ? std::atoi(argv[2]) : 100'000;
     
        std::vector<EventRecord> events = read_stream("streams/" + stream_name + ".bin");
        prefault(events);

        auto [stream_cfg, stream_m] = read_meta("streams/" + stream_name + ".json");
        BookConfig book_cfg{.base_price = stream_m.lowest_price, .band_ticks = stream_m.highest_price - stream_m.lowest_price + 1,
            .arena_size = stream_m.max_live_orders + 50, 
            .number_of_levels = size_t(stream_m.highest_price - stream_m.lowest_price) 
            };
        std::cout << "Benchmarking books on " << stream_name << "\n";
        std::vector<uint32_t> book_timings(stream_cfg.n_events-n_warm, 0);
        PmcCycles pmc;
        pmc.open();
        uint32_t timer_overhead = measure_timer_overhead(pmc);
        out << stream_name << ',';
        write_stream_cfg(stream_cfg, out);
        write_stream_measurements(stream_m, out);
        ArrayBook array_book(book_cfg);
        benchmark_to_csv(array_book, "ArrayBook", events, book_timings, n_warm, stream_cfg.n_events, timer_overhead, pmc);
        SortedVecBook sorted_vec_book(book_cfg);
        benchmark_to_csv(sorted_vec_book, "SortedVecBook", events, book_timings, n_warm, stream_cfg.n_events, timer_overhead, pmc);
        HashBook hash_book(book_cfg);
        benchmark_to_csv(hash_book, "HashBook", events, book_timings, n_warm, stream_cfg.n_events, timer_overhead, pmc);
        BitmapBook bitmap_book(book_cfg);
        benchmark_to_csv(bitmap_book, "BitmapBook", events, book_timings, n_warm, stream_cfg.n_events, timer_overhead, pmc);
        out << "\n";
    }

    
    
    return 0;
}