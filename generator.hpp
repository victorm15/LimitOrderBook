#ifndef GENERATOR_H
#define GENERATOR_H

#pragma once
#include "types.hpp"
#include "events.hpp"
#include "snapshot.hpp"
#include <random>
#include <ostream>
#include <variant>
#include <type_traits>

// ---------------------------------------------------------------------------
// Generator configuration.
// Deliberately narrow price range and small quantities: that makes the edge
// cases (exact fills, several orders queued at one price, sweeps across levels)
// happen constantly instead of once in a million events.
// ---------------------------------------------------------------------------
struct GenConfig {
    Price    mid;                   // centre of generated prices; must sit well inside the book's band
    Price    passive_depth    = 15; // resting orders land up to this many ticks behind mid
    Price    aggressive_depth = 3;  // aggressive orders reach up to this many ticks through mid
    Quantity max_quantity     = 10; // small on purpose: exact fills and ties become common
    int w_add = 50, w_cancel = 15, w_reduce = 10, w_execute = 10, w_replace = 15;
};

// ---------------------------------------------------------------------------
// Produces one *valid* event at a time. Validity needs the current book state
// (you can only cancel an order that exists, reduce by at most its remaining
// quantity, ...), so the generator reads the reference book's snapshot rather
// than tracking state itself. That way matching - which removes orders the
// generator never explicitly cancelled - is accounted for automatically.
// ---------------------------------------------------------------------------
class Generator {
public:
    Generator(uint64_t seed, const GenConfig& cfg) : rng_(seed), cfg_(cfg) {}

    Event next(const Snapshot& live, Price best_bid, Price best_ask) {
        if (live.empty()) return make_add();

        const int total = cfg_.w_add + cfg_.w_cancel + cfg_.w_reduce + cfg_.w_execute + cfg_.w_replace;
        int r = int(uniform(0, total - 1));
        const SnapEntry& t = live[size_t(uniform(0, int64_t(live.size()) - 1))];

        if ((r -= cfg_.w_add)     < 0) return make_add();
        if ((r -= cfg_.w_cancel)  < 0) return Cancel {.id = t.id};
        if ((r -= cfg_.w_reduce)  < 0) return Reduce {.id = t.id, .quantity_removed = Quantity(uniform(1, t.quantity))};
        if ((r -= cfg_.w_execute) < 0) return Execute{.id = t.id, .quantity         = Quantity(uniform(1, t.quantity))};
        return make_replace(t, best_bid, best_ask);
    }

private:
    std::mt19937_64 rng_;
    GenConfig       cfg_;
    OrderId         next_id_ = 1;   // ids are never reused

    int64_t uniform(int64_t lo, int64_t hi) {
        return std::uniform_int_distribution<int64_t>(lo, hi)(rng_);
    }

    Add make_add() {
        Side  side = uniform(0, 1) ? Side::Buy : Side::Sell;
        // negative offset = aggressive (reaches through mid, likely crosses)
        // positive offset = passive (rests behind mid)
        Price off  = Price(uniform(-cfg_.aggressive_depth, cfg_.passive_depth));
        Price px   = side == Side::Buy ? cfg_.mid - off : cfg_.mid + off;
        return Add{.id = next_id_++, .price = px,
                   .quantity = Quantity(uniform(1, cfg_.max_quantity)), .side = side};
    }

    // Replace never matches, so its new price must not cross the other side -
    // a crossed book has no defined behaviour and the two books could
    // legitimately disagree about it.
    Replace make_replace(const SnapEntry& t, Price best_bid, Price best_ask) {
        Price off = Price(uniform(0, cfg_.passive_depth));
        Price px  = t.side == Side::Buy ? cfg_.mid - off : cfg_.mid + off;
        if (t.side == Side::Buy  && best_ask != NO_ASK && px >= best_ask) px = best_ask - 1;
        if (t.side == Side::Sell && best_bid != NO_BID && px <= best_bid) px = best_bid + 1;
        return Replace{.old_id = t.id, .new_id = next_id_++, .price = px,
                       .quantity = Quantity(uniform(1, cfg_.max_quantity))};
    }
};

// ---------------------------------------------------------------------------
// Dispatch one Event to a book. Works for any book with the common interface.
// ---------------------------------------------------------------------------
template <typename Book, typename Sink>
void apply(Book& book, const Event& e, Sink& sink) {
    std::visit([&](const auto& ev) {
        using T = std::decay_t<decltype(ev)>;
        if constexpr      (std::is_same_v<T, Add>)     book.add(ev, sink);
        else if constexpr (std::is_same_v<T, Execute>) book.execute(ev, sink);
        else if constexpr (std::is_same_v<T, Cancel>)  book.cancel(ev);
        else if constexpr (std::is_same_v<T, Reduce>)  book.reduce(ev);
        else if constexpr (std::is_same_v<T, Replace>) book.replace(ev);
    }, e);
}

inline const char* side_str(Side s) { return s == Side::Buy ? "B" : "S"; }

inline void print_event(std::ostream& os, const Event& e) {
    std::visit([&](const auto& ev) {
        using T = std::decay_t<decltype(ev)>;
        if constexpr      (std::is_same_v<T, Add>)     os << "ADD     id=" << ev.id << ' ' << side_str(ev.side) << " px=" << ev.price << " q=" << ev.quantity;
        else if constexpr (std::is_same_v<T, Cancel>)  os << "CANCEL  id=" << ev.id;
        else if constexpr (std::is_same_v<T, Reduce>)  os << "REDUCE  id=" << ev.id << " by=" << ev.quantity_removed;
        else if constexpr (std::is_same_v<T, Execute>) os << "EXECUTE id=" << ev.id << " q=" << ev.quantity;
        else if constexpr (std::is_same_v<T, Replace>) os << "REPLACE old=" << ev.old_id << " new=" << ev.new_id << " px=" << ev.price << " q=" << ev.quantity;
    }, e);
}

#endif