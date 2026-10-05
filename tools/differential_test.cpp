#include "../src/testing/differential.hpp"

int main(int argc, char** argv) {
    const int n_seeds  = argc > 1 ? std::atoi(argv[1]) : 2000;
    const int n_events = argc > 2 ? std::atoi(argv[2]) : 20;
    struct Case { const char* name; BookConfig book; };
    const Case cases[] = {
        {"base=0",      {.base_price = 0,      .band_ticks = 2000,
                         .arena_size = 10'000, .number_of_levels = 512}},
        {"base=999000", {.base_price = 999000, .band_ticks = 2000,
                         .arena_size = 10'000, .number_of_levels = 512}},
    };

    for (const Case& c : cases) {
        GenConfig gc{.mid = c.book.base_price + Price(c.book.band_ticks / 2)};

        if (!run_book((std::string("ArrayBook ")     + c.name).c_str(),
                      [&] { return ArrayBook(c.book); },     gc, n_seeds, n_events)) return 1;

        if (!run_book((std::string("SortedVecBook ") + c.name).c_str(),
                      [&] { return SortedVecBook(c.book); }, gc, n_seeds, n_events)) return 1;

        if (!run_book((std::string("HashBook ")      + c.name).c_str(),
                      [&] { return HashBook(c.book); },      gc, n_seeds, n_events)) return 1;
        if (!run_book((std::string("BitmapBook ")      + c.name).c_str(),
                      [&] { return BitmapBook(c.book); },    gc, n_seeds, n_events)) return 1;
    }
    std::printf("\nall books passed\n");
    return 0;
}