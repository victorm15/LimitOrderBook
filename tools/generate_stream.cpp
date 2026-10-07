
#include "../src/testing/differential.hpp"
#include "../src/benchmarking/benchmark.hpp"
#include <string>

int main(int argc, char** argv) {
    std::string file_name = argc > 1 ? argv[1] : "stream0";
    
    StreamConfig cfg;
    
    if (file_name == "stream1") {
        cfg.dist_scale_factor = 100;
    }
    else {
        file_name = "stream0";
    }


    std::cout << "Generating " << file_name << " with " << cfg.n_events << "events\n";
    StreamGenerator gen(cfg);
    std::vector<EventRecord> events;
    events.reserve(cfg.n_events);
    gen.generate_events(events, cfg.n_events);

    StreamAnalyser analyser;
    StreamMeasurements m = analyser.analyse(events);

    write_stream("streams/" + file_name + ".bin", events);
    write_meta("streams/" + file_name + ".json", cfg, m);
}