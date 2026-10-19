#ifndef TYPES_H
#define TYPES_H

#include <cstdint>
#include <numeric>
#include <cstring>

using Price = std::int32_t;
using Quantity = std::uint32_t;
using OrderId = std::uint64_t;
using Handle = std::uint32_t;

inline constexpr Handle NIL = std::numeric_limits<Handle>::max();
inline constexpr Price NO_BID = std::numeric_limits<Price>::min();
inline constexpr Price NO_ASK = std::numeric_limits<Price>::max();
// inline constexpr size_t N_WARM = 100'000;
inline constexpr size_t N_WARM = 0;

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
    size_t heap_size = 1024;
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
    uint32_t bbos_deleted;

};
struct BenchTimestamps {
        uint32_t p50 = 0, p99 = 0, p999 = 0, max = 0, mean = 0;
};

#ifdef BOOK_STATS
    

    struct ArrayBookStats {
        uint64_t scans = 0;
        uint64_t scan_ticks = 0;
    };
    struct BitmapBookStats {
        uint64_t scans = 0;
        uint64_t scan_ticks = 0;
        uint64_t scan_words = 0;
    };
    struct HashBookStats {
        uint64_t best_calls = 0;
        uint64_t lazy_deletions = 0;
        uint64_t max_lazy_deletions = 0;
        uint64_t max_heap_size = 0;
    };
    struct SortedVecBookStats {
        uint64_t searches = 0;
        uint64_t search_steps = 0;
        uint64_t shifts = 0;
        uint64_t shifted_elements = 0;
    };
#endif
#endif