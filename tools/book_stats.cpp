#include "../src/benchmarking/stream_io.hpp"
#include "../src/benchmarking/benchmark.hpp"
#include "../src/books/array_book.hpp"
#include "../src/books/sorted_vec_book.hpp"
#include "../src/books/hash_book.hpp"
#include "../src/books/bitmap_book.hpp"
#include <string>
#include <numeric>


int main(int argc, char** argv) {
    std::string stream_name = argc > 1 ? argv[1] : "stream0";
    std::ofstream csv_out("streams/" + stream_name + "_book_stats.csv");
    if (!csv_out) throw std::runtime_error("cannot open csv");
    write_stats_csv_header(csv_out);
    size_t n_warm = 100'000;

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
    else stream_names.push_back("streams/" + stream_name);

    for (size_t i = 0; i < stream_names.size(); ++i) {
        std::cout << "\nGetting Book Stats For: " << stream_names[i] 
        << "(" << i+1 << "/" << stream_names.size() << ")" << "...\n";

        std::vector<EventRecord> events = read_stream(stream_names[i] + ".bin");
        prefault(events);
        auto [stream_cfg, stream_m] = read_meta(stream_names[i] + ".json");
        BookConfig book_cfg{.base_price = stream_m.lowest_price, .band_ticks = stream_m.highest_price - stream_m.lowest_price + 1,
            .arena_size = stream_m.max_live_orders + 50, 
            .number_of_levels = size_t(stream_m.highest_price - stream_m.lowest_price) 
            };

        csv_out << stream_names[i] << ',';
        
        ArrayBook array_book(book_cfg);
        run_untimed(array_book, events, 0, n_warm);
        array_book.reset_stats();
        run_untimed(array_book, events, n_warm, stream_cfg.n_events);
        ArrayBookStats array_book_stats = array_book.get_stats();
        write_stats(csv_out, array_book_stats);
        Price array_best_bid = array_book.best_bid();
        Price array_best_ask = array_book.best_ask();
        asm volatile("" : : "r,m"(array_best_bid) : "memory");
        asm volatile("" : : "r,m"(array_best_ask) : "memory");

        // BitmapBook bitmap_book(book_cfg);
        // run_untimed(bitmap_book, events, 0, n_warm);
        // bitmap_book.reset_stats();
        // run_untimed(bitmap_book, events, n_warm, stream_cfg.n_events);
        // BitmapBookStats bitmap_book_stats = bitmap_book.get_stats();
        // write_stats(csv_out, bitmap_book_stats);

        // HashBook hash_book(book_cfg);
        // run_untimed(hash_book, events, 0, n_warm);
        // hash_book.reset_stats();
        // run_untimed(hash_book, events, n_warm, stream_cfg.n_events);
        // HashBookStats hash_book_stats = hash_book.get_stats();
        // write_stats(csv_out, hash_book_stats);

        // SortedVecBook sorted_vec_book(book_cfg);
        // run_untimed(sorted_vec_book, events, 0, n_warm);
        // sorted_vec_book.reset_stats();
        // run_untimed(sorted_vec_book, events, n_warm, stream_cfg.n_events);
        // SortedVecBookStats sorted_vec_book_stats = sorted_vec_book.get_stats();
        // write_stats(csv_out, sorted_vec_book_stats);
        // csv_out << "\n";

    }
    return 0;
}