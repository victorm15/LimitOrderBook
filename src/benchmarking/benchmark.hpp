#ifndef BENCHMARK_H
#define BENCHMARK_H

#include "stream_generator.hpp"
#include "stream_analyser.hpp"
#include "stream_io.hpp"
#include <string>


void gen_stream(StreamConfig& cfg, std::string file_name) {
    StreamGenerator gen(cfg);
    std::vector<EventRecord> events;
    events.reserve(cfg.n_events);
    gen.generate_events(events, cfg.n_events);

    StreamAnalyser analyser;
    StreamMeasurements m = analyser.analyse(events);

    write_stream("../src/benchmarking/streams/" + file_name + ".bin", events);
    write_meta("../src/benchmarking/streams/" + file_name + ".json", cfg, m);
}




#endif