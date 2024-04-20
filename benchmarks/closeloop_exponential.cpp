

#include <random>
#include <signal.h>
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
#include "closeloop_exponential.hpp"

#define unlikely(x) __builtin_expect(!!(x), 0)
#define kAppMaxWindowSize 256

FILE *perf_log = NULL;
volatile sig_atomic_t ctrl_c_pressed = 0;
void ctrl_c_handler(int) { ctrl_c_pressed = 1; }

int rps_array[100];
std::unordered_map<int, int> seperate_rps;

/// Simple time that uses std::chrono
class ChronoTimer {
 public:
  ChronoTimer() { reset(); }
  void reset() { start_time_ = std::chrono::high_resolution_clock::now(); }

  /// Return seconds elapsed since this timer was created or last reset
  double get_sec() const { return get_ns() / 1e9; }

  /// Return milliseconds elapsed since this timer was created or last reset
  double get_ms() const { return get_ns() / 1e6; }

  /// Return microseconds elapsed since this timer was created or last reset
  double get_us() const { return get_ns() / 1e3; }

  /// Return nanoseconds elapsed since this timer was created or last reset
  size_t get_ns() const {
    return static_cast<size_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now() - start_time_)
            .count());
  }

 private:
  std::chrono::time_point<std::chrono::high_resolution_clock> start_time_;
};

/// Return the TSC
static inline size_t rdtsc() {
  uint64_t rax;
  uint64_t rdx;
  asm volatile("rdtsc" : "=a"(rax), "=d"(rdx));
  return static_cast<size_t>((rdx << 32) | rax);
}

static double measure_rdtsc_freq() {
  ChronoTimer chrono_timer;
  const uint64_t rdtsc_start = rdtsc();

  // Do not change this loop! The hardcoded value below depends on this loop
  // and prevents it from being optimized out.
  uint64_t sum = 5;
  for (uint64_t i = 0; i < 1000000; i++) {
    sum += i + (sum + i) * (i % sum);
  }
  if (unlikely(sum != 13580802877818827968ull)) {
    printf("Error in RDTSC freq measurement");
    exit(-1);
  }

  const uint64_t rdtsc_cycles = rdtsc() - rdtsc_start;
  const double freq_ghz = rdtsc_cycles * 1.0 / chrono_timer.get_ns();
  if (unlikely(!(freq_ghz >= 0.5 && freq_ghz <= 5.0))) {
    printf("Invalid RDTSC frequency");
    exit(-1);
  }

  return freq_ghz;
}

static size_t ms_to_cycles(double ms, double freq_ghz) {
  return static_cast<size_t>(ms * 1000 * 1000 * freq_ghz);
}

double ran_expo2(std::mt19937& generator, double lambda) {
    std::exponential_distribution<double> distribution(lambda);
    return distribution(generator);
}

class ClientContext {
 public:
  rfaas::executor *exec;
  std::string fname;
  int output_size;
  size_t num_resps = 0;
  uint64_t max_requests = 0;
  size_t thread_id;
  int rps;
  ChronoTimer start_time;
  std::vector<double> exp_nums;
  //Latency latency
  std::vector<double> latency_array;
  std::vector<double> delayed_latency_array;
  std::vector<double> total_delayed_latency_array;
  std::vector<rdmalib::Buffer<char>> ins;
  std::vector<rdmalib::Buffer<char>> outs;
  std::chrono::time_point<std::chrono::high_resolution_clock> next_should_send_ts;
  std::mt19937 generator;
  ~ClientContext() {}
};

void return_callback(int return_val, void *context, void *index) {
  auto current = std::chrono::high_resolution_clock::now();
  auto *c = static_cast<ClientContext *>(context);
  const double req_lat_us = c->start_time.get_us();
  
  const auto w_i = reinterpret_cast<size_t>(index);
  c->latency_array.push_back(req_lat_us);
  c->num_resps++;

  const double total_delayed_latency_us = static_cast<size_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(current - c->next_should_send_ts).count()) / 1e3;
  c->total_delayed_latency_array.push_back(total_delayed_latency_us);

  double ms = ran_expo2(c->generator, c->rps) * 1000;
  std::chrono::duration<double, std::milli> interval(ms);
  c->next_should_send_ts = std::chrono::time_point_cast<std::chrono::high_resolution_clock::duration>(c->next_should_send_ts + interval);
  while (current < c->next_should_send_ts) {
    current = std::chrono::high_resolution_clock::now();
  }

  if (c->num_resps < c->max_requests) {
    c->start_time.reset();
    c->exec->async_callback(c->fname, c->ins[w_i], c->outs[w_i], reinterpret_cast<void *>(w_i), return_callback);
  }
}

