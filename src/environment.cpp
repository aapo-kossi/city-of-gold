#include "api.h"
#include "cards.h"
#include "constants.h"
#include "environment.h"
#include <cassert>
#include <iostream>
#include <random>

#ifdef COG_BUILD_WITH_RENDERING
#include "render.h"
#endif

cog_env::cog_env()
    : seed(std::random_device()()), n_players(MAX_N_PLAYERS),
      n_pieces(DEFAULT_N_PIECES), difficulty(DEFAULT_DIFFICULTY),
      max_steps(MAX_STEPS), b_render(false), rng(seed), observations(nullptr),
      info(nullptr), map(), shop(), shop_free(false),
      special_function(nullptr) {};

cog_env::cog_env(uint32_t seed_, u_char n_players_, u_char n_pieces_,
                 Difficulty difficulty_, unsigned int max_steps_, bool render)
    : seed(seed_), n_players(n_players_), n_pieces(n_pieces_),
      difficulty(difficulty_), max_steps(max_steps_), b_render(render),
      rng(seed), observations(nullptr), info(nullptr), map(), shop(),
      shop_free(false), special_function(nullptr) {};

cog_env::~cog_env() = default;

void cog_env::init(ObsData &observations_, Info &info_,
                   std::array<float, MAX_N_PLAYERS> &rewards_,
                   ActionMask &selected_) {
  observations = &observations_;
  info = &info_;
  rewards = &rewards_;
  selected_action_mask = &selected_;
  map.init(observations->shared.map);
  shop.init(observations->shared.shop);
  for (u_char i = 0; i < n_players; i++) {
    players[i].initialize(i, &rng, &observations->player_data[i].obs,
                          selected_action_mask,
                          &observations->player_data[i].action_mask,
                          &observations->shared.current_resources);
  }
#ifdef COG_BUILD_WITH_RENDERING
  if (b_render) {
    renderer = std::make_unique<cog_renderer>(this);
    if (renderer->get_state() != SDL_APP_CONTINUE) {
      throw std::runtime_error("Could not create renderer!");
    }
  }
#endif
}

void cog_env::reset() {

  agent_selection = 0;
  observations->shared.phase = static_cast<u_char>(TurnPhase::INACTIVE);

  map.reset();
  map.generate(n_pieces, difficulty, 0, MAX_FAILURES, rng);
  for (u_char i = 0; i < n_players; i++) {
    players[i].reset();
  }
  map.add_players(n_players);

  shop.reset();

  done = false;
  special_function = nullptr;
  turn_counter = 0;

  for (u_char i = 0; i < n_players; i++) {
    update_observation(i, observations->player_data[i].action_mask);
  }
  *selected_action_mask =
      observations->player_data[agent_selection].action_mask;
#ifdef COG_BUILD_WITH_RENDERING
  if (b_render) {
    renderer->set_map_size();
  }
#endif
};

void cog_env::reset(uint32_t seed_, u_char n_players_, u_char n_pieces_,
                    Difficulty difficulty_, unsigned int max_steps_,
                    bool render_) {
  n_players = n_players_;
  n_pieces = n_pieces_;
  difficulty = difficulty_;
  max_steps = max_steps_;
  seed = seed_;
  rng.seed(seed);
  b_render = render_;
  reset();
}

void cog_env::next_agent() {
  Player &p = players[agent_selection];
  p.end_turn();
  agent_selection += 1;
  if (agent_selection >= n_players) {
    agent_selection = 0;
  }
  players[agent_selection].load_actionmask();
  observations->shared.current_resources.fill(0);
  turn_counter++;
}

