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

Price NO_BID = std::numeric_limits<Price>::min();
Price NO_ASK = std::numeric_limits<Price>::max();
Handle NIL = std::numeric_limits<Handle>::max();

struct Level {
    Handle head = NIL, tail = NIL;
    Quantity total_quantity = 0;
    uint32_t total_orders = 0;
};

#endif