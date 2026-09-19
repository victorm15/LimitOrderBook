#ifndef SNAPSHOT_H
#define SNAPSHOT_H

#include <vector>
#include <iostream>
#include "types.hpp"


using Snapshot = std::vector<Order>;

void print_snapshot(const Snapshot& snapshot) {
    std::cout << "========================\n";
    std::cout << "Book:\n";
    for (const Order& ord : snapshot) {
        std::cout << ord.id << ": " << ((ord.side == Side::Buy) ? " Buy" : "Sell") << ", Price: " 
        << ord.price << ", Quantity: " << ord.quantity << "\n";
    }            
    std::cout << "========================\n\n";
}



#endif