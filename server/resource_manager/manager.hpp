
#ifndef __RFAAS_RESOURCE_MANAGER__
#define __RFAAS_RESOURCE_MANAGER__

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

#include <rfaas/devices.hpp>
#include "../common/readerwriterqueue.h"

namespace rdmalib {
  struct AllocationRequest;
}

namespace rfaas::resource_manager {

  // Manager configuration settings.
  // Includes the RDMA connection, and the HTTP connection.
  struct Settings
  {
    std::string rdma_device;
    int rdma_device_port;
    rfaas::device_data* device;
    
    std::string http_network_address;
    int http_network_port;

    template <class Archive>
    void load(Archive & ar )
    {
      ar(
        CEREAL_NVP(rdma_device), CEREAL_NVP(rdma_device_port),
        CEREAL_NVP(http_network_address), CEREAL_NVP(http_network_port)
      );
    }

    static Settings deserialize(std::istream & in);
  };

  struct Options {
    std::string json_config;
    std::string initial_database;
    std::string output_database;
    std::string device_database;
    bool verbose;
  };
  Options opts(int, char**);

  struct Manager
  {
    //moodycamel::ReaderWriterQueue<std::pair<int, std::unique_ptr<rdmalib::Connection>>> _q1;
    //moodycamel::ReaderWriterQueue<std::pair<int,Client>> _q2;
    //std::mutex clients;
    //std::map<int, Client> _clients;
    //int _ids;

    // FIXME: database

    // Handling RDMA connections with clients and executor managers
    rdmalib::RDMAPassive _state;
    // Handling HTTP events
    // FIXME

    //rdmalib::server::ServerStatus _status;
    Settings _settings;
    //rdmalib::Buffer<Accounting> _accounting_data;
    std::string _address;
    int _port;
    int _secret;

    Manager(Settings &);

    void start();
    //void listen();
    //void poll_rdma();
  };

}

#endif

