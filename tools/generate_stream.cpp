
#include "../src/testing/differential.hpp"
#include "../src/benchmarking/benchmark.hpp"
#include <string>

int main(int argc, char** argv) {
    std::string file_name = argc > 1 ? argv[1] : "stream0";
    StreamConfig cfg;
    if (file_name == "stream_dist_lambda_testing") {
        std::vector<double> dist_lambdas = {-0.5, -0.4, -0.3, -0.2, -0.1, 0.0, 0.1, 0.2, 0.3, 0.4, 0.5};
        for (size_t i = 0; i < dist_lambdas.size(); ++i) {
            cfg.dist_scale_factor = 100;
            cfg.cancel_dist_lambda = dist_lambdas[i];

            file_name = "stream_dlt" + std::to_string(i);
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
    }
    else if (file_name == "stream_lambda_sf_grid") {
        std::vector<double> dist_lambdas = {0.0, 0.01, 0.05, 0.1, 0.2, 0.3, 0.4, 0.5};
        std::vector<int> scale_factors = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 20, 50, 100};

        for (size_t r = 0; r < dist_lambdas.size(); ++r) {
            for (size_t i = 0; i < scale_factors.size(); ++i) {
                cfg.cancel_dist_lambda = dist_lambdas[r];
                cfg.dist_scale_factor = scale_factors[i];

                file_name = "stream_cdlsf_grid" + std::to_string(int(dist_lambdas[r] * 100)) + 
                "-" + std::to_string(scale_factors[i]);
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
        }


    }
    else {
        file_name = "stream0";
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


    
}