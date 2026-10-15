#ifndef BITMAPBOOK_H
#define BITMAPBOOK_H

#include "../core/types.hpp"
#include "../core/arena.hpp"
#include "../core/events.hpp"
#include "../core/sinks.hpp"
#include "../core/snapshot.hpp"
#include "../../third_party/unordered_dense.h"
#include <vector>
#include <cassert>
#include <cmath>
#include <bit>

class BitmapBook {

private:
    struct Level {
        Handle head = NIL, tail = NIL;
        Quantity total_quantity = 0;
        uint32_t total_orders = 0;
        
    };
    std::vector<Level> bids_, asks_;
    std::vector<uint64_t> bids_bitmap_, asks_bitmap_;
    Price best_bid_;
    Price best_ask_;
    OrderArena arena_;
    Price base_price_;
    ankerl::unordered_dense::map<OrderId, Handle> id_to_handle_;
    #ifdef BOOK_STATS
        mutable uint64_t scans = 0, scan_ticks = 0, scan_words = 0;
    #endif

    void push_back(Level& lvl, Handle h) {
        Order& o = arena_[h];
        o.prev = lvl.tail;
        o.next = NIL;
        if (lvl.tail != NIL) arena_[lvl.tail].next = h;
        else lvl.head = h;
        lvl.tail = h;
        ++lvl.total_orders;
        lvl.total_quantity += o.quantity;
    }

    void pop_front(Level& lvl) {
        assert(lvl.head != NIL);
        Order& o = arena_[lvl.head];
        if (lvl.head == lvl.tail) lvl.tail = NIL;
        lvl.head = o.next;
        o.next = NIL;
        o.prev = NIL;
        if (lvl.head != NIL) arena_[lvl.head].prev = NIL;
        lvl.total_quantity -= o.quantity;
        --lvl.total_orders;
    }

    void unlink(Level& lvl, Handle h) {
        Order& o = arena_[h];
        if (lvl.head == h) lvl.head = o.next; 
        else arena_[o.prev].next = o.next;
        if (lvl.tail == h) lvl.tail = o.prev; 
        else arena_[o.next].prev = o.prev;
        --lvl.total_orders;
        lvl.total_quantity -= o.quantity;
    }

    void update_best_bid() {
        #ifdef BOOK_STATS
            ++scans;
            Price prev_best = best_bid_;
        #endif
        size_t word = word_of(best_bid_);
        for (size_t t = word+1; t-- > 0;) {
            #ifdef BOOK_STATS
               ++scan_words;     
            #endif
            if (bids_bitmap_[t]) {
                best_bid_ = t*64 + 63-std::countl_zero(bids_bitmap_[t]) + base_price_;
                #ifdef BOOK_STATS
                    scan_ticks += prev_best - best_bid_;
                #endif
                return;
            }
        }
        best_bid_ = NO_BID;
    }
    
    void update_best_ask() {
        #ifdef BOOK_STATS
            ++scans;
            Price prev_best = best_ask_;
        #endif
        size_t word = word_of(best_ask_);
        for (size_t t = word; t < asks_bitmap_.size(); ++t) {
            #ifdef BOOK_STATS
               ++scan_words;     
            #endif
            if (asks_bitmap_[t]) {
                best_ask_ = t*64 + std::countr_zero(asks_bitmap_[t]) + base_price_;
                #ifdef BOOK_STATS
                    scan_ticks += best_ask_ - prev_best;
                #endif
                return;
            }
        }
        best_ask_ = NO_ASK;
    }

    Level& level_of(Side side, Price price) { 
        assert(price >= base_price_ && price - base_price_ < Price(bids_.size()));
        return (side == Side::Buy) ? (bids_[price - base_price_]) : (asks_[price - base_price_]); 
    }
    
    const Level& level_of(Side side, Price price) const { 
        assert(price >= base_price_ && price - base_price_ < Price(bids_.size()));
        return (side == Side::Buy) ? (bids_[price - base_price_]) : (asks_[price - base_price_]); 
    }

    constexpr size_t word_of(Price p) const { return (p-base_price_) >> 6; }

    void clear_bit(Side side, Price p) {
        size_t word = (p - base_price_) >> 6;
        int bit = (p - base_price_) & 63;
        if (side == Side::Buy) bids_bitmap_[word] &= ~(1ULL << bit);
        else asks_bitmap_[word] &= ~(1ULL << bit);
    }
    
    void set_bit(Side side, Price p) {
        size_t word = (p - base_price_) >> 6;
        int bit = (p - base_price_) & 63;
        if (side == Side::Buy) bids_bitmap_[word] |= 1ULL << bit;
        else asks_bitmap_[word] |= 1ULL << bit;
    }

