

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
#include "closeloop_trap_client.hpp"

#define unlikely(x) __builtin_expect(!!(x), 0)
#define kAppMaxWindowSize 256

#define Ki(x) (static_cast<size_t>(x) * 1000)
#define Mi(x) (static_cast<size_t>(x) * 1000 * 1000)
#define Gi(x) (static_cast<size_t>(x) * 1000 * 1000 * 1000)


FILE *perf_log = NULL;
volatile sig_atomic_t ctrl_c_pressed = 0;
void ctrl_c_handler(int) { ctrl_c_pressed = 1; }

std::map<int, std::vector<double>> throughput_map;
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
  size_t thread_id;
  size_t trap_type = 0;
  size_t total_cycles_when_send_trap; //when to send out the trap request
  int trap_has_sent = 0;
  uint64_t start_ts = 0;
  ChronoTimer start_time;
  std::vector<double> exp_nums;
  //Latency latency
  std::vector<double> latency_array;
  std::vector<rdmalib::Buffer<char>> ins;
  std::vector<rdmalib::Buffer<char>> outs;
  ~ClientContext() {}
};

void return_callback(int return_val, void *context, void *index) {
  auto *c = static_cast<ClientContext *>(context);
  const double req_lat_us = c->start_time.get_us();
  const auto w_i = reinterpret_cast<size_t>(index);
  c->latency_array.push_back(req_lat_us);
  c->num_resps++;
  if (c->trap_has_sent == 0) {
    uint64_t diff_ts = rdtsc() - c->start_ts;
    if (diff_ts >= c->total_cycles_when_send_trap) {
      ((char*)c->ins.back().data())[0] = c->trap_type;
      c->start_time.reset();
      c->exec->async_callback(c->fname, c->ins[0], c->outs[0], reinterpret_cast<void *>(0), return_callback);
      c->trap_has_sent = 1;
      printf("thread id %zu send trap %zu request\n", c->thread_id, c->trap_type);
    } else {
      ((char*)c->ins.back().data())[0] = 4;
      c->start_time.reset();
      c->exec->async_callback(c->fname, c->ins[0], c->outs[0], reinterpret_cast<void *>(0), return_callback);
    }
  } else {
    ((char*)c->ins.back().data())[0] = 4;
    c->start_time.reset();
    c->exec->async_callback(c->fname, c->ins[0], c->outs[0], reinterpret_cast<void *>(0), return_callback);
  }

}

void client_func(size_t thread_id, size_t trap_type, rfaas::benchmark::Settings &settings, closeloop_trap_client::Options &opts) {

  double freq_ghz = measure_rdtsc_freq();
  printf("thread %zu cpu freq %f\n", thread_id, freq_ghz);


  ClientContext c;

  rfaas::executor executor("10.10.1.1", 10000, settings.benchmark.numcores, settings.benchmark.memory, thread_id + 1, *settings.device);
  executor.set_context(static_cast<void *> (&c));
  
  c.exec = &executor;
  c.fname = opts.fnames[thread_id];
  c.output_size = opts.output_size;
  c.thread_id = thread_id;
  c.trap_type = trap_type;
  if (trap_type == 1) {
    c.total_cycles_when_send_trap = ms_to_cycles(5000, freq_ghz);
  } else {
    c.total_cycles_when_send_trap = ms_to_cycles(10000, freq_ghz);
  }
  printf("trap type is %zu total_cycles_when_send_trap %zu\n", c.trap_type, c.total_cycles_when_send_trap );

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
      ((char*)c.ins.back().data())[i] = 4;
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
  size_t print_interval_cycles = ms_to_cycles(1000, freq_ghz);
  uint32_t total_send_out = 0;

  //send the request.
  c.start_ts = rdtsc();
  c.start_time.reset();
  executor.async_callback(opts.fnames[thread_id], c.ins[0], c.outs[0], reinterpret_cast<void *>(0), return_callback);

  struct timespec startT, endT;
  clock_gettime(CLOCK_MONOTONIC, &startT);
  uint64_t begin, end, begin2, end2;
  begin = rdtsc();
  begin2 = begin;
  end2 = begin;
  end = begin;

  while ((end - begin) < total_cycles && ctrl_c_pressed != 1) {
    executor.poll_queue_once2(0);
    end2 = rdtsc();
    if ((end2 - begin2) >= print_interval_cycles) {
      /*printf("%zu: %.1f %.1f %.1f %.1f %.2f\n", thread_id,
           c.latency.perc(.5) / kAppLatFac, c.latency.perc(.05) / kAppLatFac,
           c.latency.perc(.99) / kAppLatFac, c.latency.perc(.999) / kAppLatFac,
           c.num_resps / (seconds * Mi(1)));
      */
      printf("%zu: %.2f\n", thread_id, c.num_resps / (1.0 * Mi(1)));

      throughput_map[thread_id].push_back(c.num_resps / (1.0 * Mi(1)));
      c.num_resps = 0;
      begin2 = rdtsc();
    }
    end = rdtsc();
  }

  clock_gettime(CLOCK_MONOTONIC, &endT);
  printf("thread %zu finished test\n", thread_id);
 
  int64_t delta_ms = (endT.tv_sec - startT.tv_sec) * 1000 + (endT.tv_nsec - startT.tv_nsec) / 1000000;
  int64_t delta_s = delta_ms / 1000;
  //int rps = c.num_resps / delta_s;
  int rps = total_send_out / delta_s;
  rps_array[thread_id] = rps;
 
  printf("Thread %zu finished execution %zu \n", thread_id, c.num_resps); 
  if (seperate_rps.count(opts.req_type_array[thread_id]) > 0) {
    seperate_rps[opts.req_type_array[thread_id]] += rps;
  } else {
    seperate_rps[opts.req_type_array[thread_id]] = rps;
  }

  for (size_t i = 0; i < total_send_out; i++) {
    //fprintf(perf_log, "%zu %d %f %d\n", thread_id, opts.req_type_array[thread_id], c.latency_array[i], 0);
    fprintf(perf_log, "%zu %d %f\n", thread_id, opts.req_type_array[thread_id], c.latency_array[i]);
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

  auto opts = closeloop_trap_client::options(argc, argv);
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
    threads[i] = std::thread(client_func, i, i+1, std::ref(settings), std::ref(opts));
    bind_to_core(threads[i], i); 
    printf("Pin thread %zu to cpu core %zu\n", i, i+1); 
  }

  for (size_t i = 0; i < num_threads; i++) threads[i].join();
  
  for (auto it = seperate_rps.begin(); it != seperate_rps.end(); ++it) {
    printf("type %d sending rate %d\n", it->first, it->second);
    fprintf(perf_log, "type %d sending rate %d service rate %d\n", it->first, it->second, 0);
  }

  size_t min_vector_len = throughput_map[0].size();
  for (size_t i = 1; i < num_threads; i++) {
        //find the minimum vector length of all throughput arrays
        if (min_vector_len > throughput_map[i].size()) {
            min_vector_len = throughput_map[i].size();
        }
  }

  std::vector<double> total_throughput;
  for (size_t i = 0; i < min_vector_len; i++) {
        double total_tput = 0;
        for (size_t j = 0; j < num_threads; j++) {
            total_tput += throughput_map[j][i];
        }
        total_throughput.push_back(total_tput);
        fprintf(perf_log, "%.2f ", total_tput);
        printf("%.2f ", total_tput);
  }
  printf("\n");

  printf("client exit\n");
  return 0;
}
