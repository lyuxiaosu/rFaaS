
#ifndef __TESTS__MULTI_FUNCTIONS_HPP__
#define __TESTS__MULTI_FUNCTIONS_HPP__

#include <string>

namespace multi_functions {

  struct Options {

    std::string json_config;
    std::string device_database;
    std::string executors_database;
    std::string output_stats;
    bool verbose;
    std::vector<std::string> fnames;
    std::vector<std::string> flibs;
    std::vector<int> req_parameters;
    int input_size;
    int output_size;
    int test_ms;

  };

  Options options(int argc, char ** argv);

}

#endif
