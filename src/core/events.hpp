#ifndef EVENTS_H
#define EVENTS_H

#include <variant>

#include "types.hpp"

struct Add {
    OrderId id;
    Price price;
    Quantity quantity;
    Side side;
};
struct Cancel {
    OrderId id;
};
struct Execute {
    OrderId id;
    Quantity quantity;
};
struct Reduce {
    OrderId id;
    Quantity quantity_removed;
};
struct Replace {
    OrderId old_id, new_id;
    Price price;
    Quantity quantity;
};

using Event = std::variant<Add, Cancel, Execute, Reduce, Replace>;



#endif