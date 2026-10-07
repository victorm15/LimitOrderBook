#ifndef BENCHMARK_H
#define BENCHMARK_H

#include "stream_generator.hpp"
#include "stream_analyser.hpp"
#include "stream_io.hpp"
#include "pmc_cycles.hpp"
#include <string>

// #if defined(__x86_64__)
//     #include <x86intrin.h>
// #endif

// static inline uint64_t get_tsc() {
//     unsigned aux;
//     uint64_t t = __rdtscp(&aux);
//     _mm_lfence();
//     return t;
// }

template <typename Book> void run_untimed(Book& book, std::vector<EventRecord>& events, size_t n_events, size_t n_warm) {
    for (size_t i = 0; i < n_warm; ++i) {
        switch(events[i].event_type) {
            case EventType::Add:
                book.add(Add{events[i].id, events[i].price, events[i].quantity, events[i].side}, NullSink{});
                break;
            case EventType::Cancel:
                book.cancel(Cancel{events[i].id});
                break;
            case EventType::Reduce:
                book.reduce(Reduce{events[i].id, events[i].quantity});
                break;
            default:
                break;
        }
    }
}

template <typename Book> void run_timed(Book& book, std::vector<EventRecord>& events, size_t n_events, size_t n_warm,
std::vector<uint32_t>& cycles, PmcCycles& pmc) {
    for (size_t i = n_warm; i < n_events; ++i) {
        switch(events[i].event_type) {
            case EventType::Add: {
                Add a{events[i].id, events[i].price, events[i].quantity, events[i].side};
                // uint64_t before = get_tsc();
                uint64_t before = pmc.read();
                book.add(a, NullSink{});
                // uint64_t after = get_tsc();
                uint64_t after = pmc.read();
                // cycles[i - n_warm] = uint32_t(after - before); 
                cycles[i - n_warm] = uint32_t(pmc.delta(before, after));
                break;
            }
            case EventType::Cancel: {
                Cancel c{events[i].id};
                // uint64_t before = get_tsc();
                uint64_t before = pmc.read();
                book.cancel(c);
                // uint64_t after = get_tsc();
                uint64_t after = pmc.read();
                // cycles[i - n_warm] = uint32_t(after - before); 
                cycles[i - n_warm] = uint32_t(pmc.delta(before, after));
                break;
            }   
            case EventType::Reduce: {
                Reduce r{events[i].id, events[i].quantity};
                // uint64_t before = get_tsc();
                uint64_t before = pmc.read();
                book.reduce(r);
                // uint64_t after = get_tsc();
                uint64_t after = pmc.read();
                // cycles[i - n_warm] = uint32_t(after - before); 
                cycles[i - n_warm] = uint32_t(pmc.delta(before, after));
                break;
            }
            default:
                break;
        }
    }
}

inline uint32_t measure_timer_overhead(PmcCycles& pmc, size_t n = 1'000'000) {
    std::vector<uint32_t> v(n);
    for (size_t i = 0; i < n; ++i) {
        // const uint64_t t0 = get_tsc();
        // const uint64_t t1 = get_tsc();
        // v[i] = uint32_t(t1 - t0);
        const uint64_t t0 = pmc.read();
        const uint64_t t1 = pmc.read();
        v[i] = uint32_t(pmc.delta(t0, t1));
    }
    std::sort(v.begin(), v.end());
    return v[v.size() / 2];
}

template <typename Book> void benchmark(Book& book, std::string book_name, std::vector<EventRecord>& events, 
    std::vector<uint32_t>& book_timings, size_t n_warm, size_t n_events, uint32_t timer_overhead, PmcCycles& pmc) {

    run_untimed(book, events, n_events, n_warm);
    run_timed(book, events, n_events, n_warm, book_timings, pmc);
    std::cout << book_name << ":\n" << "best bid: " << book.best_bid() << "best ask: " << book.best_ask() << "\n";

    for (uint32_t& t : book_timings) t = (t > timer_overhead) ? t - timer_overhead : 0;
    std::sort(book_timings.begin(), book_timings.end());


    // uint32_t prev = book_timings[0];
    // std::cout << "{" << prev << ", ";
    // for (size_t i = 1; i < book_timings.size(); ++i) {
    //     if (book_timings[i] != prev) {
    //         std::cout << book_timings[i] << ", ";
    //         prev = book_timings[i];
    //     }
    // }
    // std::cout << "}\n";


    std::cout << "p50: " << book_timings[size_t((book_timings.size()-1)*0.5)] 
    << "\np90: " << book_timings[size_t((book_timings.size()-1)*0.9)] 
    << "\np99.9: " << book_timings[size_t((book_timings.size()-1)*0.999)] 
    << "\nmax: " << book_timings.back()
    << "\n-----------------------\n";
}

#endif