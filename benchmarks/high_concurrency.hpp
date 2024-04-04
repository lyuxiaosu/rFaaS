
#ifndef __TESTS__HIGH_CONCURRENCY_HPP__
#define __TESTS__HIGH_CONCURRENCY_HPP__

#include <string>

namespace high_concurrency {

  struct Options {

    std::string json_config;
    std::string device_database;
    std::string executors_database;
    std::string output_stats;
    bool verbose;
    std::vector<std::string> fnames;
    std::vector<std::string> flibs;
    std::vector<int> req_parameters;
    std::vector<int> req_type_array;
    int input_size;
    int output_size;
    int test_ms;
    std::vector<int> cores;

  };

  Options options(int argc, char ** argv);

}

#endif
