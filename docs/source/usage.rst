
Usage
=====

Game mechanics
--------------

The game board consists of hexagonal tiles with different features with different
movement costs. A player wins the game if they reach an ending hex first, or before
the player who first reached an ending hex would take their next turn.

Movement is enabled by playing cards, which can have various resources or special
effects. Additional powerful cards can be purchased from a shop that is common
between all players.

A differentiating feature from most adversarial game environments is that the step
order is not pre-determined as a round-robin. Due to the action space complexity,
a single player turn consists of a sequence of steps, each of which corresponds
to one of:

1. playing a card normally
2. playing the special action of a card
3. movement to an adjacent hex
4. removing a card from the players hand
5. adding a card to the player deck from the shop

A player turn starts in the movement phase, in which cards can be played in order
to take advantage of their resources to move on the map. When an empty action is
taken, the turn moves to the shop phase, where played cards add coins to the player.
Then, given enough coins are accumulated, the player can buy a card from the shop,
which ends the turn. Some cards have special actions which overrule the phase
system until they have been completed. Such actions can be a free movement,
removing cards from the hand, and adding a card from the shop for free.

The observation space of the environment consists of a shared component and
data individual to each player. The shared observation contains an encoding of
the Map, where for each hex the following information is stored:

1. Currently occupying player
2. machete requirement
3. paddle requirement
4. coin requirement
5. discard requirement
6. remove requirement
7. is finish hex

The currently occupying player is preprocessed such that the hex of the currently
selected player has value 1, the player next to take their turn value 2, etc.
Unoccupied hexes have value 0. The various requirements encode how many resources
are required to move onto the hex. Discard requirements require the player to
discard n cards from their active cards and remove requirements likewise remove
the cards from play completely. The map observation consists of a 48x48 array
of the encoded hex data.

The shared observation also contains the shop, which stores simply how many of each
card of the shop are available to buy.

A single unsigned value stores the active turn phase. An array is used to store
the current resources of the active player.

Player specific observations store the information of the player's cards. They
are split to the drawpile, hand, active, played and discard arrays. Each of these
store the frequencies of each card type currently in that state. This gives the
benefit of order invariance over an actual queue/list of cards.

The player observation also contains an action mask, signaling the set of
valid actions that player can take at this time, based on the cards in hand,
current resources, possibly active special actions and their surrounding hexes.

The environment mechanics are similar to the "The Quest for El Dorado"
board-game. No external assets are used, and no UI is implemented
for "playing" the City of Gold game outside Python scripting or interfacing
with C++ code. This code is intended as a tool to research and validate
reinforcement learning methods and models in a complex, procedurally generated
environment with interactions between multiple players. As game mechanics alone
do not fall under copyright [1]_, this repository can be freely distributed and is licensed under Apache 2.0.

.. _installation:

Installation
------------

A pypi package of the environment is a work in progress. Currently you can install
the environment by cloning the repo and building the source code using the included
pyproject.toml configuration.

.. code-block:: console

    git clone git@github.com:aapo-kossi/city-of-gold.git
    pip install ./city-of-gold


.. _example:

Running the environment
-----------------------

The environment exposes multiple interfaces for running the game. For performance
considerations and due to the uniqueness of the observation and action spaces,
the interface does not conform to any existing framework like gymnasium or pettingzoo,
though the paradigm used by pettingzoo environments is an inspiration for this
environment, as this project originated as a python implementation that used
the pettingzoo api.

The main usage pattern of the environment follows the example below:

.. code-block:: python

    import multiprocessing as mp

    # Running environments in parallel
    n_envs = 16
    seed = 0
    workers = mp.cpu_count() - 1
    steps = 1_000

    env_cls = city_of_gold.get_vec_env(n_envs)
    sampler_cls = city_of_gold.get_vec_sampler(n_envs)
    envs = env_cls()
    samplers = sampler_cls(seed)
    runner_cls = city_of_gold.get_runner(n_envs)
    runner = runner_cls(envs, samplers, workers)

    envs.reset(seed, 4, 3, city_of_gold.Difficulty.EASY, 100000, False)

    # get reference to persistent actions vector, updated internally
    actions = samplers.get_actions()

    # get references to internal data structures
    next_agents = np.expand_dims(envs.agent_selection, 1)
    next_obs = envs.observations
    am = next_obs["player_data"]["action_mask"]
    player_masks = envs.selected_action_masks
    current_rewards = envs.rewards
    current_dones = envs.dones
    current_infos = envs.infos

    start = time.time()
    for i in range(steps):

        # update actions array using action_masks
        runner.sample()

        # step the environments with the sampled actions, block until ready!
        runner.step_sync()

        # print info from finished episodes
        for i in np.nonzero(current_dones)[0]:
            print(current_infos[i])


Primitive rendering of the game map can be enabled for a single environment with
the `render` flag in the :py:func:`city_of_gold.cog_env` constructor. Then,
the current environment state can be displayed on screen by calling the
:py:func:`city_of_gold.cog_env.render` method. Below is an example render
of a game state.

.. image:: render.png
   :width: 600px
   :height: 400px
   :alt: A black background with hexagonal tiles in various colours with symbols displaying features of heach tile. Four player avatars are shown on top of their tiles.
   :align: center

The built-in action sampler is a simple uniform random agent, sampling from
all possible valid actions. Full reference documentation of the module interface
is a work in progress.

Not respecting the action mask of the current player when stepping the environment
results in undefined behaviour. It is the user's responsibility to prevent this
in their application. This is a choice based on performance considerations,
given that an RL algorithm learning the environment needs to mask its policy
distribution using the action mask, making verification inside the environment
unnecessary.

.. [1] https://www.khuranaandkhurana.com/2025/03/04/copyright-in-the-gaming-industry-protecting-game-developers-rights/
