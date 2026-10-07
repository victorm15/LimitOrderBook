
#include "../src/testing/differential.hpp"
#include "../src/benchmarking/benchmark.hpp"
#include <string>

int main(int argc, char** argv) {
    std::string file_name = argc > 1 ? argv[1] : "stream0";
    uint64_t seed = argc > 2 ? std::atoi(argv[2]) : 12345;
    StreamConfig cfg{.seed = seed, .n_events = 1'000'000, .dist_scale_factor = 10};
    std::cout << "Generating " << file_name << " with " << cfg.n_events << "events\n";
    StreamGenerator gen(cfg);
    std::vector<EventRecord> events;
    events.reserve(cfg.n_events);
    gen.generate_events(events, cfg.n_events);

    StreamAnalyser analyser;
    StreamMeasurements m = analyser.analyse(events);

    write_stream("../src/benchmarking/streams/" + file_name + ".bin", events);
    write_meta("../src/benchmarking/streams/" + file_name + ".json", cfg, m);
}