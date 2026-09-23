#include "ref_book.hpp"
#include "differential.hpp"




void diff(const int n_seeds, const int n_events) {

    struct Case { const char* name; ArrayBookConfig book; };
    // Two bands. With base 0, a tick index and a price are the same number, so
    // tick/price mix-ups are invisible. The large base makes them fail loudly.
    const Case cases[] = {
        {"base=0",      {.base_price = 0,      .band_ticks = 2000, .arena_size = 10'000}},
        {"base=999000", {.base_price = 999000, .band_ticks = 2000, .arena_size = 10'000}},
    };

    for (const Case& c : cases) {
        GenConfig gc{.mid = c.book.base_price + Price(c.book.band_ticks / 2)};
        for (int s = 0; s < n_seeds; ++s) {
            ArrayBook fast(c.book);
            if (!run_one(uint64_t(s), n_events, gc, fast)) {
                std::printf("  case: %s\n", c.name);
                return;
            }
        }
        std::printf("%-12s %d seeds x %d events: OK\n", c.name, n_seeds, n_events);
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
    diff(2000,200);
    return 0;
}