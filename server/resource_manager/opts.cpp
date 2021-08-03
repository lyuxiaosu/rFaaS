
#include <cxxopts.hpp>

#include "server.hpp"
#include "manager.hpp"

namespace rfaas::resource_manager {

  Options opts(int argc, char ** argv)
  {
    cxxopts::Options options("rFaaS resource manager",
      "Handle resource availability and announce it."
    );
    options.add_options()
      ("c,config", "JSON input config.",
       cxxopts::value<std::string>())
      ("i,input-database", "JSON with initial data of clients.",
       cxxopts::value<std::string>()->default_value(""))
      ("o,output-database", "Write and update JSON with data of clients.",
       cxxopts::value<std::string>()->default_value(""))
      ("devices", "JSON configuration of devices.", cxxopts::value<std::string>())
      ("device", "Selected device.", cxxopts::value<std::string>())
      ("v,verbose", "Verbose output", cxxopts::value<bool>()->default_value("false"))
    ;
    auto parsed_options = options.parse(argc, argv);

    Options result;
    result.json_config = parsed_options["config"].as<std::string>();
    result.initial_database = parsed_options["input-database"].as<std::string>();
    result.output_database = parsed_options["output-database"].as<std::string>();
    result.device_database = parsed_options["device_database"].as<std::string>();
    result.device = parsed_options["device"].as<std::string>();
    result.verbose = parsed_options["verbose"].as<bool>();
    return result;
  }
}

