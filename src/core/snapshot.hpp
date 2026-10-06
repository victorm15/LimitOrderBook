#ifndef SNAPSHOT_H
#define SNAPSHOT_H

#include <vector>
#include <iostream>
#include "types.hpp"

// Sorted by: bids, highest to lowest, then asks, lowest to highest
struct SnapEntry {
    OrderId id;
    Price price;
    Quantity quantity;
    Side side;
    bool operator==(const SnapEntry&) const = default;
};
using Snapshot = std::vector<SnapEntry>;

inline void print_snapshot(const Snapshot& snapshot) {
    std::cout << "========================\n";
    std::cout << "Book:\n";
    for (const SnapEntry& ord : snapshot) {
        std::cout << ord.id << ": " << ((ord.side == Side::Buy) ? " Buy" : "Sell") << ", Price: " 
        << ord.price << ", Quantity: " << ord.quantity << "\n";
    }            
    std::cout << "========================\n\n";
}



#endif