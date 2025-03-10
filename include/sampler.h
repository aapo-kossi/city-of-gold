#pragma once

#include "api.h"
#include "constants.h"
#include <random>

struct CITYOFGOLD_API CACHE_ALIGNED action_sampler {
  std::default_random_engine rng;
  action_sampler(uint32_t seed = 42) : rng(seed) {};
  std::vector<u_char> valid_buffer;

  void set_seed(size_t s) { rng.seed(s); }

  ActionData sample(const ActionMask &mask) {
    ActionData action = {0, 0, 0, 0, 0};

    // Sample `play`
    valid_buffer.clear();
    for (u_char i = 0; i < mask.play.size(); ++i) {
      if (mask.play[i]) {
        valid_buffer.push_back(i);
      }
    }
    if (!valid_buffer.empty()) {
      std::uniform_int_distribution<size_t> dist(0, valid_buffer.size() - 1);
      action.play = static_cast<u_char>(valid_buffer[dist(rng)]);
    }

    // Sample `play_special`
    valid_buffer.clear();
    for (u_char i = 0; i < mask.play_special.size(); ++i) {
      if (mask.play_special[i]) {
        valid_buffer.push_back(i);
      }
    }
    if (!valid_buffer.empty()) {
      std::uniform_int_distribution<size_t> dist(0, valid_buffer.size() - 1);
      action.play_special = static_cast<u_char>(valid_buffer[dist(rng)]);
    }

    // Sample `remove`
    valid_buffer.clear();
    for (u_char i = 0; i < mask.remove.size(); ++i) {
      if (mask.remove[i]) {
        valid_buffer.push_back(i);
      }
    }
    if (!valid_buffer.empty()) {
      std::uniform_int_distribution<size_t> dist(0, valid_buffer.size() - 1);
      action.remove = static_cast<u_char>(valid_buffer[dist(rng)]);
    }

    // Sample `move`
    valid_buffer.clear();
    for (u_char i = 0; i < mask.move.size(); ++i) {
      if (mask.move[i]) {
        valid_buffer.push_back(i);
      }
    }
    if (!valid_buffer.empty()) {
      std::uniform_int_distribution<size_t> dist(0, valid_buffer.size() - 1);
      action.move = static_cast<u_char>(valid_buffer[dist(rng)]);
    }

    // Sample `get_from_shop`
    valid_buffer.clear();
    for (u_char i = 0; i < mask.get_from_shop.size(); ++i) {
      if (mask.get_from_shop[i]) {
        valid_buffer.push_back(i);
      }
    }

    if (!valid_buffer.empty()) {
      std::uniform_int_distribution<size_t> dist(0, valid_buffer.size() - 1);
      action.get_from_shop = static_cast<u_char>(valid_buffer[dist(rng)]);
    }

    return action;
  };
};
