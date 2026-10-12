
#include "../src/testing/differential.hpp"
#include "../src/benchmarking/benchmark.hpp"
#include <string>

int main(int argc, char** argv) {
    std::string file_name = argc > 1 ? argv[1] : "stream0";

    std::cout << "Reading " << file_name << "\n";
    
    std::vector<EventRecord> events = read_stream("streams/" + file_name + ".bin");
    std::cout << "Analysing" << file_name << "\n";
    StreamAnalyser analyser;
    StreamMeasurements m = analyser.analyse(events);

    std::cout << "BBOS DELETED: " << m.bbos_deleted << "\n";


    
}