void client_func(size_t thread_id, rfaas::benchmark::Settings &settings, closeloop_exponential::Options &opts) {

  double freq_ghz = measure_rdtsc_freq();
  printf("thread %zu cpu freq %f\n", thread_id, freq_ghz);


  ClientContext c;
  rfaas::executor executor("10.10.1.1", 10000, settings.benchmark.numcores, settings.benchmark.memory, thread_id + 1, *settings.device);
  executor.set_context(static_cast<void *> (&c));
  
  c.exec = &executor;
  c.fname = opts.fnames[thread_id];
  c.output_size = opts.output_size;
  c.thread_id = thread_id;
  /* set seed for this thread */
  c.generator = std::mt19937(thread_id);
  c.max_requests = (opts.test_ms/1000) * opts.rps_array[thread_id];
  c.rps = opts.rps_array[thread_id];

  //the last parameter is skip_exec_manager, not skip_resource_manager
  //This function will accept executor connection and then send function code data to executor 
  if (!executor.allocate(opts.flibs[thread_id], opts.input_size, opts.output_size,
                         settings.benchmark.hot_timeout, settings.benchmark.numcores, 0, false)) {
    spdlog::error("Connection to executor and allocation failed!");
    return;
  }

  // FIXME: move me to a memory allocator
  for(int i = 0; i < settings.benchmark.numcores; ++i) {
    c.ins.emplace_back(opts.input_size, rdmalib::functions::Submission::DATA_HEADER_SIZE);
    c.ins.back().register_memory(executor._state.pd(), IBV_ACCESS_LOCAL_WRITE);
    memset(c.ins.back().data(), 0, opts.input_size);
    for(int i = 0; i < opts.input_size; ++i) {
      ((char*)c.ins.back().data())[i] = opts.req_parameters[thread_id];
    }
    
    c.outs.emplace_back(opts.output_size);
    c.outs.back().register_memory(executor._state.pd(), IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
  }

  rdmalib::Benchmarker<1> benchmarker{settings.benchmark.repetitions};
  spdlog::info("Warmups begin");
  for(int i = 0; i < settings.benchmark.warmup_repetitions; ++i) {
    SPDLOG_DEBUG("Submit warm {}", i);
    executor.execute(opts.fnames[thread_id], c.ins, c.outs);
  }
  spdlog::info("Warmups completed");

  
  // Start actual measurements
  size_t total_cycles = ms_to_cycles(opts.test_ms, freq_ghz);
  struct timespec startT, endT;
  clock_gettime(CLOCK_MONOTONIC, &startT);
  uint64_t begin, end;
  begin = rdtsc();
  end = begin;

  c.next_should_send_ts = std::chrono::high_resolution_clock::now();
  c.start_time.reset();
  executor.async_callback(opts.fnames[thread_id], c.ins[0], c.outs[0], reinterpret_cast<void *>(0), return_callback);

  while (c.num_resps < c.max_requests && ctrl_c_pressed != 1) {
      executor.poll_queue_once2(0);
  }

  clock_gettime(CLOCK_MONOTONIC, &endT);
  printf("thread %d finished test\n", thread_id);
 
  int64_t delta_ms = (endT.tv_sec - startT.tv_sec) * 1000 + (endT.tv_nsec - startT.tv_nsec) / 1000000;
  int64_t delta_s = delta_ms / 1000;
  int rps = c.num_resps / delta_s;
  rps_array[thread_id] = rps;
 
  printf("Thread %d finished execution %u expected rps %d actual rps %d\n", thread_id, c.num_resps, opts.rps_array[thread_id], rps); 
  if (seperate_rps.count(opts.req_type_array[thread_id]) > 0) {
    seperate_rps[opts.req_type_array[thread_id]] += rps;
  } else {
    seperate_rps[opts.req_type_array[thread_id]] = rps;
  }

  for (size_t i = 0; i < c.num_resps; i++) {
    //fprintf(perf_log, "%zu %d %f %d\n", thread_id, opts.req_type_array[thread_id], c.latency_array[i], 0);
    fprintf(perf_log, "%zu %d %f %f\n", thread_id, opts.req_type_array[thread_id], c.latency_array[i], c.total_delayed_latency_array[i]);
  }

  executor.deallocate();
  /*printf("thread %zu exp nums:\n", thread_id);
  for (size_t i = 0; i < 100; i++) {
    printf("%f\n", c.exp_nums[i]);
  }*/

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

void perf_log_init(std::string& fname)
{
  if (fname != "") {
    printf("Client Performance Log %s\n", fname.c_str());
    perf_log = fopen(fname.c_str(), "w");
    if (perf_log == NULL) perror("perf_log_init\n");
    fprintf(perf_log, "thread id, type id, true latency, delayed latency, total delayed latency\n");
  }
}
 
int main(int argc, char **argv) {
  signal(SIGINT, ctrl_c_handler);

  auto opts = closeloop_exponential::options(argc, argv);
  if (opts.verbose)
    spdlog::set_level(spdlog::level::debug);
  else
    spdlog::set_level(spdlog::level::info);
  spdlog::set_pattern("[%H:%M:%S:%f] [T %t] [%l] %v ");
  spdlog::info("Executing serverless-rdma test warm_benchmarker!");

  perf_log_init(opts.output_stats);
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
    printf("Pin thread %d to cpu core %d\n", i, i+1); 
  }

  for (size_t i = 0; i < num_threads; i++) threads[i].join();
  
  for (auto it = seperate_rps.begin(); it != seperate_rps.end(); ++it) {
    printf("type %d sending rate %d\n", it->first, it->second);
    fprintf(perf_log, "type %d sending rate %d service rate %d\n", it->first, it->second, 0);
  }

  printf("client exit\n");
  return 0;
}
