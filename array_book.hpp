#ifndef ARRAY_BOOK_H
#define ARRAY_BOOK_H

#include "types.hpp"
#include "arena.hpp"
#include "book_config.hpp"
#include "events.hpp"
#include "sinks.hpp"
#include "snapshot.hpp"

class ArrayBook {

private:
    std::vector<Level> bids_;
    std::vector<Level> asks_;
    Price best_bid_;
    Price best_ask_;
    OrderArena arena_;
    Price base_price_;

    void push_back(Level& lvl, Handle h) {
        arena_[h].prev = lvl.tail;
        arena_[h].next = NIL;
        if (lvl.tail != NIL) arena_[lvl.tail].next = h;
        else lvl.head = h;
        lvl.tail = h;
        ++lvl.total_orders;
        lvl.total_quantity += arena_[h].quantity;
    }

    void pop_front(Level& lvl) {
        assert(lvl.head != NIL);
        Handle h  = lvl.head;
        if (lvl.head == lvl.tail) lvl.tail = NIL;
        lvl.head = arena_[h].next;
        arena_[h].next = NIL;
        arena_[h].prev = NIL;
        arena_[lvl.head].prev = NIL;
        lvl.total_quantity -= arena_[h].quantity;
        --lvl.total_orders;
    }

    void unlink(Level& lvl, Handle h) {
        if (lvl.head == h) lvl.head = arena_[h].next; 
        else arena_[arena_[h].prev].next = arena_[h].next;
        if (lvl.tail == h) lvl.tail = arena_[h].prev; 
        else arena_[arena_[h].next].prev = arena_[h].prev;
        --lvl.total_orders;
        lvl.total_quantity -= arena_[h].quantity;
    }

public:

    ArrayBook(ArrayBookConfig& config) 
        : bids_(config.band_ticks),
          asks_(config.band_ticks),
          best_bid_{NO_BID},
          best_ask_{NO_ASK},
          arena_{config.arena_size},
          base_price_{config.base_price}
    {}

    template <typename Sink> void add(const Add& a, Sink&& sink);
    void cancel(const Cancel& c);
    void reduce(const Reduce& r);
    template <typename Sink> void execute(const Execute& e, Sink&& sink);
    template <typename Sink> void replace(const Replace& r);
    Price best_bid() const;
    Price best_ask() const;
    Quantity   quantity_at(Side side, Price price) const;
    void  snapshot(Snapshot& snap) const;

};


#endif