void cog_env::step(const ActionData &action) {
  dead_step = done;
  if (dead_step) {
    return;
  }

  info->agent_infos[agent_selection].steps_taken += 1;
  maybe_cycle_phase();

  Player &p = players[agent_selection];
  p.stepped();

  // activating card
  if (action.play) {
    u_char i = action.play - 1;
    p.play_card(static_cast<CardType>(i),
                static_cast<TurnPhase>(observations->shared.phase));

    // playing card with special action
  } else if (action.play_special) {
    special_function =
        p.play_special(static_cast<CardType>(action.play_special - 1));

    // player moving
  } else if (action.move) {
    MovementInfo data = map.move_in_direction(agent_selection, action.move);
    if (!p.next_move_free) {
      p.handle_requirement(data.requirement, data.n_required);
    } else {
      p.next_move_free = false;
      p.enable_playing();
    }
    p.moved();
    p.has_won = data.is_end;

    // action is not playing or moving, thus native cannot be active!
    // remaining checks for getting new card and removing a card
  } else {
    p.next_move_free = false;

    // adding card to the player deck
    if (action.get_from_shop) {
      u_char i = action.get_from_shop - 1;
      const Card *card;
      if (p.next_card_free) {
        card = &shop.transmit(i);
      } else {
        card = &shop.buy(i);
        p.pay(card->cost);
        cycle_phase();
      }
      p.add_card(card->type);

      // Removing card from the player deck
    } else if (action.remove) {
      u_char card_to_remove = action.remove - 1;
      p.remove_from_hand(card_to_remove);

      // Check if this was the last available remove
      if (!--p.n_removes) {
        p.enable_playing();

        // if not, shop needs to still be disabled to prevent removes
        // persisting to the next turn
      } else {
        special_function = [](ActionMask &mask, Player &, Map &, Shop &s) {
          s.set_available_mask(0, mask.get_from_shop);
        };
      };

      // action was null, remove actions cannot be delayed
    } else {
      cycle_phase();
      if (p.n_removes > 0) {
        p.n_removes = 0;
        p.enable_playing();
      }
    }

    if (p.next_card_free) {
      p.next_card_free = false;
      p.enable_playing();
    }
  }

  if (p.movement_in_progress && !action.move) {
    p.movement_in_progress = false;
    observations->shared.current_resources.fill(0);
  }

  maybe_end_turn();
  update_observation(agent_selection, *selected_action_mask);
  if (special_function != nullptr) {
    special_function(*selected_action_mask, p, map, shop);
    special_function = nullptr;
  } else if (map.player_done(agent_selection) || (turn_counter >= max_steps)) {

    done = true;
    info->total_length = turn_counter;
    for (u_char agent = 0; agent < n_players; agent++) {
      Player &player = players[agent];
      AgentInfo &agent_info = info->agent_infos[agent];
      agent_info.steps_taken = player.get_steps_taken();
      agent_info.returns = (*rewards)[agent] = get_reward(agent);
      agent_info.travelled_hexes = player.get_n_movements();
      agent_info.cards_added = player.get_n_added_cards();
      const std::array<unsigned int, N_RESOURCETYPES> &n_spent =
          player.get_n_spent();
      agent_info.n_machete_uses =
          n_spent[static_cast<u_char>(Resource::MACHETE)];
      agent_info.n_paddle_uses = n_spent[static_cast<u_char>(Resource::PADDLE)];
      agent_info.n_coin_uses = n_spent[static_cast<u_char>(Resource::COIN)];
      agent_info.n_card_uses = player.get_n_discarded();
      agent_info.cards_removed = player.get_n_removed();
    }
  }
  [[maybe_unused]] auto &pd = observations->player_data[agent_selection].obs;
  [[maybe_unused]] auto &pm = *selected_action_mask;
  for (size_t i = 1; i < N_CARDTYPES + 1; i++) {
    assert(!(pm.play[i] && !pd.hand[i - 1]) && "play mask fucked");
    assert(!(pm.play_special[i] && !pd.hand[i - 1]) && "special mask fucked");
    assert(!(pm.remove[i] && !pd.hand[i - 1]) && "remove mask fucked");
    assert((pd.draw[i - 1] <= MAX_CARD_COPIES) && "draw invalid");
    assert((pd.hand[i - 1] <= MAX_CARD_COPIES) && "hand invalid");
    assert((pd.active[i - 1] <= MAX_CARD_COPIES) && "active invalid");
    assert((pd.played[i - 1] <= MAX_CARD_COPIES) && "played invalid");
    assert((pd.discard[i - 1] <= MAX_CARD_COPIES) && "discard invalid");
  };
  for (size_t i = 0; i < static_cast<size_t>(Resource::MAX_RESOURCE); i++) {
    assert((observations->shared.current_resources[i] >= 0.0) &&
           "resources fucked");
  }
};

void cog_env::maybe_cycle_phase() {
  if (observations->shared.phase == static_cast<u_char>(TurnPhase::INACTIVE)) {
    observations->shared.phase = static_cast<u_char>(
        ::cycle_phase(static_cast<TurnPhase>(observations->shared.phase)));
  }
}

// Turn phases change according to the following logic:
// The movement phase ends if the player neither plays a card or moves
// The shop phase ends when the player is not playing a card
// This shop behaviour naturally limits the player to buying only
// a maximum of a single card per turn
// Special actions handle their turn phase logic independently
inline void cog_env::cycle_phase() {
  observations->shared.phase = static_cast<u_char>(
      ::cycle_phase(static_cast<TurnPhase>(observations->shared.phase)));
}

void cog_env::maybe_end_turn() {
  Player &player = players[agent_selection];
  if (player.has_won || (observations->shared.phase ==
                         static_cast<u_char>(TurnPhase::INACTIVE))) {
    next_agent();
  }
}

void cog_env::update_observation(u_char agent, ActionMask &am) {

  am.move.fill(false);
  am.move[0] = true;
  am.get_from_shop.fill(false);
  am.get_from_shop[0] = true;

  switch (static_cast<TurnPhase>(observations->shared.phase)) {
  case TurnPhase::INACTIVE:
    break;
  case TurnPhase::MOVEMENT:
    map.set_movement_mask(am, agent, observations->shared.current_resources,
                          players[agent].get_n_active());
    break;
  case TurnPhase::BUYING:
    shop.set_available_mask(
        observations->shared
            .current_resources[static_cast<u_char>(Resource::COIN)],
        am.get_from_shop);

    break;
  case TurnPhase::MAX_PHASE:
    assert(false && "Environment in invalid turn phase");
    break;
  }
};

float cog_env::get_reward(u_char agent) {
  float n_winners = 0;
  for (auto player : players) {
    n_winners += player.has_won;
  };

  return n_players * players[agent].has_won - n_winners;
};

void cog_env::render() {
  if (b_render) {
#ifdef COG_BUILD_WITH_RENDERING
    renderer->render();
#else
    assert(false && "This library was not built with support for rendering!");
#endif
  } else {
    std::cout << "Env not initialized with rendering enabled!" << std::endl;
  }
}
const Map &cog_env::get_map() const { return map; };
uint32_t cog_env::get_seed() const { return seed; };
u_char cog_env::get_n_players() const { return n_players; };
const Player &cog_env::get_player(u_char n) const { return players[n]; };
u_char cog_env::get_n_pieces() const { return n_pieces; };
Difficulty cog_env::get_difficulty() const { return difficulty; };
unsigned int cog_env::get_max_steps() const { return max_steps; };
bool cog_env::get_render() const { return b_render; };
bool cog_env::get_done() const { return done; };
const Info &cog_env::get_info() const { return *info; };

u_char cog_env::get_agent_selection() const { return agent_selection; };
