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
    std::ofstream csv_out("streams/" + stream_name + "_results.csv");
    int trials = argc > 2 ? std::atoi(argv[2]) : 3;
    if (!csv_out) throw std::runtime_error("cannot open csv");
    write_bench_csv_header(csv_out);

    std::vector<std::string> stream_names;
    if (stream_name == "lambda_sf_grid") {
        std::vector<double> dist_lambdas = {0.0, 0.01, 0.05, 0.1, 0.2, 0.3, 0.4, 0.5};
        std::vector<int> scale_factors = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 20, 50, 100};
        for (size_t r = 0; r < dist_lambdas.size(); ++r) {
            for (size_t i = 0; i < scale_factors.size(); ++i) {
                stream_names.push_back("streams/stream_cdlsf_grid" + 
                    std::to_string(int(dist_lambdas[r] * 100)) + 
                    "-" + std::to_string(scale_factors[i]));
            }
        }
    }
    else if (stream_name == "add_lambda") {
        std::vector<double> add_lambdas = {0.0, -0.01, -0.05, -0.1, -0.2, -0.3, -0.4, -0.5};
        std::vector<int> scale_factors = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 20, 50, 100};
        for (size_t r = 0; r < add_lambdas.size(); ++r) {
            for (size_t i = 0; i < scale_factors.size(); ++i) {
                stream_names.push_back("streams/stream_adlsf_grid" + std::to_string(int(add_lambdas[r] * 100)) + 
                "-" + std::to_string(scale_factors[i]));
            }
        }
    }
    else if (stream_name == "level_churn") {
        std::vector<double> add_lambdas = {0.5, 0.4, 0.3, 0.2, 0.1, 0.05, 0.01, 0.0, -0.01, -0.05, -0.1, -0.2, -0.3, -0.4, -0.5};
        std::vector<double> cancel_lambdas = {0.5, 0.4, 0.3, 0.2, 0.1, 0.05, 0.01, 0.0, -0.01, -0.05, -0.1, -0.2, -0.3, -0.4, -0.5};
        for (size_t r = 0; r < add_lambdas.size(); ++r) {
            for (size_t i = 0; i < cancel_lambdas.size(); ++i) {
                stream_names.push_back("streams/stream_lvlchurn_grid" + std::to_string(int(add_lambdas[r] * 100)) + 
                "-" + std::to_string(int(cancel_lambdas[i] * 100)));
            }
        }
    }
    else stream_names.push_back("streams/" + stream_name);

    PmcCycles pmc;
    pmc.open();
    uint32_t timer_overhead = measure_timer_overhead(pmc);

    for (size_t i = 0; i < stream_names.size(); ++i) {
        std::cout << "\nBenching Stream " << stream_names[i] 
        << "(" << i+1 << "/" << stream_names.size() << ")" << "...\n";

        std::vector<EventRecord> events = read_stream(stream_names[i] + ".bin");
        prefault(events);
        auto [stream_cfg, stream_m] = read_meta(stream_names[i] + ".json");
        BookConfig book_cfg{.base_price = stream_m.lowest_price, .band_ticks = stream_m.highest_price - stream_m.lowest_price + 1,
            .arena_size = stream_m.max_live_orders + 50, 
            .number_of_levels = size_t(stream_m.highest_price - stream_m.lowest_price),
            .heap_size = 200'000
            };

        csv_out << stream_names[i] << ',';
        write_stream_info(stream_cfg, stream_m, csv_out);

        std::vector<uint32_t> book_timings(stream_cfg.n_events-N_WARM, 0);
        
        BenchTimestamps array_book_times = 
        benchmark<ArrayBook>(book_cfg, events, book_timings, stream_cfg.n_events, timer_overhead, pmc, trials);
        write_bench_results(array_book_times, csv_out);
        std::cout << "Array Book:\n";
        print_bench_results(array_book_times);

        BenchTimestamps sorted_vec_book_times = 
        benchmark<SortedVecBook>(book_cfg, events, book_timings, stream_cfg.n_events, timer_overhead, pmc, trials);
        write_bench_results(sorted_vec_book_times, csv_out);
        std::cout << "Sorted Vec Book:\n";
        print_bench_results(sorted_vec_book_times);

        BenchTimestamps hash_book_times = 
        benchmark<HashBook>(book_cfg, events, book_timings, stream_cfg.n_events, timer_overhead, pmc, trials);
        write_bench_results(hash_book_times, csv_out);
        std::cout << "Hash Book:\n";
        print_bench_results(hash_book_times);

        BenchTimestamps bitmap_book_times = 
        benchmark<BitmapBook>(book_cfg, events, book_timings, stream_cfg.n_events, timer_overhead, pmc, trials);
        write_bench_results(bitmap_book_times, csv_out);
        std::cout << "Bitmap Book:\n";
        print_bench_results(bitmap_book_times);
        csv_out << "\n";

    }
    return 0;
}