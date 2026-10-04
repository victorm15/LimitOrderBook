#ifndef STREAM_GENERATOR_H
#define STREAM_GENERATOR_H

#include <random>
#include <unordered_map>
#include <cassert>
#include <cmath>
#include "../core/types.hpp"
#include "../core/events.hpp"
#include "../books/ref_book.hpp"
#include "../core/sinks.hpp"





class StreamGenerator {

private:

    std::mt19937 rng{12345};
    std::uniform_int_distribution<int> dist{0,99};

    Price mid_ = 22000;

    int p_cancel_ = 46, p_add_ = 52, p_reduce_ = 2;

    // Add Order
    int p_aggressive_ = 5, p_improve_ = 10, p_join_ = 10, p_behind_ = 75;
    
    std::unordered_map<Quantity,int> passive_add_qty_distribution_ = {{0,29},{100,58},
    {200,8},{300,3},{400,2}}; // 0-50, then only round numbers
    std::unordered_map<Quantity,int> aggressive_add_qty_distribution_ = {{0,36},{50,12},{100,37},{150,2},{200,7},{250,5},{300,1}};
    // Probability of having a round number within a range (e.g. 100 for the range [100,150[ )
    int p_round_aggressive_add_ = 85;
    // How deep should aggressive orders cross
    int p_at_contra_best_ = 85, p_one_tick_ = 10, p_two_ticks_ = 5; 
    // Distance from best price for adds behind the best price
    std::unordered_map<Price,double> behind_add_dist_distribution_ = {{1,7.0},{2,7.0},{3,5.0},{4,5.0},{5,6.0},{6,6.0},{7,7.0},{8,7.0},
    {9,8.0},{10,9.0},{11,11.0},{12,8.0},{13,6.0},{14,5.0},{15,3.0}};
    int dist_scale_factor_ = 1; // book sparsity/density

    // Cancel Order
    std::unordered_map<Price, double> cancel_dist_distribution_ = {{0,15.0},{1,6.0},{2,5.0},{3,5.0},{4,5.0},{5,5.0},{6,5.0},{7,7.0},
    {8,7.0},{9,7.0},{10,7.0},{11,7.0},{12,6.0},{13,5.0},{14,3.0},{15,2.0},{16,1.0},{17,1.0},{18,1.0}};

    // Reduce Order (at 2%, we very roughly model reduce events)
    Price reduce_median_dist_ = 10;
    Quantity reduce_quantity_ = 100; // 85% of reduce quantities are 100
    
    Price initial_spread_ = 13;
    int p_mid_drift_ = 10;

    OrderId id_counter_ = 0;
    RefBook book;

    Side get_side() { return dist(rng) >= 50 ? Side::Buy : Side::Sell; }

    Quantity get_passive_add_quantity() {
        int p_quantity = dist(rng);
        if (p_quantity < passive_add_qty_distribution_[0]) {
            return std::uniform_int_distribution(1,49)(rng);
        }
        p_quantity -= passive_add_qty_distribution_[0];
        if (p_quantity < passive_add_qty_distribution_[100]) {
            return 100;
        }
        p_quantity -= passive_add_qty_distribution_[100];

        if (p_quantity < passive_add_qty_distribution_[200]) {
            return 200;
        }
        p_quantity -= passive_add_qty_distribution_[200];
        if (p_quantity < passive_add_qty_distribution_[300]) {
            return 300;
        }
        return 400;
    }

    Quantity get_aggressive_add_quantity() {
        int p_quantity = dist(rng);
        if (p_quantity < aggressive_add_qty_distribution_[0]) {
            return std::uniform_int_distribution(1,49)(rng);
        }
        p_quantity -= aggressive_add_qty_distribution_[0];
        
        for (int i = 50; i <= 250; i+= 100) {
            if (p_quantity < aggressive_add_qty_distribution_[i]) {
                return i + std::uniform_int_distribution(0,49)(rng);
            }
            p_quantity -= aggressive_add_qty_distribution_[i];
            // Interval with round number
            if (p_quantity < aggressive_add_qty_distribution_[i+50]) {
                bool is_round = dist(rng) < p_round_aggressive_add_;
                if (is_round) return i+50;
                return i+50+std::uniform_int_distribution(1,49)(rng);
            }
            p_quantity -= aggressive_add_qty_distribution_[i+50];
        }
        bool is_round = dist(rng) < p_round_aggressive_add_;
        if (is_round) return 300;
        return 300+std::uniform_int_distribution(1,49)(rng);
    }

