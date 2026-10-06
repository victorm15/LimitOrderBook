#ifndef STREAM_ANALYSER_H
#define STREAM_ANALYSER_H


#include <map>
#include <unordered_map>
#include <cassert>
#include "../books/ref_book.hpp"
#include "../core/events.hpp"
#include "../core/types.hpp"
#include "../core/sinks.hpp"



class StreamAnalyser {

private:

    struct OrderData {
        Price p;
        Side s;
        Quantity q;
    };

    uint32_t levels_created_ = 0, levels_destroyed_ = 0;
    std::vector<Price> level_creation_dists_, level_destruction_dists_;
    std::vector<Price> median_gap_between_levels_;
    std::vector<size_t> occupied_levels_;
    uint32_t sum_fills_ = 0;
    uint32_t sum_adds_ = 0;

    std::map<Price, uint32_t> bids_, asks_;
    std::unordered_map<OrderId, OrderData> live_;

    size_t max_live_orders_ = 0;

    Price lowest_price_ = NO_ASK;
    Price highest_price_ = NO_BID;


    RefBook book;
    VecSink sink;


    void on_order_added(Price p, Side s, Quantity q, OrderId id, Price best_price) {
        auto& m = s == Side::Buy ? bids_ : asks_;
        if (m.contains(p)) ++m[p];
        else {
            m[p] = 1;
            ++levels_created_;
            if (best_price != NO_ASK && best_price != NO_BID) {
                level_creation_dists_.push_back(s == Side::Buy ? best_price - p : p - best_price);
            }
        }
        live_[id] = {p, s, q};
        if (live_.size() > max_live_orders_) max_live_orders_ = live_.size();
    }
    void on_order_removed(Side s, OrderId id, Price best_price) {
        auto& m = s == Side::Buy ? bids_ : asks_;
        assert(live_.contains(id));
        Price p = live_[id].p;
        assert(m.contains(p));
        if (--m[p] == 0) {
            ++levels_destroyed_;
            if (best_price != NO_ASK && best_price != NO_BID) {
                level_destruction_dists_.push_back(s == Side::Buy ? best_price - p : p - best_price);
            }
            m.erase(p);
        }
        live_.erase(id);
    }

    template <typename T> T to_median(std::vector<T>& vec) {
        assert(vec.size() > 1);
        std::sort(vec.begin(), vec.end());
        return vec[vec.size()/2];
    }

public:



    StreamMeasurements analyse(const std::vector<EventRecord>& events) {

        for (size_t i = 0; i < events.size(); ++i) {
            const EventRecord& event = events[i];
            if (event.event_type == EventType::Add) {
                    if (event.price > highest_price_) highest_price_ = event.price;
                    if (event.price < lowest_price_) lowest_price_ = event.price;
                    ++sum_adds_;
                    Price prev_best_bid = book.best_bid();
                    Price prev_best_ask = book.best_ask();
                    sink.clear();
                    book.add(Add{.id = event.id, .price = event.price, .quantity = event.quantity, .side = event.side}, sink);
                    // Add doesn't cross - rests
                    if ((event.side == Side::Buy) ? event.price < prev_best_ask : event.price > prev_best_bid) {
                        on_order_added(event.price, event.side, event.quantity, event.id, event.side == Side::Buy ? prev_best_bid : prev_best_ask);
                    }
                    else {
                        Quantity quantity_left = event.quantity;
                        for (const Fill& f : sink.trades) {
                            ++sum_fills_;
                            quantity_left -= f.quantity;
                            OrderData& maker = live_[f.maker];
                            if (f.quantity == maker.q) on_order_removed(maker.s, f.maker, event.side == Side::Buy ? prev_best_bid : prev_best_ask);
                            else maker.q -= f.quantity;
                        }
                        // Add rested
                        if (quantity_left > 0) {
                            on_order_added(event.price, event.side, event.quantity, event.id, event.side == Side::Buy ? prev_best_bid : prev_best_ask);
                        }
                    }

            }
            else if (event.event_type == EventType::Cancel) {
                on_order_removed(live_[event.id].s, event.id, event.side == Side::Buy ? book.best_bid() : book.best_ask());
                book.cancel(Cancel{.id = event.id});
            }

            else if (event.event_type == EventType::Reduce) {
                live_[event.id].q -= event.quantity;
                book.reduce(Reduce{.id = event.id, .quantity_removed = event.quantity});
            }
            
            if (i != 0 && i % 100 == 0) {
                std::vector<Price> gaps;
                if (bids_.size() > 1 && asks_.size() > 1) {
                    Price prev = bids_.begin()->first;
                    for (auto it = ++bids_.begin(); it != bids_.end(); ++it) {
                        gaps.push_back(it->first - prev);
                        prev = it->first;
                    }
                    prev = asks_.begin()->first;
                    for (auto it = ++asks_.begin(); it != asks_.end(); ++it) {
                        gaps.push_back(it->first - prev);
                        prev = it->first;
                    }
                    std::sort(gaps.begin(), gaps.end());
                    median_gap_between_levels_.push_back(gaps[gaps.size()/2]);
                }
                
                occupied_levels_.push_back(bids_.size());
                occupied_levels_.push_back(asks_.size());


            }



        }

        Price avg_gap = to_median(median_gap_between_levels_);
        size_t avg_occ_lvls = to_median(occupied_levels_);
        Price avg_creation_dist = to_median(level_creation_dists_);
        Price avg_destruction_dist = to_median(level_destruction_dists_);

        return StreamMeasurements{
            .level_deletions = levels_destroyed_,
            .level_creations = levels_created_,
            .average_gap = avg_gap,
            .fills_per_add = double(sum_fills_)/sum_adds_,
            .average_occupied_levels = avg_occ_lvls,
            .max_live_orders = max_live_orders_,
            .lowest_price = lowest_price_,
            .highest_price = highest_price_,
            .avg_level_destruction_dist = avg_destruction_dist,
            .avg_level_construction_dist = avg_creation_dist
            
        };


    }



};






#endif