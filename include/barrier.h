// "barrier" -*- C++ -*-

// This implementation is based on libcxx/include/barrier
// Here using indexed threads instead
// Aapo Kössi, 22.3.2025

//===-- barrier.h --------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===---------------------------------------------------------------===//

#pragma once

#include "api.h"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <thread>

struct exponential_falloff_timed_wait {
  void operator()(std::chrono::nanoseconds elapsed) const {
    if (elapsed > std::chrono::milliseconds(128)) {
      std::this_thread::sleep_for(std::chrono::milliseconds(8));
    } else if (elapsed > std::chrono::microseconds(64)) {
      std::this_thread::sleep_for(elapsed / 2);
    } else if (elapsed > std::chrono::microseconds(4)) {
      std::this_thread::yield();
    } else {
    } // poll
  };
};
/*

The implementation of __tree_barrier is a classic tree barrier.

It looks different from literature pseudocode for one main reason:
    A great deal of attention has been paid to avoid cache line thrashing
    by flattening the tree structure into cache-line sized arrays, that
    are indexed in an efficient way.

*/

using __barrier_phase_t = uint8_t;

class __tree_barrier_base {

  using __tickets_t = std::array<std::atomic<__barrier_phase_t>, 64>;
  struct CACHE_ALIGNED /* naturally-align the heap state */ __state_t {
    __tickets_t __tickets;
  };

  ptrdiff_t m_expected;
  std::unique_ptr<__state_t[]> m_state;
  std::atomic<ptrdiff_t> m_expected_adjustment;

  std::atomic<__barrier_phase_t> m_phase;

  bool inner_arrive(__barrier_phase_t __old_phase, size_t __current) {
    const __barrier_phase_t __half_step = __old_phase + 1,
                            __full_step = __old_phase + 2;

    size_t __current_expected = static_cast<size_t>(m_expected);
    __current %= static_cast<size_t>((m_expected + 1) >> 1);

    for (size_t __round = 0;; ++__round) {
      if (__current_expected <= 1)
        return true;
      size_t const __end_node = ((__current_expected + 1) >> 1),
                   __last_node = __end_node - 1;
      for (;; ++__current) {
        if (__current == __end_node)
          __current = 0;
        __barrier_phase_t __expect = __old_phase;
        auto &__phase = m_state[__current].__tickets[__round];
        if (__current == __last_node && (__current_expected & 1)) {
          if (__phase.compare_exchange_strong(__expect, __full_step,
                                              std::memory_order_acq_rel))
            break; // I'm 1 in 1, go to next __round
        } else if (__phase.compare_exchange_strong(__expect, __half_step,
                                                   std::memory_order_acq_rel)) {
          return false; // I'm 1 in 2, done with arrival
        } else if (__expect == __half_step) {
          if (__phase.compare_exchange_strong(__expect, __full_step,
                                              std::memory_order_acq_rel))
            break; // I'm 2 in 2, go to next __round
        }
      }
      __current_expected = __last_node + 1;
      __current >>= 1;
    }
  }

public:
  using arrival_token = __barrier_phase_t;

  static constexpr ptrdiff_t max() noexcept { return PTRDIFF_MAX; }

  __tree_barrier_base(ptrdiff_t __expected)
      : m_expected(__expected), m_expected_adjustment(0),
        m_phase(static_cast<__barrier_phase_t>(0)) {
    size_t const __count = static_cast<size_t>(__expected + 1) >> 1;

    m_state = std::make_unique<__state_t[]>(__count);
  }

  [[nodiscard]] arrival_token arrive(size_t idx, ptrdiff_t __update) {
    size_t __current = idx;
    auto &__phase(m_phase);
    const auto __old_phase = __phase.load(std::memory_order_relaxed);
    for (; __update; --__update) {
      if (inner_arrive(__old_phase, __current)) {
        m_expected += m_expected_adjustment.load(std::memory_order_relaxed);
        m_expected_adjustment.store(0, std::memory_order_relaxed);
        __phase.store(__old_phase + 2, std::memory_order_release);
      }
    }
    return __old_phase;
  }

  void wait(arrival_token &&__old_phase) const {
    const auto test_fn = [this, __old_phase] {
      return m_phase.load(std::memory_order_acquire) != __old_phase;
    };
    auto wait_fn = exponential_falloff_timed_wait();

    const auto start = std::chrono::high_resolution_clock::now();
    while (!test_fn()) {
      const std::chrono::nanoseconds elapsed =
          std::chrono::high_resolution_clock::now() - start;
      wait_fn(elapsed);
    }
  }
};

class barrier {
  __tree_barrier_base m_base;

public:
  using arrival_token = typename __tree_barrier_base::arrival_token;

  static constexpr ptrdiff_t max() noexcept {
    return __tree_barrier_base::max();
  }

  explicit barrier(ptrdiff_t __count) : m_base(__count) {}

  barrier(barrier const &) = delete;
  barrier &operator=(barrier const &) = delete;

  [[nodiscard]] arrival_token arrive(size_t idx, ptrdiff_t __update = 1) {
    return m_base.arrive(idx, __update);
  }

  void wait(arrival_token &&__phase) const { m_base.wait(std::move(__phase)); }

  void arrive_and_wait(size_t i) { wait(arrive(i)); }
};
