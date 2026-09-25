#ifndef TYPES_H
#define TYPES_H

#include <cstdint>
#include <numeric>

using Price = std::int32_t;
using Quantity = std::uint32_t;
using OrderId = std::uint64_t;
using Handle = std::uint32_t;

enum class Side : uint8_t { Buy, Sell };

struct Order {
    OrderId id;
    Price price;
    Quantity quantity;
    Side side;
    Handle prev, next;
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

Price NO_BID = std::numeric_limits<Price>::min();
Price NO_ASK = std::numeric_limits<Price>::max();
Handle NIL = std::numeric_limits<Handle>::max();



#endif