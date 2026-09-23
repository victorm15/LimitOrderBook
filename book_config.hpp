#ifndef BOOK_CONFIG_H
#define BOOK_CONFIG_H

#include "types.hpp"

struct ArrayBookConfig {
    Price base_price;
    Price band_ticks;
    size_t arena_size;
};
struct SortedVecBookConfig {
    size_t arena_size;
};

#endif