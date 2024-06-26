
#ifndef __RFAAS_EXECUTOR_HPP__
#define __RFAAS_EXECUTOR_HPP__

#include <algorithm>
#include <iterator>
#include <future>
#include <fcntl.h>

#include <rdmalib/benchmarker.hpp>
#include <rdmalib/connection.hpp>
#include <rdmalib/buffer.hpp>
#include <rdmalib/rdmalib.hpp>

#include <rfaas/connection.hpp>
#include <rfaas/devices.hpp>

#include <spdlog/spdlog.h>

namespace rfaas {

  typedef void (*rfaas_result_callback_t)(int return_val, void * out);
  
  typedef void (*rfaas_result_callback2_t) (int return_val, void *context, void *tag);

  struct servers;

  namespace impl {

    template <int I, class... Ts>
    decltype(auto) get(Ts&&... ts) {
        return std::get<I>(std::forward_as_tuple(ts...));
    }

  }

  struct polling_type {
    static const polling_type HOT_ALWAYS;
    static const polling_type WARM_ALWAYS;

    int _timeout;

    polling_type(int timeout);
    operator int() const;
  };

  struct executor_state {
    std::unique_ptr<rdmalib::Connection> conn;
    rdmalib::RemoteBuffer remote_input;
    //rdmalib::RecvBuffer _rcv_buffer;
    executor_state(rdmalib::Connection*, int rcv_buf_size);
  };

  struct executor {
    static constexpr int MAX_REMOTE_WORKERS = 256;
    rdmalib::RDMAPassive _state;
    rdmalib::Buffer<rdmalib::BufferInformation> _execs_buf;

    device_data _device;

    int _numcores;
    int _memory;
    int _executions;
    int _invoc_id;
    int _lease_id;
    // FIXME: global settings
    std::vector<executor_state> _connections;
    std::unique_ptr<manager_connection> _exec_manager;
    std::vector<std::string> _func_names;

    // manage async executions
    std::atomic<bool> _end_requested;
    std::atomic<bool> _active_polling;
    std::unordered_map<int, std::tuple<int, std::promise<int>>> _futures;
    std::unordered_map<int, std::tuple<int, rfaas_result_callback_t, rdmalib::Buffer<char>*>> _callbacks;
    std::unordered_map<int, std::tuple<int, rfaas_result_callback2_t, rdmalib::Buffer<char>*, void*>> _callbacks2;
    std::unique_ptr<std::thread> _background_thread;
    int events;
    void *_context;

    // Currently, we use the same device for listening and connecting to the manager.
    executor(const std::string& address, int port, int numcores, int memory, int lease_id, device_data & dev);
    ~executor();

    executor(executor&& obj);

    inline void set_context(void *context) {
      assert(_context == NULL);
      _context = context;
    }

    bool connect(const std::string & ip, int port);

    // Skipping managers is useful for benchmarking
    bool allocate(std::string functions_path, int max_input_size, int max_output_size, int hot_timeout,
        int cores, int core_start_index, bool skip_manager = false, rdmalib::Benchmarker<5> * benchmarker = nullptr);
    void deallocate();
    rdmalib::Buffer<char> load_library(std::string path);
    void poll_queue();
    // cid is connection id
    void poll_queue_once(int cid);
    void poll_queue_once2(int cid);
    void poll_queue_callback();

