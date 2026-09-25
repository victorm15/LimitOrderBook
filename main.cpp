#include "ref_book.hpp"
#include "array_book.hpp"
#include "differential.hpp"
#include "sorted_vec_book.hpp"




void diff(int n_seeds, int n_events) {
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
                      [&] { return ArrayBook(c.book); },     gc, n_seeds, n_events)) return;

        if (!run_book((std::string("SortedVecBook ") + c.name).c_str(),
                      [&] { return SortedVecBook(c.book); }, gc, n_seeds, n_events)) return;

        if (!run_book((std::string("HashBook ")      + c.name).c_str(),
                      [&] { return HashBook(c.book); },      gc, n_seeds, n_events)) return;
    }
}

void scratch_drvr() {
    RefBook refbook = RefBook();
    NullSink nsink = NullSink{};
    Snapshot snap;

    Add bid1 = Add{1, 100, 10, Side::Buy};
    refbook.add(bid1, nsink);
    refbook.snapshot(snap);
    print_snapshot(snap);

    Add bid2 = Add{3, 105, 10, Side::Buy};
    refbook.add(bid2, nsink);
    refbook.snapshot(snap);
    print_snapshot(snap);

    Add ask1 = Add{2, 105, 5, Side::Sell};
    refbook.add(ask1, nsink);
    refbook.snapshot(snap);
    print_snapshot(snap);
}


int main() {
    diff(500, 500);
    return 0;
}