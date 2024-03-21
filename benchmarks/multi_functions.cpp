
#include <assert.h>
#include <chrono>
#include <fstream>
#include <thread>

#include <cxxopts.hpp>
#include <spdlog/spdlog.h>

#include <rdmalib/benchmarker.hpp>
#include <rdmalib/functions.hpp>
#include <rdmalib/rdmalib.hpp>

#include <rfaas/executor.hpp>
#include <rfaas/resources.hpp>
#include <rfaas/rfaas.hpp>

#include "settings.hpp"
#include "multi_functions.hpp"

void client_func(size_t thread_id, rfaas::benchmark::Settings &settings, multi_functions::Options &opts) {

  rfaas::executor executor("10.10.1.1", 10000, settings.benchmark.numcores, settings.benchmark.memory, thread_id + 1, *settings.device);
 
  //the last parameter is skip_exec_manager, not skip_resource_manager
  //This function will accept executor connection and then send function code data to executor 
  if (!executor.allocate(opts.flibs[thread_id], opts.input_size, opts.output_size,
                         settings.benchmark.hot_timeout, false)) {
    spdlog::error("Connection to executor and allocation failed!");
    return;
  }

  // FIXME: move me to a memory allocator
  rdmalib::Buffer<char> in(opts.input_size,
                           rdmalib::functions::Submission::DATA_HEADER_SIZE),
      out(opts.output_size);
  in.register_memory(executor._state.pd(), IBV_ACCESS_LOCAL_WRITE);
  out.register_memory(executor._state.pd(),
                      IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
  memset(in.data(), 0, opts.input_size);
  for (int i = 0; i < opts.input_size; ++i) {
    ((char *)in.data())[i] = opts.req_parameters[thread_id];
  }

  rdmalib::Benchmarker<1> benchmarker{settings.benchmark.repetitions};
  spdlog::info("Warmups begin");
  for (int i = 0; i < settings.benchmark.warmup_repetitions; ++i) {
    SPDLOG_DEBUG("Submit warm {}", i);
    executor.execute(opts.fnames[thread_id], in, out);
  }
  spdlog::info("Warmups completed");

  // Start actual measurements
  printf("benchmark repetitions %d\n", settings.benchmark.repetitions);
  for (int i = 0; i < settings.benchmark.repetitions - 1;) {
    benchmarker.start();
    SPDLOG_DEBUG("Submit execution {}", i);
    auto ret = executor.execute(opts.fnames[thread_id], in, out);
    if (std::get<0>(ret)) {
      SPDLOG_DEBUG("Finished execution {} out of {}", i,
                   settings.benchmark.repetitions);
      benchmarker.end(0);
      ++i;
    } else {
      return;
    }
  }
  auto [median, avg] = benchmarker.summary();
  spdlog::info("thread {} Executed {} repetitions, avg {} usec/iter, median {}",
               thread_id, settings.benchmark.repetitions, avg, median);
  if (opts.output_stats != "")
    benchmarker.export_csv(opts.output_stats, {"time"});
  executor.deallocate();

  printf("Thread %zu Data: ", thread_id);
  for (int i = 0; i < std::min(100, opts.output_size); ++i)
    printf("%d ", ((char *)out.data())[i]);
  printf("\n");
}

void bind_to_core(std::thread &thread, size_t core_index) {
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);

  CPU_SET(core_index, &cpuset);
  int rc = pthread_setaffinity_np(thread.native_handle(), sizeof(cpu_set_t),
                                  &cpuset);

  if (rc != 0) {
    printf("Error setting thread affinity");
    exit(-1);
  } 
} 
int main(int argc, char **argv) {
  auto opts = multi_functions::options(argc, argv);
  if (opts.verbose)
    spdlog::set_level(spdlog::level::debug);
  else
    spdlog::set_level(spdlog::level::info);
  spdlog::set_pattern("[%H:%M:%S:%f] [T %t] [%l] %v ");
  spdlog::info("Executing serverless-rdma test warm_benchmarker!");

  // Read device details
  std::ifstream in_dev{opts.device_database};
  rfaas::devices::deserialize(in_dev);
  in_dev.close();
  
  // Read benchmark settings
  std::ifstream benchmark_cfg{opts.json_config};
  rfaas::benchmark::Settings settings =
      rfaas::benchmark::Settings::deserialize(benchmark_cfg);
  benchmark_cfg.close();

  size_t num_threads = opts.fnames.size();
  assert(num_threads == opts.flibs.size());

  std::vector<std::thread> threads(num_threads);
  
  for (size_t i = 0; i < num_threads; i++) {
    threads[i] = std::thread(client_func, i, std::ref(settings), std::ref(opts));
    bind_to_core(threads[i], i + 1); 
  }

  for (size_t i = 0; i < num_threads; i++) threads[i].join();
  
  /*rfaas::client instance(
    settings.resource_manager_address, settings.resource_manager_port,
    *settings.device
  );
  if (!instance.connect()) {
    spdlog::error("Connection to resource manager failed!");
    return 1;
  }

  auto leased_executor = instance.lease(settings.benchmark.numcores, settings.benchmark.memory, *settings.device);
  if (!leased_executor.has_value()) {
    spdlog::error("Couldn't acquire a lease!");
    return 1;
  }*/

  //rfaas::executor executor = std::move(leased_executor.value());
  // here is the exec_manager's address and port

  //instance.disconnect();
  printf("client exit\n");
  return 0;
}
