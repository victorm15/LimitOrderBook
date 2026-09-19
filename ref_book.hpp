#ifndef REF_BOOK_H
#define REF_BOOK_H

#include "events.hpp"
#include "sinks.hpp"
#include "snapshot.hpp"
#include <assert.h>

class RefBook {
private:
    std::vector<Order> orders_;

public:

    template <typename Sink> void add(const Add& a, Sink&& sink) {
        bool is_buy = a.side == Side::Buy;
        Side contra_side = is_buy ? Side::Sell : Side::Buy;
        Price best_price = is_buy ? best_ask() : best_bid();
        Quantity quantity_left = a.quantity; 
        while (quantity_left > 0) {

            if (is_buy ? (best_price > a.price) : (best_price < a.price)) { break; }

            for (size_t i = 0; i < orders_.size(); ++i) {
                // Vector is sorted by arrival order - first order matching price is highest priority

                if (orders_[i].price == best_price && orders_[i].side ==  contra_side) {
                    
                    if (quantity_left >= orders_[i].quantity) {
                        sink(Fill{orders_[i].id, orders_[i].price, orders_[i].quantity});
                        quantity_left -= orders_[i].quantity;
                        orders_.erase(orders_.begin() + i);
                        break;
                    }
                    else {
                        sink(Fill{orders_[i].id, orders_[i].price, quantity_left});
                        orders_[i].quantity -= quantity_left;
                        return;
                    }
                }
            }
            best_price = is_buy ? best_ask() : best_bid();

        }
        if (quantity_left > 0) {
            orders_.push_back(Order{a.id, a.price, quantity_left, a.side});
        }

    }
    
    void cancel(const Cancel& c) {
        for (auto it = orders_.begin(); it != orders_.end(); ++it) {
            if (it->id == c.id) {
                orders_.erase(it);
                return;
            }
        }
    }

    void reduce(const Reduce& r) {
        for (auto it = orders_.begin(); it != orders_.end(); ++it) {
            if (it->id == r.id) {
                assert(it->quantity > r.quantity_removed);
                it->quantity -= r.quantity_removed;
                return;
            }
        }
    }

    template <typename Sink> void execute(const Execute& e, Sink&& sink) {
        for (auto it = orders_.begin(); it != orders_.end(); ++it) {
            if (it->id == e.id) {
                assert(e.quantity <= it->quantity);
                if (e.quantity == it->quantity) {
                    sink(Fill{e.id, it->price, e.quantity});
                    orders_.erase(it);
                }
                else {
                    sink(Fill{e.id, it->price, e.quantity});
                    it->quantity -= e.quantity;
                }
                return;
            }
        }
    }

    template <typename Sink> void replace(const Replace& r) {
        for (auto it = orders_.begin(); it != orders_.end(); ++it) {
            if (it->id == r.old_id) {
                orders_.push_back(Order{r.new_id, r.price, r.quantity, it->side});
                orders_.erase(it);
                return;
            }
        }
    }

    Price best_bid() const { 
        Price max = NO_BID;
        for (size_t i = 0; i < orders_.size(); ++i) {
            if (orders_[i].side == Side::Buy && orders_[i].price > max) { 
                max = orders_[i].price; 
            }
        }
        return max;
    }

    Price best_ask() const { 
        Price min = NO_ASK;
        for (size_t i = 0; i < orders_.size(); ++i) {
            if (orders_[i].side == Side::Sell && orders_[i].price < min) { 
                min = orders_[i].price; 
            }
        }
        return min;
    }

    Quantity   quantity_at(Side side, Price price) const {
        Quantity sum = 0;
        for (size_t i = 0; i < orders_.size(); ++i) {
            if (orders_[i].price == price && orders_[i].side == side) {
                sum += orders_[i].quantity;
            }
        }
        return sum;
    }

    void  snapshot(Snapshot& snap) const {
        snap = orders_;
        std::stable_sort(snap.begin(), snap.end(), [](const auto& a, const auto& b) {
            if (a.side != b.side) return a.side == Side::Buy;
            if (a.side == Side::Buy) return a.price > b.price;
            return a.price < b.price;
        });
    }

};

#endif