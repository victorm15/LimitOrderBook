#ifndef DIFFERENTIAL_H
#define DIFFERENTIAL_H



// Differential test: feed identical random streams to RefBook and a fast book,
// compare everything observable after EVERY event, stop at the first divergence.
//
// usage: ./differential [n_seeds] [n_events_per_seed]
//   start short (e.g. 2000 20) so failures come with short, readable histories,
//   then go longer (e.g. 1000 5000) once short streams pass.

#include "ref_book.hpp"
#include "array_book.hpp"
#include "generator.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Failure reporting
// ---------------------------------------------------------------------------
static std::string entry_str(const Snapshot& s, size_t i) {
    if (i >= s.size()) return "-";
    const SnapEntry& e = s[i];
    return std::string(side_str(e.side)) + " px=" + std::to_string(e.price) +
           " id=" + std::to_string(e.id) + " q=" + std::to_string(e.quantity);
}

// Side-by-side, with '*' marking rows that differ.
static void dump_diff(const Snapshot& ref, const Snapshot& fast) {
    std::printf("    %-34s | %s\n", "ref", "fast");
    for (size_t i = 0; i < std::max(ref.size(), fast.size()); ++i) {
        std::string a = entry_str(ref, i), b = entry_str(fast, i);
        std::printf("  %c %-34s | %s\n", a == b ? ' ' : '*', a.c_str(), b.c_str());
    }
}

static void dump_fills(const char* who, const std::vector<Fill>& fs) {
    std::printf("  %s fills:", who);
    if (fs.empty()) std::printf(" (none)");
    for (const Fill& f : fs) std::printf(" [id=%llu px=%d q=%u]",
                                         (unsigned long long)f.maker, f.price, f.quantity);
    std::printf("\n");
}

// ---------------------------------------------------------------------------
// Everything we can observe about a book, compared against the reference.
// Returns an empty string if they agree, otherwise a description.
// ---------------------------------------------------------------------------
template <typename Fast>
static std::string compare(const RefBook& ref, const Fast& fast,
                           const Snapshot& sr, const Snapshot& sf,
                           const std::vector<Fill>& fr, const std::vector<Fill>& ff,
                           Price lo, Price hi) {
    for (const SnapEntry& e : sf) if (e.quantity == 0) return "fast book has a zero-quantity order resting";
    for (const SnapEntry& e : sr) if (e.quantity == 0) return "ref book has a zero-quantity order resting";

    if (fr != ff)                         return "fills differ";
    if (sr != sf)                         return "snapshots differ";
    if (ref.best_bid() != fast.best_bid()) return "best_bid differs: ref=" + std::to_string(ref.best_bid()) + " fast=" + std::to_string(fast.best_bid());
    if (ref.best_ask() != fast.best_ask()) return "best_ask differs: ref=" + std::to_string(ref.best_ask()) + " fast=" + std::to_string(fast.best_ask());

    if (ref.best_bid() != NO_BID && ref.best_ask() != NO_ASK && ref.best_bid() >= ref.best_ask())
        return "book is crossed";

    // Catches drift in cached level totals, which snapshots can't see.
    for (Price p = lo; p <= hi; ++p)
        for (Side s : {Side::Buy, Side::Sell})
            if (ref.quantity_at(s, p) != fast.quantity_at(s, p))
                return std::string("quantity_at differs: ") + side_str(s) + " px=" + std::to_string(p) +
                       " ref=" + std::to_string(ref.quantity_at(s, p)) +
                       " fast=" + std::to_string(fast.quantity_at(s, p));
    return {};
}

// ---------------------------------------------------------------------------
// One seed: generate n_events, apply to both books, compare after each.
// ---------------------------------------------------------------------------
template <typename Fast>
static bool run_one(uint64_t seed, int n_events, const GenConfig& gc, Fast& fast) {
    RefBook   ref;
    Generator gen(seed, gc);
    Snapshot  sr, sf;
    std::vector<Fill>  fr, ff;
    std::vector<Event> history;

    auto sink_r = [&](const Fill& f) { fr.push_back(f); };
    auto sink_f = [&](const Fill& f) { ff.push_back(f); };

    // Every price the generator can produce, plus a margin.
    const Price lo = gc.mid - gc.passive_depth - gc.aggressive_depth - 2;
    const Price hi = gc.mid + gc.passive_depth + gc.aggressive_depth + 2;

    ref.snapshot(sr);
    for (int i = 0; i < n_events; ++i) {
        Event e = gen.next(sr, ref.best_bid(), ref.best_ask());
        history.push_back(e);

        fr.clear(); ff.clear();
        apply(ref, e, sink_r);
        ref.snapshot(sr);

        // A throw (e.g. .at() on a missing id) is a failure like any other,
        // so report it with the history instead of dying without context.
        std::string why;
        try {
            apply(fast, e, sink_f);
            fast.snapshot(sf);
            // Uses check_invariants() automatically once the book has one.
            if constexpr (requires { fast.check_invariants(); }) fast.check_invariants();
            why = compare(ref, fast, sr, sf, fr, ff, lo, hi);
        } catch (const std::exception& ex) {
            why = std::string("fast book threw: ") + ex.what();
        }
        if (why.empty()) continue;

        std::printf("\nFAIL  seed=%llu  event #%d  : %s\n", (unsigned long long)seed, i, why.c_str());
        std::cout << "  event: "; print_event(std::cout, e); std::cout << "\n";
        dump_fills("ref ", fr);
        dump_fills("fast", ff);
        dump_diff(sr, sf);

        std::string path = "failing_seed_" + std::to_string(seed) + ".txt";
        std::ofstream out(path);
        for (const Event& ev : history) { print_event(out, ev); out << '\n'; }
        std::printf("  full history (%zu events) written to %s\n", history.size(), path.c_str());
        return false;
    }
    return true;
}

#endif