    // Has some bug because it only handle connections[0], couldn't be used
    // when numcores > 1
    template<typename T, typename U>
    std::future<int> async(std::string fname, const rdmalib::Buffer<T> & in, rdmalib::Buffer<U> & out, int64_t size = -1)
    {
      auto it = std::find(_func_names.begin(), _func_names.end(), fname);
      if(it == _func_names.end()) {
        spdlog::error("Function {} not found in the deployed library!", fname);
        return std::future<int>{};
      }
      int func_idx = std::distance(_func_names.begin(), it);

      // FIXME: here get a future for async
      char* data = static_cast<char*>(in.ptr());
      // TODO: we assume here uintptr_t is 8 bytes
      *reinterpret_cast<uint64_t*>(data) = out.address();
      *reinterpret_cast<uint32_t*>(data + 8) = out.rkey();

      int invoc_id = this->_invoc_id++;
      //_futures[invoc_id] = std::move(std::promise<int>{});
      _futures[invoc_id] = std::make_tuple(1, std::promise<int>{});
      uint32_t submission_id = (invoc_id << 16) | (1 << 15) | func_idx;
      SPDLOG_DEBUG(
        "Invoke function {} with invocation id {}, submission id {}",
        func_idx, invoc_id, submission_id
      );
      if(size != -1) {
        rdmalib::ScatterGatherElement sge;
        sge.add(in, size, 0);
        _connections[0].conn->post_write(
          std::move(sge),
          _connections[0].remote_input,
          submission_id,
          size <= _device.max_inline_data,
          true
        );
      } else {
        _connections[0].conn->post_write(
          in,
          _connections[0].remote_input,
          submission_id,
          in.bytes() <= _device.max_inline_data,
          true
        );
      }
      //_connections[0]._rcv_buffer.refill();
      _connections[0].conn->receive_wcs().refill();
      return std::get<1>(_futures[invoc_id]).get_future();
    }

    // Has some bug because it only handle connections[0], couldn't be used 
    // when numcores > 1
    template<typename T>
    void async_callback(std::string fname, const rdmalib::Buffer<T> & in, 
               rdmalib::Buffer<char> & out, rfaas_result_callback_t result_callback, 
               int64_t size = -1)
    {
      auto it = std::find(_func_names.begin(), _func_names.end(), fname);
      if(it == _func_names.end()) {
        spdlog::error("Function {} not found in the deployed library!", fname);
        return;
      }
      int func_idx = std::distance(_func_names.begin(), it);

      // FIXME: here get a future for async
      char* data = static_cast<char*>(in.ptr());
      // TODO: we assume here uintptr_t is 8 bytes
      *reinterpret_cast<uint64_t*>(data) = out.address();
      *reinterpret_cast<uint32_t*>(data + 8) = out.rkey();

      this->_invoc_id++;
      if (this->_invoc_id == 65536) {
	this->_invoc_id = 1;
      }
      int invoc_id = this->_invoc_id;

      _callbacks[invoc_id] = std::make_tuple(1, result_callback, &out);
      uint32_t submission_id = (invoc_id << 16) | (1 << 15) | func_idx;
      SPDLOG_DEBUG(
        "Invoke function {} with invocation id {}, submission id {}",
        func_idx, invoc_id, submission_id
      );
      if(size != -1) {
        rdmalib::ScatterGatherElement sge;
        sge.add(in, size, 0);
        _connections[0].conn->post_write(
          std::move(sge),
          _connections[0].remote_input,
          submission_id,
          size <= _device.max_inline_data,
          true
        );
      } else {
        _connections[0].conn->post_write(
          in,
          _connections[0].remote_input,
          submission_id,
          in.bytes() <= _device.max_inline_data,
          true 
        );
      }
      _connections[0].conn->poll_wc(rdmalib::QueueType::SEND, false);
      _connections[0].conn->receive_wcs().refill();
    }
    
