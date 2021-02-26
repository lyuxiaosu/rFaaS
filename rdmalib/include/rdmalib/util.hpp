
#ifndef __RDMALIB_UTIL_HPP__
#define __RDMALIB_UTIL_HPP__

#include <spdlog/spdlog.h>

namespace rdmalib { namespace impl {

  void traceback();

  void expect_true(bool flag);
  void expect_false(bool flag);

  template<typename U>
  void expect_zero(U && u)
  {
    if(u) {
      spdlog::error("Expected zero, found: {}, errno {}, message {}", u, errno, strerror(errno));
      traceback();
    }
    assert(!u);
  }

  template<typename U>
  void expect_nonzero(U && u)
  {
    if(!u) {
      spdlog::error("Expected non-zero, found: {}", u);
      traceback();
    }
    assert(u);
  }

  template<typename U>
  void expect_nonnull(U* ptr)
  {
    if(!ptr) {
      spdlog::error("Expected nonnull ptr");
      traceback();
    }
    assert(ptr);
  }

}}

#endif

