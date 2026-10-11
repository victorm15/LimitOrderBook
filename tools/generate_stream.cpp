
#include "../src/testing/differential.hpp"
#include "../src/benchmarking/benchmark.hpp"
#include <string>

int main(int argc, char** argv) {
    std::string file_name = argc > 1 ? argv[1] : "stream0";
    
    StreamConfig cfg;
    if (file_name == "stream1") {
        cfg.dist_scale_factor = 100;
        cfg.cancel_dist_lambda = -0.5;
    }
    else if (file_name == "stream2") {
        cfg.dist_scale_factor = 100;
        cfg.cancel_dist_lambda = -0.4;
    }
    else if (file_name == "stream3") {
        cfg.dist_scale_factor = 100;
        cfg.cancel_dist_lambda = -0.3;
    }
    else if (file_name == "stream4") {
        cfg.dist_scale_factor = 100;
        cfg.cancel_dist_lambda = -0.2;
    }
    else if (file_name == "stream5") {
        cfg.dist_scale_factor = 100;
        cfg.cancel_dist_lambda = -0.1;
    }
    else if (file_name == "stream6") {
        cfg.dist_scale_factor = 100;
        cfg.cancel_dist_lambda = 0.0;
    }
    else if (file_name == "stream7") {
        cfg.dist_scale_factor = 100;
        cfg.cancel_dist_lambda = 0.1;
    }
    else if (file_name == "stream8") {
        cfg.dist_scale_factor = 100;
        cfg.cancel_dist_lambda = 0.2;
    }
    else if (file_name == "stream9") {
        cfg.dist_scale_factor = 100;
        cfg.cancel_dist_lambda = 0.3;
    }
    else if (file_name == "stream10") {
        cfg.dist_scale_factor = 100;
        cfg.cancel_dist_lambda = 0.4;
    }
    else if (file_name == "stream11") {
        cfg.dist_scale_factor = 100;
        cfg.cancel_dist_lambda = 0.5;
    }
    else {
        file_name = "stream0";
    }


    std::cout << "Generating " << file_name << " with " << cfg.n_events << " events\n";
    StreamGenerator gen(cfg);
    std::vector<EventRecord> events;
    events.reserve(cfg.n_events);
    gen.generate_events(events, cfg.n_events);
    std::cout << "Stream generated\n";
    std::cout << "Analysing stream\n";
    StreamAnalyser analyser;
    StreamMeasurements m = analyser.analyse(events);
    std::cout << "Stream analysed\n";
    

    write_stream("streams/" + file_name + ".bin", events);
    write_meta("streams/" + file_name + ".json", cfg, m);
}