#ifndef SINKS_H
#define SINKS_H

#include "types.hpp"
#include <vector>

struct NullSink {
    void operator()(const Fill&) const noexcept {}
};

struct VecSink {
    std::vector<Fill> trades;
    void operator()(const Fill& trade) {
        trades.push_back(trade);
    }
    void clear() {
        trades.clear();
    }
};


#endif