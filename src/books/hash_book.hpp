#ifndef HASH_BOOK_H
#define HASH_BOOK_H

#include "../core/types.hpp"
#include "../core/events.hpp"
#include "../core/sinks.hpp"
#include "../core/arena.hpp"
#include "../core/snapshot.hpp"
#include "../../third_party/unordered_dense.h"
#include <vector>
#include <cassert>
#include <algorithm>



class HashBook {

private:
    struct Level {
        Handle head = NIL, tail = NIL;
        Quantity total_quantity = 0;
        uint32_t total_orders = 0;
        
    };
    ankerl::unordered_dense::map<Price, Level> bids_, asks_;
    mutable std::vector<Price> bid_heap_, ask_heap_;
    ankerl::unordered_dense::map<OrderId, Handle> id_to_handle_;
    OrderArena arena_;
    #ifdef BOOK_STATS
        mutable uint64_t best_calls = 0, lazy_deletions = 0, max_lazy_deletions = 0;
        mutable uint64_t max_heap_size = 0;
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

    Level& level_of(Side side, Price price) {
        auto& map = (side == Side::Buy) ? bids_ : asks_;
        assert(map.contains(price));
        return map.at(price);
    }

    const Level& level_of(Side side, Price price) const {
        const auto& map = (side == Side::Buy) ? bids_ : asks_;
        return map.at(price);
    }

    void reduce_order(Order& o, Quantity quantity_removed) {
        assert(quantity_removed <= o.quantity);
        if (o.quantity == quantity_removed) {
            Handle h = id_to_handle_.at(o.id);
            id_to_handle_.erase(o.id);
            Level& lvl = level_of(o.side, o.price);
            unlink(lvl, h);
            if (lvl.total_orders == 0) {
                if (o.side == Side::Buy) bids_.erase(o.price); else asks_.erase(o.price);
            }
            arena_.release(h);
        }
        else {
            o.quantity -= quantity_removed; 
            level_of(o.side, o.price).total_quantity -= quantity_removed;
        }
    }

    void insert_order(Order& o, Handle h) {
        bool is_buy = o.side == Side::Buy;
        auto& map = is_buy ? bids_ : asks_;
        if (!map.contains(o.price)) {
            map.insert({o.price, Level{}});
            if (is_buy) {
                bid_heap_.push_back(o.price);
                std::push_heap(bid_heap_.begin(), bid_heap_.end());
            }
            else {
                ask_heap_.push_back(o.price);
                std::push_heap(ask_heap_.begin(), ask_heap_.end(), std::greater<Price>{});
            }
        }
        push_back(level_of(o.side, o.price), h);
    }

public:

    HashBook(const BookConfig& config) :
    arena_{config.arena_size}{
        bids_.reserve(config.number_of_levels);
        asks_.reserve(config.number_of_levels);
        bid_heap_.reserve(config.band_ticks);
        ask_heap_.reserve(config.band_ticks);
        id_to_handle_.reserve(config.arena_size);
    }

    template <typename Sink> void add(const Add& a, Sink&& sink) {
        Quantity quantity_left = a.quantity;
        bool is_buy = a.side == Side::Buy;
        while (quantity_left > 0) {

            Price best_price = is_buy ? best_ask() : best_bid();
            if (is_buy ? best_price > a.price : best_price < a.price) break;

            Order& resting = arena_[level_of(
                (is_buy ? Side::Sell : Side::Buy), 
                (best_price))
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
            insert_order(incoming, incoming_handle);
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
        Order& o = arena_[id_to_handle_.at(e.id)];
        sink(Fill {e.id, o.price, e.quantity});
        reduce_order(o, e.quantity);
    }

    template <typename Sink> void replace(const Replace& r, Sink&& sink) {
        Side side = arena_[id_to_handle_.at(r.old_id)].side;
        cancel(Cancel{.id = r.old_id});
        add(Add{.id = r.new_id, .price = r.price, .quantity = r.quantity, .side = side}, sink);
    }

    Price best_bid() const {
        #ifdef BOOK_STATS
            if (max_heap_size < bid_heap_.size()) max_heap_size = bid_heap_.size();
            ++best_calls;
            uint64_t current_deletions = 0;
        #endif
        if (bid_heap_.empty()) return NO_BID;
        while (!bid_heap_.empty()) {
            if (bids_.contains(bid_heap_.front())) {
                #ifdef BOOK_STATS
                    lazy_deletions += current_deletions;
                    if (max_lazy_deletions < current_deletions) max_lazy_deletions = current_deletions;
                #endif
                return bid_heap_.front();
            }
            #ifdef BOOK_STATS
                ++current_deletions;
            #endif
            std::pop_heap(bid_heap_.begin(), bid_heap_.end());
            bid_heap_.pop_back();
        }
        return NO_BID;
    }

    Price best_ask() const {
        #ifdef BOOK_STATS
            if (max_heap_size < bid_heap_.size()) max_heap_size = ask_heap_.size();
            ++best_calls;
            uint64_t current_deletions = 0;
        #endif
        if (ask_heap_.empty()) return NO_ASK;
        while (!ask_heap_.empty()) {
            if (asks_.contains(ask_heap_.front())) {
                #ifdef BOOK_STATS
                    lazy_deletions += current_deletions;
                    if (max_lazy_deletions < current_deletions) max_lazy_deletions = current_deletions;
                #endif
                return ask_heap_.front();
            }
            #ifdef BOOK_STATS
                ++current_deletions;
            #endif
            std::pop_heap(ask_heap_.begin(), ask_heap_.end(), std::greater<Price>{});
            ask_heap_.pop_back();
        }
        return NO_ASK;
    }

    Quantity   quantity_at(Side side, Price price) const {
        return (side == Side::Buy ? bids_.contains(price) 
        : asks_.contains(price)) ? level_of(side, price).total_quantity : 0;
    }

    void  snapshot(Snapshot& snap) const {
        snap.clear();

        std::vector<Price> sorted_bid_prices;
        sorted_bid_prices.reserve(bids_.size());
        for (const auto& bid : bids_) sorted_bid_prices.push_back(bid.first);
        std::sort(sorted_bid_prices.begin(), sorted_bid_prices.end(), std::greater<Price>{});

        std::vector<Price> sorted_ask_prices;
        sorted_ask_prices.reserve(asks_.size());
        for (const auto& ask : asks_) sorted_ask_prices.push_back(ask.first);
        std::sort(sorted_ask_prices.begin(), sorted_ask_prices.end());

        for (Price p : sorted_bid_prices) {
            Handle order_handle = level_of(Side::Buy, p).head;
            while (order_handle != NIL) {
                const Order& o = arena_[order_handle];
                snap.push_back(SnapEntry{o.id, o.price, o.quantity, Side::Buy});
                order_handle = o.next;
            }
        } 
        for (Price p : sorted_ask_prices) {
            Handle order_handle = level_of(Side::Sell, p).head;
            while (order_handle != NIL) {
                const Order& o = arena_[order_handle];
                snap.push_back(SnapEntry{o.id, o.price, o.quantity, Side::Sell});
                order_handle = o.next;
            }
        }

    }

    #ifdef BOOK_STATS
        HashBookStats get_stats() const {
            return HashBookStats{
                .best_calls = best_calls, 
                .lazy_deletions = lazy_deletions,
                .max_lazy_deletions = max_lazy_deletions,
                .max_heap_size = max_heap_size
            };
        }

        void reset_stats() {
            best_calls = 0;
            lazy_deletions = 0;
            max_lazy_deletions = 0;
            max_heap_size = 0;
        }
    #endif

};

#endif