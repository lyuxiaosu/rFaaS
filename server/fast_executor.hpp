
#ifndef __SERVER_FASTEXECUTORS_HPP__
#define __SERVER_FASTEXECUTORS_HPP__

#include "rdmalib/rdmalib.hpp"
#include <vector>
#include <thread>
#include <atomic>
#include <condition_variable>

#include <rdmalib/buffer.hpp>
#include <rdmalib/connection.hpp>
#include <rdmalib/recv_buffer.hpp>
#include <rdmalib/functions.hpp>

//#include "structures.hpp"

namespace rdmalib {
  struct RecvBuffer;
}

namespace server {

  struct Thread {
    std::string addr;
    int port;
    int max_inline_data;
    int id, repetitions;
    int max_repetitions;
    uint64_t sum;
    rdmalib::Buffer<char> send, rcv;
    rdmalib::RecvBuffer wc_buffer;
    rdmalib::Connection* conn;

    Thread(std::string addr, int port, int id, int buf_size, int recv_buffer_size, int max_inline_data):
      //active(addr, port, recv_buffer_size),
      addr(addr),
      port(port),
      max_inline_data(max_inline_data),
      id(id),
      repetitions(0),
      max_repetitions(0),
      sum(0),
      send(buf_size),
      rcv(buf_size, rdmalib::functions::Submission::DATA_HEADER_SIZE),
      wc_buffer(recv_buffer_size),
      conn(nullptr)
    {
    }

    void work(int func_id);
    void hot();
    void warm();
    void thread_work(int timeout);
  };

  struct FastExecutors {

    std::vector<Thread> _threads_data;
    std::vector<std::thread> _threads;
    bool _closing;
    int _numcores;
    int _max_repetitions;
    int _warmup_iters;
    int _pin_threads;

    FastExecutors(
      std::string client_addr, int port,
      int numcores,
      int msg_size,
      int recv_buf_size,
      int max_inline_data,
      int pin_threads
    );
    ~FastExecutors();

    void close();
    void allocate_threads(int, int);
  };

}

#endif