    template<typename T>
    void async_callback(std::string fname, const rdmalib::Buffer<T> & in, 
               rdmalib::Buffer<char> & out, void *para, rfaas_result_callback2_t result_callback, 
               int64_t size = -1)
    {
      auto it = std::find(_func_names.begin(), _func_names.end(), fname);
      if(it == _func_names.end()) {
        spdlog::error("Function {} not found in the deployed library!", fname);
        return;
      }
      auto cid = reinterpret_cast<size_t>(para);
      int func_idx = std::distance(_func_names.begin(), it);

      // FIXME: here get a future for async
      char* data = static_cast<char*>(in.ptr());
      // TODO: we assume here uintptr_t is 8 bytes
      *reinterpret_cast<uint64_t*>(data) = out.address();
      *reinterpret_cast<uint32_t*>(data + 8) = out.rkey();

      this->_invoc_id++;
      if (this->_invoc_id == 65536) {
	this->_invoc_id = 1;
      }
      int invoc_id = this->_invoc_id;

      _callbacks2[invoc_id] = std::make_tuple(1, result_callback, &out, para);
      uint32_t submission_id = (invoc_id << 16) | (1 << 15) | func_idx;
      SPDLOG_DEBUG(
        "Invoke function {} with invocation id {}, submission id {}",
        func_idx, invoc_id, submission_id
      );
      if(size != -1) {
        rdmalib::ScatterGatherElement sge;
        sge.add(in, size, 0);
        _connections[cid].conn->post_write(
          std::move(sge),
          _connections[cid].remote_input,
          submission_id,
          size <= _device.max_inline_data,
          true
        );
      } else {
        _connections[cid].conn->post_write(
          in,
          _connections[cid].remote_input,
          submission_id,
          in.bytes() <= _device.max_inline_data,
          true 
        );
      }
      _connections[cid].conn->poll_wc(rdmalib::QueueType::SEND, false);
      for(size_t i = 0; i < _connections.size(); ++i) {
        _connections[i].conn->receive_wcs().refill();
      }
    }
    template<typename T,typename U>
    std::future<int> async(std::string fname, const std::vector<rdmalib::Buffer<T>> & in, std::vector<rdmalib::Buffer<U>> & out)
    {
      auto it = std::find(_func_names.begin(), _func_names.end(), fname);
      if(it == _func_names.end()) {
        spdlog::error("Function {} not found in the deployed library!", fname);
        return std::future<int>{};
      }
      int func_idx = std::distance(_func_names.begin(), it);

      this->_invoc_id++;
      if (this->_invoc_id == 65536) { // rfaas only reserved 16 bits for the invocation id, so we need to reset it to 0
        this->_invoc_id = 1;
      }
      int invoc_id = this->_invoc_id;

      //_futures[invoc_id] = std::move(std::promise<int>{});
      int numcores = _connections.size();
      _futures[invoc_id] = std::make_tuple(numcores, std::promise<int>{});
      uint32_t submission_id = (invoc_id << 16) | (1 << 15) | func_idx;
      for(int i = 0; i < numcores; ++i) {
        // FIXME: here get a future for async
        char* data = static_cast<char*>(in[i].ptr());
        // TODO: we assume here uintptr_t is 8 bytes
        *reinterpret_cast<uint64_t*>(data) = out[i].address();
        *reinterpret_cast<uint32_t*>(data + 8) = out[i].rkey();

        SPDLOG_DEBUG("Invoke function {} with invocation id {}", func_idx, _invoc_id);
        _connections[i].conn->post_write(
          in[i],
          _connections[i].remote_input,
          submission_id,
          in[i].bytes() <= _device.max_inline_data,
          true
        );
      }

      for(int i = 0; i < numcores; ++i) {
        _connections[i].conn->receive_wcs().refill();
      }
      return std::get<1>(_futures[invoc_id]).get_future();
    }

    bool block()
    {
      _connections[0].conn->poll_wc(rdmalib::QueueType::SEND, true);

      //auto wc = _connections[0]._rcv_buffer.poll(true);
      auto wc = _connections[0].conn->receive_wcs().poll(true);
      uint32_t val = ntohl(std::get<0>(wc)[0].imm_data);
      int return_val = val & 0x0000FFFF;
      int finished_invoc_id = val >> 16;
      if(return_val == 0) {
        SPDLOG_DEBUG("Finished invocation {} succesfully", finished_invoc_id);
        return true;
      } else {
        if(val == 1)
          spdlog::error("Invocation: {}, Thread busy, cannot post work", finished_invoc_id);
        else
          spdlog::error("Invocation: {}, Unknown error {}", finished_invoc_id, val);
        return false;
      }
    }

