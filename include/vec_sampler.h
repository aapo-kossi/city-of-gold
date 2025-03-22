#pragma once

#include "api.h"
#include "sampler.h"
#include <array>

template <size_t N> class vec_action_sampler {
public:
  vec_action_sampler(std::array<ActionData, N> &act, uint32_t seed = 42)
      : samplers{}, actions{act} {
    for (size_t i = 0; i < N; i++) {
      samplers[i].set_seed(seed + i);
    }
  }
  void sample(const std::array<ActionMask, N> &am) {
    for (size_t i = 0; i < N; i++) {
      sample_single(am[i], i);
    }
  }
  void sample_single(const ActionMask &am, size_t i) {
    actions[i] = samplers[i].sample(am);
  }

  const std::array<ActionData, N> &get_actions() const { return actions; }

private:
  std::array<action_sampler, N> samplers;
  std::array<ActionData, N> &actions;
};
