#ifndef BENCHMARK_H
#define BENCHMARK_H

#include "stream_generator.hpp"
#include "stream_analyser.hpp"
#include "stream_io.hpp"
#include "pmc_cycles.hpp"
#include <string>
#include <cassert>

template <typename Book> void run_untimed(Book& book, std::vector<EventRecord>& events, size_t n_warm) {
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
                uint64_t before = pmc.read();
                book.add(a, NullSink{});
                uint64_t after = pmc.read();
                cycles[i - n_warm] = uint32_t(pmc.delta(before, after));
                break;
            }
            case EventType::Cancel: {
                Cancel c{events[i].id};
                uint64_t before = pmc.read();
                book.cancel(c);
                uint64_t after = pmc.read();
                cycles[i - n_warm] = uint32_t(pmc.delta(before, after));
                break;
            }   
            case EventType::Reduce: {
                Reduce r{events[i].id, events[i].quantity};
                uint64_t before = pmc.read();
                book.reduce(r);
                uint64_t after = pmc.read();
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
        const uint64_t t0 = pmc.read();
        const uint64_t t1 = pmc.read();
        v[i] = uint32_t(pmc.delta(t0, t1));
    }
    std::sort(v.begin(), v.end());
    return v[v.size() / 2];
}

template <typename Book> BenchTimestamps benchmark(BookConfig& cfg, std::vector<EventRecord>& events, std::vector<uint32_t>& book_timings, 
    size_t n_warm, size_t n_events, uint32_t timer_overhead, PmcCycles& pmc, int trials) {
    
    
    assert(n_warm < n_events);
    std::vector<uint32_t> p50_ts;
    std::vector<uint32_t> p99_ts;
    std::vector<uint32_t> p999_ts;
    std::vector<uint32_t> max_ts;
    for (int i = 0; i < trials; ++i) {
        Book book(cfg);
        run_untimed(book, events, n_warm);
        run_timed(book, events, n_events, n_warm, book_timings, pmc);
        asm volatile("" : : "r,m"(book.best_bid) : "memory");
        asm volatile("" : : "r,m"(book.best_ask) : "memory");

        for (uint32_t& t : book_timings) t = (t > timer_overhead) ? t - timer_overhead : 0;
        std::sort(book_timings.begin(), book_timings.end());

        p50_ts.push_back(book_timings[size_t((book_timings.size()-1)*0.5)]);
        p99_ts.push_back(book_timings[size_t((book_timings.size()-1)*0.99)]);
        p999_ts.push_back(book_timings[size_t((book_timings.size()-1)*0.999)]);
        max_ts.push_back(book_timings.back());
    }
    std::nth_element(p50_ts.begin(), p50_ts.begin() + p50_ts.size()/2, p50_ts.end()); 
    std::nth_element(p99_ts.begin(), p99_ts.begin() + p99_ts.size()/2, p99_ts.end()); 
    std::nth_element(p999_ts.begin(), p999_ts.begin() + p999_ts.size()/2, p999_ts.end()); 
    std::nth_element(max_ts.begin(), max_ts.begin() + max_ts.size()/2, max_ts.end()); 

    return BenchTimestamps{
        .p50 = p50_ts[p50_ts.size()/2],
        .p99 = p99_ts[p99_ts.size()/2],
        .p999 = p999_ts[p999_ts.size()/2],
        .max = max_ts[max_ts.size()/2]
    };
    
}


#endif