    // FIXME: irange for cores
    // FIXME: now only operates on buffers
    //template<class... Args>
    //void execute(int numcores, std::string fname, Args &&... args)
    template<typename T, typename U>
    std::tuple<bool, int> execute(std::string fname, const rdmalib::Buffer<T> & in, rdmalib::Buffer<U> & out)
    {
      auto it = std::find(_func_names.begin(), _func_names.end(), fname);
      if(it == _func_names.end()) {
        spdlog::error("Function {} not found in the deployed library!", fname);
        return std::make_tuple(false, 0);
      }
      int func_idx = std::distance(_func_names.begin(), it);

      // FIXME: here get a future for async
      char* data = static_cast<char*>(in.ptr());
      // TODO: we assume here uintptr_t is 8 bytes
      *reinterpret_cast<uint64_t*>(data) = out.address();
      *reinterpret_cast<uint32_t*>(data + 8) = out.rkey();

      this->_invoc_id++;
      if (this->_invoc_id == 65536) {
        this->_invoc_id = 1;
      }
      int invoc_id = this->_invoc_id;

      SPDLOG_DEBUG(
        "Invoke function {} with invocation id {}, submission id {}",
        func_idx, invoc_id, (invoc_id << 16) | func_idx
      );
      _connections[0].conn->post_write(
        in,
        _connections[0].remote_input,
        (invoc_id << 16) | func_idx,
        in.bytes() <= _device.max_inline_data
      );
      _active_polling = true;
      //_connections[0]._rcv_buffer.refill();
      _connections[0].conn->receive_wcs().refill();

      bool found_result = false;
      int return_value = 0;
      int out_size = 0;
      while(!found_result) {
        //auto wc = _connections[0]._rcv_buffer.poll(true);
        auto wc = _connections[0].conn->receive_wcs().poll(true);
        for(int i = 0; i < std::get<1>(wc); ++i) {
          uint32_t val = ntohl(std::get<0>(wc)[i].imm_data);
          int return_val = val & 0x0000FFFF;
          int finished_invoc_id = val >> 16;

          if(finished_invoc_id == invoc_id) {
            found_result = true;
            return_value = return_val;
            out_size = std::get<0>(wc)[i].byte_len;
            //spdlog::info("Result for id {}", finished_invoc_id);
          } else {
            auto it = _futures.find(finished_invoc_id);
            //spdlog::info("Poll Future for id {}", finished_invoc_id);
            // if it == end -> we have a bug, should never appear
            //(*it).second.set_value(return_val);
            if(!--std::get<0>(it->second))
              std::get<1>(it->second).set_value(return_val);
          }
        }
        if(found_result) {
          _active_polling = false;
          //auto wc = _connections[0]._rcv_buffer.poll(false);
          auto wc = _connections[0].conn->receive_wcs().poll(false);
          // Catch very unlikely interleaving
          // Event arrives after we poll while the background thread is skipping
          // because we still hold the atomic
          // Thus, we later unset the variable since we're done
          for(int i = 0; i < std::get<1>(wc); ++i) {
            uint32_t val = ntohl(std::get<0>(wc)[i].imm_data);
            int return_val = val & 0x0000FFFF;
            int finished_invoc_id = val >> 16;
            auto it = _futures.find(finished_invoc_id);
            //spdlog::info("Poll Future for id {}", finished_invoc_id);
            // if it == end -> we have a bug, should never appear
            //(*it).second.set_value(return_val);
            if(!--std::get<0>(it->second))
              std::get<1>(it->second).set_value(return_val);
          }
        }
      }
      _connections[0].conn->poll_wc(rdmalib::QueueType::SEND, false);
      if(return_value == 0) {
        SPDLOG_DEBUG("Finished invocation {} succesfully", invoc_id);
        return std::make_tuple(true, out_size);
      } else {
        if(return_value == 1)
          spdlog::error("Invocation: {}, Thread busy, cannot post work", invoc_id);
        else
          spdlog::error("Invocation: {}, Unknown error {}", invoc_id, return_value);
        return std::make_tuple(false, 0);
      }
    }