    int get_behind_add_distance() {
        double p_behind_dist = double(dist(rng));
        for (int i = 1; i <= 15; ++i) {
            if (p_behind_dist < behind_add_dist_distribution_[i]) return i*dist_scale_factor_;
            p_behind_dist -= behind_add_dist_distribution_[i];
        }
        return 15*dist_scale_factor_; 
    }

    bool add_add_event(std::vector<EventRecord>& events) {
        int p_add_type = dist(rng);
        Side add_side = get_side();
        
        Price best_bid = book.best_bid();
        Price best_ask = book.best_ask();
        if (best_bid == NO_BID && best_ask == NO_ASK) {
            best_bid = mid_ - initial_spread_/2;
            best_ask = best_bid + initial_spread_;
        }
        else if (best_bid == NO_BID) best_bid = best_ask - initial_spread_;
        else if (best_ask == NO_ASK) best_ask = best_bid + initial_spread_;

        // Add order behind BBO
        if (p_add_type < p_behind_) {
            int add_dist = get_behind_add_distance();   
            Add add = Add{
                .id = id_counter_++, 
                .price = add_side == Side::Buy ? best_bid - add_dist : best_ask + add_dist,
                .quantity = get_passive_add_quantity(),
                .side = add_side};
            book.add(add, NullSink{});
            events.push_back(EventRecord{.event_type = EventType::Add, .id = add.id, 
                .price = add.price, .quantity = add.quantity, .side = add.side});
            return true;
        }
        p_add_type -= p_behind_;

        // Add order joins BBO
        if (p_add_type < p_join_) {
            Add add = Add{
                .id = id_counter_++, 
                .price = add_side == Side::Buy ? best_bid : best_ask,
                .quantity = get_passive_add_quantity(),
                .side = add_side};
            book.add(add, NullSink{});
            events.push_back(EventRecord{.event_type = EventType::Add, .id = add.id, 
                .price = add.price, .quantity = add.quantity, .side = add.side});
            return true;
            
        }
        p_add_type -= p_join_;

        // Add order inside spread 
        if (p_add_type < p_improve_) {
            int spread = best_ask-best_bid;
            int increase = 0;
            if (spread > 1) increase = std::uniform_int_distribution(1,spread-1)(rng);
            Add add = Add{
                .id = id_counter_++, 
                .price = add_side == Side::Buy ? best_bid + increase : best_ask - increase,
                .quantity = get_passive_add_quantity(),
                .side = add_side};
            book.add(add, NullSink{});
            events.push_back(EventRecord{.event_type = EventType::Add, .id = add.id, 
                .price = add.price, .quantity = add.quantity, .side = add.side});
            return true;
        }

        // Aggressive add order
        int p_aggressive_ticks = dist(rng);
        Price ticks;
        if (p_aggressive_ticks < p_at_contra_best_) ticks = 0;
        else {
            p_aggressive_ticks -= p_at_contra_best_;
            if (p_aggressive_ticks < p_one_tick_) ticks = 1;
            else ticks = 2;
        }
        Add add = Add{
                .id = id_counter_++, 
                .price = add_side == Side::Buy ? best_ask + ticks : best_bid - ticks,
                .quantity = get_aggressive_add_quantity(),
                .side = add_side};
        book.add(add, NullSink{});
        events.push_back(EventRecord{.event_type = EventType::Add, .id = add.id, 
                .price = add.price, .quantity = add.quantity, .side = add.side});
        return true;     
    }

