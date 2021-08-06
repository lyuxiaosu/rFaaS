
#ifndef __SERVER_EXECUTOR_MANAGER_MANAGER_HPP__
#define __SERVER_EXECUTOR_MANAGER_MANAGER_HPP__

#include <atomic>
#include <chrono>
#include <cstdint>
#include <vector>
#include <mutex>
#include <map>

#include <rdmalib/connection.hpp>
#include <rdmalib/rdmalib.hpp>
#include <rdmalib/server.hpp>
#include <rdmalib/buffer.hpp>
#include <rdmalib/recv_buffer.hpp>

#include "client.hpp"
#include "settings.hpp"
#include "../common.hpp"
#include "../common/readerwriterqueue.h"

namespace rdmalib {
  struct AllocationRequest;
}

namespace rfaas::executor_manager {

  struct Options {
    std::string json_config;
    std::string device_database;
    bool verbose;
  };
  Options opts(int, char**);

  struct Manager
  {
    // FIXME: we need a proper data structure that is thread-safe and scales
    //static constexpr int MAX_CLIENTS_ACTIVE = 128;
    static constexpr int MAX_EXECUTORS_ACTIVE = 8;
    static constexpr int MAX_CLIENTS_ACTIVE = 1024;
    moodycamel::ReaderWriterQueue<std::pair<int, std::unique_ptr<rdmalib::Connection>>> _q1;
    moodycamel::ReaderWriterQueue<std::pair<int,Client>> _q2;

    std::mutex clients;
    std::map<int, Client> _clients;
    int _ids;

    //std::vector<Client> _clients;
    //std::atomic<int> _clients_active;
    rdmalib::RDMAActive _active_connections;
    //std::unique_ptr<rdmalib::Connection> _res_mgr_connection;

    rdmalib::RDMAPassive _state;
    //rdmalib::server::ServerStatus _status;
    Settings _settings;
    //rdmalib::Buffer<Accounting> _accounting_data;
    int _secret;

    Manager(Settings &);

    void start();
    void listen();
    void poll_rdma();
  };

}

#endif

