

#ifndef __RFAAS_EXECUTOR_MANAGER_SETTINGS_HPP__
#define __RFAAS_EXECUTOR_MANAGER_SETTINGS_HPP__

#include <string>

#include <rfaas/devices.hpp>

#include <cereal/details/helpers.hpp>

namespace rfaas::executor_manager {

  struct ExecutorSettings
  {
    bool use_docker;
    int repetitions;
    int warmup_iters;
    int recv_buffer_size;
    int max_inline_data;
    bool pin_threads;

    template <class Archive>
    void load(Archive & ar )
    {
      ar(
        CEREAL_NVP(use_docker), CEREAL_NVP(repetitions),
        CEREAL_NVP(warmup_iters), CEREAL_NVP(pin_threads)
      );
    }
  };

  // Manager configuration settings.
  // Includes the RDMA connection, and the HTTP connection.
  struct Settings
  {
    std::string rdma_device;
    int rdma_device_port;
    rfaas::device_data* device;

    // Passed to the scheduled executor
    ExecutorSettings exec;

    template <class Archive>
    void load(Archive & ar )
    {
      ar(
        CEREAL_NVP(rdma_device), CEREAL_NVP(rdma_device_port)
      );
    }

    static Settings deserialize(std::istream & in);
  };

}

#endif