    template<typename T>
    bool execute(std::string fname, const std::vector<rdmalib::Buffer<T>> & in, std::vector<rdmalib::Buffer<T>> & out)
    {
      auto it = std::find(_func_names.begin(), _func_names.end(), fname);
      if(it == _func_names.end()) {
        spdlog::error("Function {} not found in the deployed library!", fname);
        return false;
      }
      int func_idx = std::distance(_func_names.begin(), it);
     
      this->_invoc_id++;
      if (this->_invoc_id == 65536) {
        this->_invoc_id = 1;
      }
      int invoc_id = this->_invoc_id;

      int numcores = _connections.size();
      for(int i = 0; i < numcores; ++i) {
        // FIXME: here get a future for async
        char* data = static_cast<char*>(in[i].ptr());
        // TODO: we assume here uintptr_t is 8 bytes
        *reinterpret_cast<uint64_t*>(data) = out[i].address();
        *reinterpret_cast<uint32_t*>(data + 8) = out[i].rkey();

        SPDLOG_DEBUG("Invoke function {} with invocation id {}", func_idx, _invoc_id);
        _connections[i].conn->post_write(
          in[i],
          _connections[i].remote_input,
          (invoc_id << 16) | func_idx,
          in[i].bytes() <= _device.max_inline_data
        );
      }

      for(int i = 0; i < numcores; ++i) {
        //_connections[i]._rcv_buffer.refill();
        _connections[i].conn->receive_wcs().refill();
      }
      int expected = numcores;
      while(expected) {
        auto wc = _connections[0].conn->poll_wc(rdmalib::QueueType::SEND, true);
        expected -= std::get<1>(wc);
      }

      expected = numcores;
      bool correct = true;
      _active_polling = true;
      while(expected) {
        //auto wc = _connections[0]._rcv_buffer.poll(true);
        auto wc = _connections[0].conn->receive_wcs().poll(true);
        expected -= std::get<1>(wc);
        for(int i = 0; i < std::get<1>(wc); ++i) {
          uint32_t val = ntohl(std::get<0>(wc)[i].imm_data);
          int return_val = val & 0x0000FFFF;
          int finished_invoc_id = val >> 16;
          if(return_val == 0) {
            SPDLOG_DEBUG("Finished invocation {} succesfully", finished_invoc_id);
          } else {
            if(val == 1)
              spdlog::error("Invocation: {}, Thread busy, cannot post work", finished_invoc_id);
            else
              spdlog::error("Invocation: {}, Unknown error {}", finished_invoc_id, val);
          }
          correct &= return_val == 0;
        }
      }
      _active_polling = false;

      // We polled from connection number 0, time to update.
      //_connections[0]._rcv_buffer._requests += numcores - 1;
      _connections[0].conn->receive_wcs().update_requests(numcores - 1);
      for(int i = 1; i < numcores; ++i)
        _connections[i].conn->receive_wcs().update_requests(-1);
      return correct;
    }
    template<typename T, typename U>
    void stop_execute(std::string fname, const rdmalib::Buffer<T> & in, rdmalib::Buffer<U> & out)
    {
      auto it = std::find(_func_names.begin(), _func_names.end(), fname);
      if(it == _func_names.end()) {
        spdlog::error("Function {} not found in the deployed library!", fname);
        return;
      }
      int func_idx = -1; //we use func_idx = -1 to indicate the executor should stop

      // FIXME: here get a future for async
      char* data = static_cast<char*>(in.ptr());
      // TODO: we assume here uintptr_t is 8 bytes
      *reinterpret_cast<uint64_t*>(data) = out.address();
      *reinterpret_cast<uint32_t*>(data + 8) = out.rkey();

      this->_invoc_id++;
      if (this->_invoc_id == 65536) {
        this->_invoc_id = 1;
      }
      int invoc_id = this->_invoc_id;

      SPDLOG_DEBUG(
        "Invoke function {} with invocation id {}, submission id {}",
        func_idx, invoc_id, (invoc_id << 16) | func_idx
      );
      _connections[0].conn->post_write(
        in,
        _connections[0].remote_input,
        (invoc_id << 16) | func_idx,
        in.bytes() <= _device.max_inline_data
      );

      _connections[0].conn->poll_wc(rdmalib::QueueType::SEND, false);
    }
  };


}

#endif
