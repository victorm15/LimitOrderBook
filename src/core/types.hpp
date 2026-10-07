#ifndef TYPES_H
#define TYPES_H

#include <cstdint>
#include <numeric>
#include <cstring>

using Price = std::int32_t;
using Quantity = std::uint32_t;
using OrderId = std::uint64_t;
using Handle = std::uint32_t;

Handle NIL = std::numeric_limits<Handle>::max();
Price NO_BID = std::numeric_limits<Price>::min();
Price NO_ASK = std::numeric_limits<Price>::max();

enum class Side : uint8_t { Buy, Sell };

struct Order {
    OrderId id;
    Price price;
    Quantity quantity;
    Side side;
    Handle prev = NIL, next = NIL;
    bool operator==(const Order&) const = default;
};

struct Fill {
    OrderId maker;
    Price price;
    Quantity quantity;
    bool operator==(const Fill&) const = default;
};

struct BookConfig {
    Price base_price = 0;
    Price band_ticks = 2000;
    size_t arena_size = 10000;
    size_t number_of_levels = 1024;
};

struct StreamConfig {
    uint64_t seed = 12345;
    size_t   n_events = 1'000'000;
    int      p_cancel = 46, p_add = 52, p_reduce = 2;
    double   add_dist_lambda = 0.0, cancel_dist_lambda = 0.0;
    int      dist_scale_factor = 1;
    int      occupied_level_range = 0; // 0 when default range used
    int      p_join = 10;
    int      p_behind = 75;
    int      p_improve = 10;
};

struct StreamMeasurements {
    uint32_t level_deletions;
    uint32_t level_creations;
    Price average_gap;
    double fills_per_add;
    size_t average_occupied_levels;
    size_t max_live_orders;
    Price lowest_price;
    Price highest_price;
    Price avg_level_destruction_dist;
    Price avg_level_construction_dist;

};





#endif