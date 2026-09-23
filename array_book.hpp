#ifndef ARRAY_BOOK_H
#define ARRAY_BOOK_H

#include "types.hpp"
#include "arena.hpp"
#include "book_config.hpp"
#include "events.hpp"
#include "sinks.hpp"
#include "snapshot.hpp"
#include <unordered_map>
#include <vector>
#include <cassert>

class ArrayBook {

private:
    std::vector<Level> bids_;
    std::vector<Level> asks_;
    Price best_bid_;
    Price best_ask_;
    OrderArena arena_;
    Price base_price_;
    std::unordered_map<OrderId, Handle> id_to_handle_;

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

    void update_best_bid(Price prev_best) {
        for (Price p = prev_best; p >= base_price_; --p) {
            if (bids_[p - base_price_].head != NIL) {
                best_bid_ = p;
                return;
            }
        }
        best_bid_ = NO_BID;
    }
    
    void update_best_ask(Price prev_best) {
        for (size_t t = size_t(prev_best - base_price_); t < asks_.size(); ++t) {
            if (asks_[t].head != NIL) {
                best_ask_ = Price(t) + base_price_;
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

    void reduce_order(Order& o, Quantity quantity_removed) {
        assert(quantity_removed <= o.quantity);
        if (o.quantity == quantity_removed) {
            Handle h = id_to_handle_.at(o.id);
            id_to_handle_.erase(o.id);
            unlink(level_of(o.side, o.price), h);
            if (o.side == Side::Buy) update_best_bid(best_bid_); else update_best_ask(best_ask_);
            arena_.release(h);
        }
        else {
            o.quantity -= quantity_removed; 
            level_of(o.side, o.price).total_quantity -= quantity_removed;
        }
    }

public:

    ArrayBook(const ArrayBookConfig& config) 
        : bids_(config.band_ticks),
          asks_(config.band_ticks),
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
                (is_buy) ? Side::Sell : Side::Buy, 
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
            push_back(level_of(a.side, a.price), incoming_handle);
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
        assert(e.quantity <= resting.quantity);
        sink(Fill {resting.id, resting.price, e.quantity});
        reduce_order(resting, e.quantity);
    }
    
    void replace(const Replace& r) {
        Handle h = id_to_handle_.at(r.old_id);
        Order& o = arena_[h];
        Level& old_lvl = level_of(o.side, o.price);
        unlink(old_lvl, h);

        o.price = r.price;
        o.quantity = r.quantity;
        o.id = r.new_id;
        id_to_handle_.erase(r.old_id);
        assert(!id_to_handle_.contains(r.new_id));
        id_to_handle_[r.new_id] = h;
        push_back(level_of(o.side, o.price), h);

        if (o.side == Side::Buy) {
            if (o.price >= best_bid_) best_bid_ = o.price;
            else update_best_bid(best_bid_);
        }
        else {
            if (o.price <= best_ask_) best_ask_ = o.price;
            else update_best_ask(best_ask_);
        }
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

};


#endif