    void reduce_order(Order& o, Quantity quantity_removed) {
        assert(quantity_removed <= o.quantity);
        if (o.quantity == quantity_removed) {
            Handle h = id_to_handle_.at(o.id);
            id_to_handle_.erase(o.id);
            Level& lvl = level_of(o.side, o.price);
            unlink(lvl, h);
            if (lvl.total_orders == 0) {
                clear_bit(o.side, o.price);
                if (o.side == Side::Buy && o.price == best_bid_) update_best_bid(); 
                else if (!(o.side == Side::Buy) && o.price == best_ask_) update_best_ask();
            }
            arena_.release(h);
        }
        else {
            o.quantity -= quantity_removed; 
            level_of(o.side, o.price).total_quantity -= quantity_removed;
        }
    }

public:

    BitmapBook(const BookConfig& config) 
        : bids_(config.band_ticks),
          asks_(config.band_ticks),
          bids_bitmap_((config.band_ticks + 63) / 64),
          asks_bitmap_((config.band_ticks + 63) / 64),
          best_bid_{NO_BID},
          best_ask_{NO_ASK},
          arena_{config.arena_size},
          base_price_{config.base_price}
    {id_to_handle_.reserve(config.arena_size);}

    template <typename Sink> void add(const Add& a, Sink&& sink) {
        Quantity quantity_left = a.quantity;
        bool is_buy = a.side == Side::Buy;
        while (quantity_left > 0) {
            if (is_buy ? best_ask_ > a.price : best_bid_ < a.price) break;

            Order& resting = arena_[level_of(
                (is_buy ? Side::Sell : Side::Buy), 
                (is_buy ? best_ask_ : best_bid_))
                .head];

            if (resting.quantity > quantity_left) {
                sink(Fill{resting.id, resting.price, quantity_left});
                reduce_order(resting, quantity_left);
                quantity_left = 0;
            }
            else {
                sink(Fill{resting.id, resting.price, resting.quantity});
                quantity_left -= resting.quantity;
                reduce_order(resting, resting.quantity);
            }
        }
        if (quantity_left > 0) {
            Handle incoming_handle = arena_.allocate();
            Order& incoming = arena_[incoming_handle];
            assert(!id_to_handle_.contains(a.id));
            id_to_handle_[a.id] = incoming_handle;
            incoming.id = a.id;
            incoming.price = a.price;
            incoming.quantity = quantity_left;
            incoming.side = a.side;
            Level& new_lvl = level_of(incoming.side, incoming.price);
            if (new_lvl.total_orders == 0) set_bit(incoming.side, incoming.price);
            push_back(new_lvl, incoming_handle);
            if (is_buy && best_bid_ < a.price) best_bid_ = a.price;
            else if (!is_buy && best_ask_ > a.price) best_ask_ = a.price;
        }
    }
    
    void cancel(const Cancel& c) {
        Order& o = arena_[id_to_handle_.at(c.id)];
        reduce_order(o, o.quantity);
    }
    
    void reduce(const Reduce& r) { 
        reduce_order(arena_[id_to_handle_.at(r.id)], r.quantity_removed);
    }

    template <typename Sink> void execute(const Execute& e, Sink&& sink) {
        Order& resting = arena_[id_to_handle_.at(e.id)];
        sink(Fill {resting.id, resting.price, e.quantity});
        reduce_order(resting, e.quantity);
    }
    
    template <typename Sink> void replace(const Replace& r, Sink&& sink) {
        Side side = arena_[id_to_handle_.at(r.old_id)].side;
        cancel(Cancel{.id = r.old_id});
        add(Add{.id = r.new_id, .price = r.price, .quantity = r.quantity, .side = side}, sink);
    }


    Price best_bid() const { return best_bid_; }
    
    Price best_ask() const { return best_ask_; }
    
    Quantity   quantity_at(Side side, Price price) const { 
        assert(price >= base_price_ && price - base_price_ < Price(bids_.size()));
        return level_of(side, price).total_quantity;
    }

    void  snapshot(Snapshot& snap) const {
        snap.clear();
        for (Price p = Price(bids_.size()-1) + base_price_; p >= base_price_; --p) {
            Handle order_handle = level_of(Side::Buy, p).head;
            while (order_handle != NIL) {
                const Order& o = arena_[order_handle];
                snap.push_back(SnapEntry{o.id, o.price, o.quantity, Side::Buy});
                order_handle = o.next;
            }
        }
        for (Price p = base_price_; p < Price(asks_.size() + base_price_); ++p) {
            Handle order_handle = level_of(Side::Sell, p).head;
            while (order_handle != NIL) {
                const Order& o = arena_[order_handle];
                snap.push_back(SnapEntry{o.id, o.price, o.quantity, Side::Sell});
                order_handle = o.next;
            }
        }
    }

    #ifdef BOOK_STATS
        BitmapBookStats get_stats() const {
            return BitmapBookStats{.scans = scans, .scan_ticks = scan_ticks, .scan_words = scan_words};
        }

        void reset_stats() {
            scans = 0;
            scan_ticks = 0;
            scan_words = 0;
        }
    #endif

};


#endif