    int get_cancel_distance() {
        double p_cancel_dist = double(dist(rng));
        for (int i = 0; i <= 18; ++i) {
            if (p_cancel_dist < cancel_dist_distribution_[i]) return i*dist_scale_factor_;
            p_cancel_dist -= cancel_dist_distribution_[i];
        }
        return 18*dist_scale_factor_; 
    }

    bool add_cancel_event(std::vector<EventRecord>& events) {
        Side side = get_side();
        Price best_price = side == Side::Buy ? book.best_bid() : book.best_ask();
        if (best_price == NO_BID || best_price == NO_ASK) return false;
        Price dist = get_cancel_distance();
        Price p = side == Side::Buy ? best_price - dist : best_price + dist;
        const Order* nearest_order = book.nearest_order(side, p, rng);
        if (nearest_order != nullptr) {
            Cancel ccl = Cancel{.id = nearest_order->id};
            book.cancel(ccl);
            events.push_back(EventRecord{.event_type = EventType::Cancel, .id = ccl.id});
            return true;
        }
        return false;
    }

    bool add_reduce_event(std::vector<EventRecord>& events) {
        Side side = get_side();
        Price p = side==Side::Buy ? 
        book.best_bid() - reduce_median_dist_: 
        book.best_ask() + reduce_median_dist_;
        const Order* nearest_order = book.nearest_order(side, p, rng);
        if (nearest_order != nullptr) {
            Quantity rdc_quantity = reduce_quantity_ < nearest_order->quantity ? 
            reduce_quantity_ : nearest_order->quantity/2; // If order is smaller than 100, just remove half 
            Reduce rdc = Reduce{nearest_order->id, rdc_quantity};
            book.reduce(rdc);
            events.push_back(EventRecord{.event_type = EventType::Reduce, .id = rdc.id, .quantity = rdc.quantity_removed});
            return true;
        }
        return false;
    }
    

public:
    
    StreamGenerator(StreamConfig& cfg)
    : p_cancel_{cfg.p_cancel}, 
    dist_scale_factor_{cfg.dist_scale_factor} {
        assert(p_cancel_ + p_reduce_ <= 100);
        p_add_ = 100 - p_cancel_ - p_reduce_;
        if (cfg.occupied_level_range > 0 ) {
            behind_add_dist_distribution_.clear();
            for (int i = 1; i < cfg.occupied_level_range; ++i) {
                behind_add_dist_distribution_[i] = 100.0/cfg.occupied_level_range;
            }
            cancel_dist_distribution_.clear();
            for (int i = 0; i < cfg.occupied_level_range; ++i) {
                cancel_dist_distribution_[i] = 100.0/cfg.occupied_level_range;
            }
        }

        // lambda = 0 - normal distribution, lambda > 0 closer to touch, lambda < 0 further from touch
        double sum = 0.0;
        for (int i = 1; i <= 15; ++i) {
            behind_add_dist_distribution_[i] = exp(i*-cfg.add_dist_lambda)*behind_add_dist_distribution_[i];
            sum += behind_add_dist_distribution_[i];
        }
        for (int i = 1; i <= 15; ++i) {
            behind_add_dist_distribution_[i] = behind_add_dist_distribution_[i]/sum*100;
        }
        sum = cancel_dist_distribution_[0];
        for (int i = 1; i <= 18; ++i) {
            cancel_dist_distribution_[i] = exp(i*-cfg.cancel_dist_lambda)*cancel_dist_distribution_[i];
            sum += cancel_dist_distribution_[i];
        }
        for (int i = 0; i <= 18; ++i) {
            cancel_dist_distribution_[i] = cancel_dist_distribution_[i]/sum*100;
        }
    }

    StreamGenerator() {}

    void generate_events(std::vector<EventRecord>& events, size_t num_events) {
        size_t i = 0;
        while (i < num_events) {
            int p_event_type = dist(rng);
            int p_price_drift = dist(rng);
            if (p_event_type < p_add_) {
                if (add_add_event(events)) ++i;
                continue;
            }
            p_event_type -= p_add_;
            if (p_event_type < p_cancel_) {
                if (add_cancel_event(events)) ++i;
                continue;
            }
            if(add_reduce_event(events)) ++i;
        }
    }

    




};


#endif