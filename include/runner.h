#pragma once

#include "api.h"
#include "barrier.h"
#include "vec_environment.h"
#include "vec_sampler.h"
#include <atomic>
#include <optional>
#include <thread>
#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#endif

enum class Task : uint8_t { STEP, SAMPLE, STOP, SYNC, SLEEP };

// queue-like buffer with spin-locking iterator. For resource-constrained
// environments you should either submit the SLEEP task when workers
// are known to not be needed to free resources, or prefer sequential
// execution. Using OS level wait and notify functionality, threading
// overhead would exceed any benefits of parallelisation for reasonable N.
template <typename T> class CACHE_ALIGNED  atomic_ringbuffer {
  static constexpr size_t SIZE = 8;
  using idx_t = uint32_t;

private:
  // tail and tasks are always written in conjunction when pushing a new item
  // By having the tail and task array on the same cache line,
  // we can hope to only fetch the single line each time this happens
  // This holds for T s.t. sizeof(T) <= 4
  std::atomic<idx_t> tail{0};
  std::array<T, SIZE> tasks;

  void increment(std::atomic<idx_t> &idx) {
    idx_t orig_idx = idx.load(std::memory_order_relaxed);
    idx.store((orig_idx + 1) % SIZE, std::memory_order_relaxed);
  }
  void increment(size_t &idx) { idx = (idx + 1) % SIZE; }

public:
  void push(T &&t) {
    tasks[tail.load(std::memory_order_relaxed)] = std::move(t);
    increment(tail);
  }

  void get(T &t, size_t &i) {
    while (tail.load(std::memory_order_relaxed) == i) {
      std::this_thread::yield();
    }
    t = tasks[i];
    increment(i);
  }
};

template <size_t N> class ThreadedRunner {
  using q_type = atomic_ringbuffer<Task>;

public:
  ThreadedRunner(std::optional<size_t> thread_count = std::nullopt)
      : envs{}, action_masks{envs.get_selected_action_masks()}, actions{},
        n_threads{thread_count.value_or(
            std::thread::hardware_concurrency() > 1
                ? std::min(static_cast<size_t>(
                               std::thread::hardware_concurrency() - 1),
                           N)
                : 1)},
        sync_barrier{static_cast<ptrdiff_t>(n_threads + 1)} {
    workers.reserve(n_threads);
    total_pending_tasks.store(0, std::memory_order_relaxed);
  }

  void start_workers() {
    size_t base_batch_size = N / n_threads;
    size_t remainder = N % n_threads;

    for (size_t i = 0; i < n_threads; ++i) {

      size_t batch_size = base_batch_size;
      if (i < remainder) {
        batch_size += 1;
      };

      size_t start = i * base_batch_size + std::min(i, remainder);
      size_t end = start + batch_size;
      workers.emplace_back([this, start, end, i] {
        bool done = false;
        size_t q_idx = 0;
        while (!done) {
          Task task;
          task_queue.get(task, q_idx);
          switch (task) {
          case Task::SAMPLE:
            assert(
                samplers.has_value() &&
                "Tried to sample actions but no sampler has been instantiated");
            for (size_t j = start; j < end; j++) {
              samplers->sample_single(action_masks[j], j);
            }
            break;
          case Task::STEP:
            for (size_t j = start; j < end; j++) {
              envs.step_single(actions[j], j);
            }
            sync_barrier.arrive_and_wait(i + 1);
            break;
          case Task::STOP:
            done = true;
            break;
          case Task::SYNC:
            sync_barrier.arrive_and_wait(i + 1);
            break;
          case Task::SLEEP:
            active.wait(true, std::memory_order_relaxed);
          }
        }
      });
    }
    active.store(true, std::memory_order_relaxed);
  }

  ~ThreadedRunner() {
    run_async<Task::STOP>();
    for (auto &worker : workers) {
      if (worker.joinable()) {
        worker.join();
      }
    }
  }
  // The runner is not copy or move-assignable or constructible (it owns
  // threads)
  ThreadedRunner &operator=(ThreadedRunner<N> &) = delete;
  ThreadedRunner(ThreadedRunner<N> &) = delete;
  ThreadedRunner &operator=(ThreadedRunner<N> &&) = delete;
  ThreadedRunner(ThreadedRunner<N> &&) = delete;

  size_t get_n_threads() const { return n_threads; }
  vec_cog_env<N> &get_envs() { return envs; }
  vec_action_sampler<N> &get_samplers() {
    if (!samplers.has_value()) {
      samplers.emplace(actions);
    }

    return *samplers;
  }

  void make_samplers(uint32_t s) { samplers.emplace(actions, s); }

  template <Task T> void run_async() { task_queue.push(T); }

  void step() {
    run_async<Task::STEP>();
    sync_barrier.arrive_and_wait(0);
  }
  void step_seq() { envs.step(actions); }
  void sample() { run_async<Task::SAMPLE>(); }
  void sample_seq() { samplers->sample(action_masks); }

  void sync() {
    run_async<Task::SYNC>();
    sync_barrier.arrive_and_wait(0);
  }
  void sleep() {
    active.store(false, std::memory_order_relaxed);
    run_async<Task::SLEEP>();
  }
  void wake() {
    active.store(true, std::memory_order_relaxed);
    active.notify_all();
  }
  std::array<ActionData, N> &get_actions() { return actions; }
  const std::array<ActionMask, N> &get_action_masks() const {
    return action_masks;
  }

private:
  size_t n_threads;
  vec_cog_env<N> envs;
  std::optional<vec_action_sampler<N>> samplers;

  const std::array<ActionMask, N> &action_masks;
  std::array<ActionData, N> actions;
  std::array<size_t, N> idx;

  barrier sync_barrier;
  std::atomic<bool> active = false;

  std::vector<std::thread> workers;
  q_type task_queue;
  std::atomic<size_t> total_pending_tasks;
};
