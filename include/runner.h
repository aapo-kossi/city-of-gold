#pragma once

#include "api.h"
#include "atomic_queues.hpp" // jdz atomic queues
#include "vec_environment.h"
#include "vec_sampler.h"
#include <optional>
#include <thread>
#include <algorithm>
#ifdef _WIN32
#include <Windows.h>
#endif

constexpr size_t Q_SIZE = 256;

enum class Task { STEP, SAMPLE, STOP };

using q_type = typename jdz::SpscQueue<Task, Q_SIZE, jdz::EnforcePowerOfTwo,
                                       jdz::UseStackBuffer>;
using queue_ptr = typename std::unique_ptr<q_type>;

template <size_t N> class ThreadedRunner {
public:
  ThreadedRunner(vec_cog_env<N> &envs_, vec_action_sampler<N> &samplers_,
                 std::optional<size_t> thread_count = std::nullopt)
      : envs{envs_}, samplers{samplers_},
        action_masks{envs_.get_selected_action_masks()},
        actions{samplers.get_actions()} {
    n_threads = thread_count.value_or(std::thread::hardware_concurrency());
    task_queues.reserve(n_threads);
    workers.reserve(n_threads);
    total_pending_tasks.store(0, std::memory_order_relaxed);

    size_t base_batch_size = N / n_threads;
    size_t remainder = N % n_threads;

    for (size_t i = 0; i < n_threads; ++i) {
      task_queues.emplace_back(
          std::make_unique<q_type>());

      size_t batch_size = base_batch_size;
      if (i < remainder) {
        batch_size += 1;
      }

      size_t start = i * base_batch_size + std::min(i, remainder);
      size_t end = start + batch_size;

      workers.emplace_back([this, start, end, i] {
        set_thread_affinity(i);
        bool done = false;
        while (!done) {
          Task task;
          task_queues[i]->pop(task);
          switch (task) {
          case Task::SAMPLE:
            for (size_t j = start; j < end; j++) {
              samplers.sample_single(action_masks[j], j);
            }
            break;
          case Task::STEP:
            for (size_t j = start; j < end; j++) {
              envs.step_single(actions[j], j);
            }
            break;
          case Task::STOP:
            done = true;
            break;
          }
          total_pending_tasks.fetch_sub(1, std::memory_order_relaxed);
        }
      });
    }
  }

  ~ThreadedRunner() {
    for (size_t i = 0; i < n_threads; i++) {
      run_async<Task::STOP>();
    }
    for (auto &worker : workers) {
      if (worker.joinable()) {
        worker.join();
      }
    }
  }

  size_t get_n_threads() const { return n_threads; }
  vec_cog_env<N> &get_envs() const { return envs; }
  vec_action_sampler<N> &get_samplers() const { return samplers; }

  void enqueue(Task task, size_t worker) {
    size_t thread_idx = worker % n_threads;
    total_pending_tasks.fetch_add(1, std::memory_order_relaxed);
    task_queues[thread_idx]->push(task);
  }

  template <Task T> void run_async() {
    for (size_t t = 0; t < n_threads; t++) {
      enqueue(T, t);
    }
  }

  void step() { run_async<Task::STEP>(); }
  void sample() { run_async<Task::SAMPLE>(); }

  void sync() {
    while (total_pending_tasks.load(std::memory_order_acquire) > 0) {
      std::this_thread::yield();
    }
  }
  const std::array<ActionData, N> &get_actions() const { return actions; }
  const std::array<ActionMask, N> &get_action_masks() const {
    return action_masks;
  }

private:
  size_t n_threads;
  vec_cog_env<N> &envs;
  vec_action_sampler<N> &samplers;

  const std::array<ActionMask, N> &action_masks;
  const std::array<ActionData, N> &actions;

  std::vector<std::thread> workers;
  std::vector<queue_ptr> task_queues;
  std::atomic<size_t> total_pending_tasks;
  static void set_thread_affinity(size_t core_id) {
#if defined(__linux__)
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    pthread_t thread = pthread_self();
    pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);

#elif defined(_WIN32)
    HANDLE thread = GetCurrentThread();
    DWORD_PTR mask = 1ULL << core_id;
    SetThreadAffinityMask(thread, mask);

#else
    (void)core_id; // No-op on unsupported platforms
#endif
  }
};
