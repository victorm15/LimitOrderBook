
#include "../src/testing/differential.hpp"
#include "../src/benchmarking/benchmark.hpp"
#include <string>

int main(int argc, char** argv) {
    std::string stream_name = argc > 1 ? argv[1] : "stream0";
    std::vector<StreamConfig> cfgs;
    std::vector<std::string> stream_names;

    if (stream_name == "lambda_sf_grid") {
        std::vector<double> dist_lambdas = {0.0, 0.01, 0.05, 0.1, 0.2, 0.3, 0.4, 0.5};
        std::vector<int> scale_factors = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 20, 50, 100};
        for (size_t r = 0; r < dist_lambdas.size(); ++r) {
            for (size_t i = 0; i < scale_factors.size(); ++i) {
                cfgs.push_back(StreamConfig{.dist_scale_factor = dist_lambdas[r], 
                    .cancel_dist_lambda = scale_factors[i]});
                stream_names.push_back("stream_cdlsf_grid" + std::to_string(int(dist_lambdas[r] * 100)) + 
                "-" + std::to_string(scale_factors[i]));
            }
        }
    }
    else {
        cfgs.push_back(Streamconfig{});
        stream_names.push_back("stream0");
    }
    for (size_t i = 0; i < cfgs.size(); ++i) {
        std::string& file_name = stream_names[i];
        StreamConfig& cfg = cfgs[i];
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
    return 0;    
}