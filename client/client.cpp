
#include <chrono>
#include <thread>

#include <spdlog/spdlog.h>

#include <rdmalib/rdmalib.hpp>
#include <rdmalib/recv_buffer.hpp>
#include "client.hpp"
#include "benchmarker.hpp"


int main(int argc, char ** argv)
{
  auto opts = client::options(argc, argv);
  int buf_size = opts["size"].as<int>();
  int recv_buf_size = opts["requests"].as<int>();
  int repetitions = opts["repetitions"].as<int>();
  int warmup_iters = opts["warmup-iters"].as<int>();
  if(opts["verbose"].as<bool>())
    spdlog::set_level(spdlog::level::debug);
  else
    spdlog::set_level(spdlog::level::info);
  spdlog::set_pattern("[%H:%M:%S:%f] [T %t] [%l] %v ");
  spdlog::info("Executing serverless-rdma client!");

  std::ifstream in(opts["file"].as<std::string>());
  client::ServerConnection client(rdmalib::server::ServerStatus::deserialize(in), recv_buf_size);
  in.close();
  client.allocate_send_buffers(2, buf_size);
  client.allocate_receive_buffers(2, buf_size);
  if(!client.connect())
    return -1;
  spdlog::info("Connected to the server!");

  // prepare args
  memset(client.send_buffer(0).data(), 0, buf_size);
  memset(client.send_buffer(1).data(), 0, buf_size);
  for(int i = 0; i < buf_size; ++i) {
    ((int*)client.send_buffer(0).data())[i] = 1;
    ((int*)client.send_buffer(1).data())[i] = 2;
  }

  // Warmup iterations
  client::Benchmarker benchmarker{repetitions};
  rdmalib::RecvBuffer rcv_buffer{recv_buf_size};
  rcv_buffer.connect(&client.connection());
  spdlog::info("Warmups begin");
  for(int i = 0; i < warmup_iters; ++i) {
    rcv_buffer.refill();
    client.submit_fast(1, "test");
    auto wc = rcv_buffer.poll(true);
  }
  spdlog::info("Warmups completed");

  // Start actual measurements
  for(int i = 0; i < repetitions; ++i) {
    benchmarker.start();
    rcv_buffer.refill();
    int id = client.submit_fast(1, "test");
    auto wc = rcv_buffer.poll(true);
    benchmarker.end();
    SPDLOG_DEBUG("Finished execution with ID {}", ntohl(wc->imm_data)); 
  }
  auto [median, avg] = benchmarker.summary();
  spdlog::info("Executed {} repetitions, avg {} usec/iter, median {}", repetitions, avg, median);
  benchmarker.export_csv(opts["out-file"].as<std::string>());

  for(int i = 0; i < 100; ++i)
    printf("%d ", ((int*)client.recv_buffer(0).data())[i]);
  printf("\n");
  for(int i = 0; i < 100; ++i)
    printf("%d ", ((int*)client.recv_buffer(1).data())[i]);
  printf("\n");

  return 0;
}
