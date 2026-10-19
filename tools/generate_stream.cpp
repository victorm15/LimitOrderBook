
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
                cfgs.push_back(StreamConfig{.cancel_dist_lambda = dist_lambdas[r],
                    .dist_scale_factor = scale_factors[i]});
                stream_names.push_back("stream_cdlsf_grid" + std::to_string(int(dist_lambdas[r] * 100)) + 
                "-" + std::to_string(scale_factors[i]));
            }
        }
    }
    else if (stream_name == "add_lambda") {
        std::vector<double> add_lambdas = {0.0, -0.01, -0.05, -0.1, -0.2, -0.3, -0.4, -0.5};
        std::vector<int> scale_factors = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 20, 50, 100};
        for (size_t r = 0; r < add_lambdas.size(); ++r) {
            for (size_t i = 0; i < scale_factors.size(); ++i) {
                cfgs.push_back(StreamConfig{.add_dist_lambda = add_lambdas[r],
                    .dist_scale_factor = scale_factors[i]});
                stream_names.push_back("stream_adlsf_grid" + std::to_string(int(add_lambdas[r] * 100)) + 
                "-" + std::to_string(scale_factors[i]));
            }
        }
    }
    else if (stream_name == "level_churn") {
        std::vector<double> add_lambdas = {0.5, 0.4, 0.3, 0.2, 0.1, 0.05, 0.01, 0.0, -0.01, -0.05, -0.1, -0.2, -0.3, -0.4, -0.5};
        std::vector<double> cancel_lambdas = {0.5, 0.4, 0.3, 0.2, 0.1, 0.05, 0.01, 0.0, -0.01, -0.05, -0.1, -0.2, -0.3, -0.4, -0.5};
        int occupied_level_range = 300;
        for (size_t r = 0; r < add_lambdas.size(); ++r) {
            for (size_t i = 0; i < cancel_lambdas.size(); ++i) {
                cfgs.push_back(StreamConfig{.add_dist_lambda = add_lambdas[r],
                    .cancel_dist_lambda = cancel_lambdas[i],
                    .occupied_level_range = occupied_level_range});
                stream_names.push_back("stream_lvlchurn_grid" + std::to_string(int(add_lambdas[r] * 100)) + 
                "-" + std::to_string(int(cancel_lambdas[i] * 100)));
            }
        }
    }
    else if (stream_name == "level_churn_no_warm") {
        std::vector<double> add_lambdas = {0.5, 0.4, 0.3, 0.2, 0.1, 0.05, 0.01, 0.0, -0.01, -0.05, -0.1, -0.2, -0.3, -0.4, -0.5};
        std::vector<double> cancel_lambdas = {0.5, 0.4, 0.3, 0.2, 0.1, 0.05, 0.01, 0.0, -0.01, -0.05, -0.1, -0.2, -0.3, -0.4, -0.5};
        int occupied_level_range = 300;
        for (size_t r = 0; r < add_lambdas.size(); ++r) {
            for (size_t i = 0; i < cancel_lambdas.size(); ++i) {
                cfgs.push_back(StreamConfig{.add_dist_lambda = add_lambdas[r],
                    .cancel_dist_lambda = cancel_lambdas[i],
                    .occupied_level_range = occupied_level_range});
                stream_names.push_back("stream_lvlchurn_grid_nw" + std::to_string(int(add_lambdas[r] * 100)) + 
                "-" + std::to_string(int(cancel_lambdas[i] * 100)));
            }
        }
    }
    else {
        cfgs.push_back(StreamConfig{});
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