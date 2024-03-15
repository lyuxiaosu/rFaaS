
#include <cxxopts.hpp>

#include "manager.hpp"

namespace rfaas::executor_manager {

  Options opts(int argc, char ** argv)
  {
    cxxopts::Options options("rfaas-executor-manager", "Handle client connections and allocation of executors.");
    options.add_options()
      ("c,config", "JSON input config.",  cxxopts::value<std::string>())
      ("device-database", "JSON configuration of devices.", cxxopts::value<std::string>())
      ("skip-resource-manager", "Ignore resource manager and don't connect to it.", cxxopts::value<bool>()->default_value("false"))
      ("max-funcs", "Support the max number of functions running in parallel if skipping resource manager.", cxxopts::value<int>()->default_value("1"))
      ("v,verbose", "Verbose output", cxxopts::value<bool>()->default_value("false"))
    ;
    auto parsed_options = options.parse(argc, argv);

    Options result;
    result.json_config = parsed_options["config"].as<std::string>();
    result.device_database = parsed_options["device-database"].as<std::string>();
    result.verbose = parsed_options["verbose"].as<bool>();
    result.skip_rm = parsed_options["skip-resource-manager"].as<bool>();
    result.max_funcs = parsed_options["max-funcs"].as<int>();    

    return result;
  }


}
