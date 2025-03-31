#pragma once

#include <memory>
#include <random>

#include "api.h"
#include "cards.h"
#include "map.h"
#include "player.h"

#ifdef COG_BUILD_WITH_RENDERING
class cog_renderer;
#endif

class CITYOFGOLD_API CACHE_ALIGNED cog_env {
private:

#ifdef COG_BUILD_WITH_RENDERING
  std::unique_ptr<cog_renderer> renderer;
#endif

  uint32_t seed;
  u_char n_players;
  u_char n_pieces;
  Difficulty difficulty;
  unsigned int max_steps;
  bool b_render;
  std::default_random_engine rng;
  std::array<Player, MAX_N_PLAYERS> players;
  std::array<float, MAX_N_PLAYERS> *rewards;
  u_char last_player;
  u_char agent_selection;
  ObsData *observations;
  ActionMask *selected_action_mask;
  Info *info;
  Map map;
  Shop shop;
  bool shop_free;
  bool done;
  spcl_action_ptr special_function;
  /*mask_override_ptr mask_override;*/
  unsigned int turn_counter;

  void update_observation(u_char agent, ActionMask &am);

  void next_agent();
  float get_reward(u_char agent);
  void maybe_end_turn();
  void cycle_phase();
  void maybe_cycle_phase();
  void maybe_play_card(const ActionData &action);

public:
  bool dead_step;

  cog_env();
  cog_env(uint32_t seed_, u_char n_players_, u_char n_pieces_,
          Difficulty difficulty_, unsigned int max_steps_, bool render_);
  ~cog_env();
  void init(ObsData &observations_, Info &info_,
            std::array<float, MAX_N_PLAYERS> &rewards_, ActionMask &selected_);

  void reset();
  void reset(uint32_t seed_, u_char n_players_, u_char n_pieces_,
             Difficulty difficulty_, unsigned int max_steps_, bool render_);

  void step(const ActionData &action);

  void render();

  const Map &get_map() const;
  uint32_t get_seed() const;
  u_char get_n_players() const;
  const Player &get_player(u_char n) const;
  u_char get_n_pieces() const;
  Difficulty get_difficulty() const;
  unsigned int get_max_steps() const;
  bool get_render() const;

  u_char get_agent_selection() const;
  bool get_done() const;
  const Info &get_info() const;
};

void clear_console();

