
#include <iostream>

#include <cxxopts.hpp>

#include "closeloop_log_normal.hpp"

namespace closeloop_log_normal {

  // Compilation time of client.cpp decreased from 11 to 1.5 seconds!!!

  Options options(int argc, char ** argv)
  {
    cxxopts::Options options("serverless-rdma-client", "Invoke functions");
    options.add_options()
      ("c,config", "JSON input config.",  cxxopts::value<std::string>())
      ("device-database", "JSON configuration of devices.", cxxopts::value<std::string>())
      ("executors-database", "JSON configuration of executor servers.", cxxopts::value<std::string>()->default_value(""))
      ("output-stats", "Output file for benchmarking statistics.", cxxopts::value<std::string>()->default_value(""))
      ("v,verbose", "Verbose output", cxxopts::value<bool>()->default_value("false"))
      ("names", "Function name", cxxopts::value<std::vector<std::string>>())
      ("functions", "Functions library", cxxopts::value<std::vector<std::string>>())
      ("req-parameters", "Request parameters", cxxopts::value<std::vector<int>>())
      ("req-type", "Request type array", cxxopts::value<std::vector<int>>())
      ("rps", "Request per second", cxxopts::value<std::vector<int>>())
      ("input-size", "Packet size", cxxopts::value<int>()->default_value("1"))
      ("output-size", "response buffer size", cxxopts::value<int>()->default_value("1"))
      ("test-ms", "Testing time lasting in ms", cxxopts::value<int>()->default_value("10000"))
      ("h,help", "Print usage", cxxopts::value<bool>()->default_value("false"))
    ;
    auto parsed_options = options.parse(argc, argv);
    if(parsed_options.count("help"))
    {
      std::cout << options.help() << std::endl;
      exit(0);
    }

    Options result;
    result.json_config = parsed_options["config"].as<std::string>();
    result.device_database = parsed_options["device-database"].as<std::string>();
    result.verbose = parsed_options["verbose"].as<bool>();;
    result.fnames = parsed_options["names"].as<std::vector<std::string>>();
    result.flibs = parsed_options["functions"].as<std::vector<std::string>>();
    result.req_parameters = parsed_options["req-parameters"].as<std::vector<int>>();
    result.req_type_array = parsed_options["req-type"].as<std::vector<int>>();
    result.input_size = parsed_options["input-size"].as<int>();
    result.output_size = parsed_options["output-size"].as<int>();
    result.output_stats = parsed_options["output-stats"].as<std::string>();
    result.executors_database = parsed_options["executors-database"].as<std::string>();
    result.test_ms = parsed_options["test-ms"].as<int>();
    result.rps_array = parsed_options["rps"].as<std::vector<int>>(); 

    return result;
  }

}
