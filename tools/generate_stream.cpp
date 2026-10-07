
#include "../src/testing/differential.hpp"
#include "../src/benchmarking/benchmark.hpp"
#include <string>

int main(int argc, char** argv) {
    std::string file_name = argc > 1 ? argv[1] : "stream0";
    
    StreamConfig cfg;
    if (file_name == "stream1") {
        cfg.dist_scale_factor = 100;
    }
    else if (file_name == "stream2") {
        cfg.dist_scale_factor = 100;
        cfg.p_join = 0;
        cfg.p_behind = 50;
        cfg.p_improve = 0;
        // cfg.p_aggressive = 50;
    }
    else if (file_name == "stream3") {
        cfg.dist_scale_factor = 200;
        cfg.p_join = 0;
        cfg.p_behind = 50;
        cfg.p_improve = 0;
        // cfg.p_aggressive = 50;
    }
    else if (file_name == "stream4") {
        cfg.dist_scale_factor = 10;
        cfg.p_join = 0;
        cfg.p_behind = 50;
        cfg.p_improve = 0;
        // cfg.p_aggressive = 50;
    }
    else {
        file_name = "stream0";
    }


    std::cout << "Generating " << file_name << " with " << cfg.n_events << " events\n";
    StreamGenerator gen(cfg);
    std::vector<EventRecord> events;
    events.reserve(cfg.n_events);
    gen.generate_events(events, cfg.n_events);

    StreamAnalyser analyser;
    StreamMeasurements m = analyser.analyse(events);

    write_stream("streams/" + file_name + ".bin", events);
    write_meta("streams/" + file_name + ".json", cfg, m);
}