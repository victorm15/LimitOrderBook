#ifndef SORTED_VEC_BOOK_H
#define SORTED_VEC_BOOK_H

#include "../core/types.hpp"
#include "../core/events.hpp"
#include "../core/snapshot.hpp"
#include "../core/sinks.hpp"
#include "../core/arena.hpp"
#include "../../third_party/unordered_dense.h"
#include <vector>
#include <unordered_map>
#include <cassert>

class SortedVecBook {

private:

    struct Level {
        Price price;
        Handle head = NIL, tail = NIL;
        Quantity total_quantity = 0;
        uint32_t total_orders = 0;
    };
    static constexpr size_t NOT_FOUND = size_t(-1);

    std::vector<Level> bids_; // ascending
    std::vector<Level> asks_; // descending
    ankerl::unordered_dense::map<OrderId, Handle> id_to_handle_;
    // std::unordered_map<OrderId, Handle> id_to_handle_;
    OrderArena arena_;

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

    // asserts level of that price exists
    size_t index_at_price(Side side, Price price) const { 
        size_t lo = 0;
        bool is_buy = side == Side::Buy;
        size_t hi = is_buy ? bids_.size() : asks_.size();
        const std::vector<Level>& vec = is_buy ? bids_ : asks_;
        while(true) {
            if (lo == hi) return NOT_FOUND;
            size_t mid = lo + (hi-lo)/2;
            if (vec[mid].price == price) return mid;
            if (is_buy) {
                if (vec[mid].price > price) hi = mid; else lo = mid + 1;
            }
            else {
                if (vec[mid].price > price) lo = mid + 1; else hi = mid;
            }
        }
    }

    void reduce_order(size_t lvl_idx, Order& o, Quantity quantity_removed) {
        assert(quantity_removed <= o.quantity);
        bool is_buy = o.side == Side::Buy;
        Level& lvl = is_buy ? bids_[lvl_idx] : asks_[lvl_idx];
        if (o.quantity == quantity_removed) {
            Handle h = id_to_handle_.at(o.id);
            id_to_handle_.erase(o.id);
            unlink(lvl, h);
            if (lvl.total_orders == 0) {
                if (is_buy) bids_.erase(bids_.begin() + lvl_idx); 
                else asks_.erase(asks_.begin() + lvl_idx);
            }
            arena_.release(h);
        }
        else {
            o.quantity -= quantity_removed; 
            lvl.total_quantity -= quantity_removed;
        }
    }

    Level& get_level(Side side, size_t lvl_idx) {
        std::vector<Level>& vec = (side == Side::Buy) ? bids_ : asks_;
        assert(lvl_idx < vec.size());
        return vec[lvl_idx];
    }

    void insert_order(Order& o, Handle h) {
        size_t lo = 0;
        bool is_buy = o.side == Side::Buy;
        size_t hi = is_buy ? bids_.size() : asks_.size();
        std::vector<Level>& vec = is_buy ? bids_ : asks_;
        while(true) {
            if (lo == hi) {
                // Level doesn't exist
                o.next = NIL;
                o.prev = NIL;
                vec.insert(vec.begin() + lo, Level{o.price, h, h, o.quantity, 1});
                return;
            }
            size_t mid = lo + (hi-lo)/2;
            if (vec[mid].price == o.price) {
                push_back(vec[mid], h);
                return;
            }
            if (is_buy) {
                if (vec[mid].price > o.price) hi = mid; else lo = mid + 1;
            }
            else {
                if (vec[mid].price > o.price) lo = mid + 1; else hi = mid;
            }
        }
    }

public:

    SortedVecBook(const BookConfig& config) :
    arena_{config.arena_size}{
        bids_.reserve(config.number_of_levels);
        asks_.reserve(config.number_of_levels);
        id_to_handle_.reserve(config.arena_size);
    }

    template <typename Sink> void add(const Add& a, Sink&& sink) {
        Quantity quantity_left = a.quantity;
        bool is_buy = a.side == Side::Buy;
        std::vector<Level>& vec = is_buy ? asks_ : bids_;
        while (quantity_left > 0) {

            if (is_buy ? best_ask() > a.price : best_bid() < a.price) break;

            Order& resting = arena_[vec.back().head];
            if (resting.quantity > quantity_left) {
                sink(Fill{resting.id, resting.price, quantity_left});
                reduce_order(vec.size()-1, resting, quantity_left);
                quantity_left = 0;
            }
            else {
                sink(Fill{resting.id, resting.price, resting.quantity});
                quantity_left -= resting.quantity;
                reduce_order(vec.size()-1, resting, resting.quantity);
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
        size_t lvl_idx = index_at_price(o.side, o.price);
        assert(lvl_idx != NOT_FOUND);
        reduce_order(lvl_idx, o, o.quantity);
    }

    void reduce(const Reduce& r) {
        Handle h = id_to_handle_.at(r.id);
        Order& o = arena_[h];
        size_t lvl_idx = index_at_price(o.side, o.price);
        assert(lvl_idx != NOT_FOUND);
        reduce_order(lvl_idx, o, r.quantity_removed);
    }

    template <typename Sink> void execute(const Execute& e, Sink&& sink) {
        Handle h = id_to_handle_.at(e.id);
        Order& o = arena_[h];
        sink(Fill{o.id, o.price, e.quantity});
        size_t lvl_idx = index_at_price(o.side, o.price);
        assert(lvl_idx != NOT_FOUND);
        reduce_order(lvl_idx, o, e.quantity);
    }

    template <typename Sink> void replace(const Replace& r, Sink&& sink) {
        Side side = arena_[id_to_handle_.at(r.old_id)].side;
        cancel(Cancel{.id = r.old_id});
        add(Add{.id = r.new_id, .price = r.price, .quantity = r.quantity, .side = side}, sink);
    }

    Price best_bid() const { 
        return bids_.empty() ? NO_BID : bids_.back().price;
    }

    Price best_ask() const {
        return asks_.empty() ? NO_ASK : asks_.back().price;
    }

    Quantity   quantity_at(Side side, Price price) const {
        size_t lvl_idx = index_at_price(side, price);
        if (lvl_idx == NOT_FOUND) return 0;
        return (side == Side::Buy) ? bids_[lvl_idx].total_quantity 
        : asks_[lvl_idx].total_quantity;
    }

    void  snapshot(Snapshot& snap) const {
        snap.clear();
        for (size_t i = bids_.size(); i-- > 0;) {
            Handle order_handle = bids_[i].head;
            while (order_handle != NIL) {
                const Order& o = arena_[order_handle];
                snap.push_back(SnapEntry{o.id, o.price, o.quantity, Side::Buy});
                order_handle = o.next;
            }
        }
        for (size_t i = asks_.size(); i-- > 0;) {
            Handle order_handle = asks_[i].head;
            while (order_handle != NIL) {
                const Order& o = arena_[order_handle];
                snap.push_back(SnapEntry{o.id, o.price, o.quantity, Side::Sell});
                order_handle = o.next;
            }
        }
    }


};

#endif