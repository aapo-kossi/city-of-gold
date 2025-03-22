#pragma once

#include <array>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <string>

#include "api.h"
#include "constants.h"
#include "pybind/common.h"
#include "runner.h"
#include "vec_environment.h"
#include "vec_sampler.h"

namespace py = pybind11;
using namespace pybind11::literals;

extern std::string_view vec_env_cls;
extern std::string_view vec_sampler_cls;
extern std::string_view vec_runner_cls;

template <size_t N> class py_vec_env {
private:
  vec_cog_env<N> &env;
  constexpr const static std::array<ptrdiff_t, 3> map_strides = {
      GRIDSIZE, GRIDSIZE, N_MAP_FEATURES};
  constexpr const static std::array<ptrdiff_t, 1> shop_strides = {
      N_BUYABLETYPES};
  constexpr const static std::array<ptrdiff_t, 1> cards_strides = {N_CARDTYPES};
  constexpr const static std::array<ptrdiff_t, 1> resource_strides = {
      N_RESOURCETYPES};

  constexpr const static std::array<ptrdiff_t, 1> play_mask_strides = {
      N_CARDTYPES + 1};
  constexpr const static std::array<ptrdiff_t, 1> special_mask_strides = {2};
  constexpr const static std::array<ptrdiff_t, 1> shop_mask_strides = {
      N_BUYABLETYPES + 1};
  constexpr const static std::array<ptrdiff_t, 1> move_mask_strides = {
      N_DIRECTIONS};
  constexpr const static std::array<ptrdiff_t, 2> remove_mask_strides = {
      N_CARDTYPES, MAX_CARD_COPIES + 1};

  constexpr const static std::array<ptrdiff_t, 1> n_envs_stride = {N};
  constexpr const static std::array<ptrdiff_t, 2> n_players_stride = {
      N, MAX_N_PLAYERS};

public:
  py_vec_env(vec_cog_env<N> &env_) : env{env_} {}

  void reset() { env.reset(); }

  void reset(uint32_t seed, u_char n_players, u_char n_pieces,
             Difficulty difficulty, unsigned int max_steps, bool render) {
    env.reset(seed, n_players, n_pieces, difficulty, max_steps, render);
  }

  void step(const py::array_t<ActionData> &actions) {
    py::buffer_info buf = actions.request();

    // Unsafe but also fast :D
    const auto &action_array =
        *reinterpret_cast<std::array<ActionData, N> *>(buf.ptr);

    env.step(action_array);
  }

  const py::array_t<ObsData> observe() {
    const auto &obs_array = env.get_observations();
    return create_numpy_view(&obs_array[0], n_envs_stride);
  }

  size_t get_num_envs() const { return env.get_num_envs(); }

  py::array_t<u_char> get_agent_selection() const {
    const std::array<u_char, N> &agents = env.get_agent_selections();
    return create_numpy_view(&agents[0], n_envs_stride);
  }

  py::array_t<bool> get_dones() const {
    const std::array<bool, N> &dones = env.get_dones();
    return create_numpy_view(&dones[0], n_envs_stride);
  }

  py::array_t<float> get_rewards() const {
    const std::array<std::array<float, MAX_N_PLAYERS>, N> &rewards =
        env.get_rewards();
    return create_numpy_view(&rewards[0][0], n_players_stride);
  }

  py::array_t<Info> get_infos() const {
    const std::array<Info, N> &infos_array = env.get_infos();
    return create_numpy_view(&infos_array[0], n_envs_stride);
  }

  py::array_t<ActionMask> get_selected_action_masks() const {
    const std::array<ActionMask, N> &masks_array =
        env.get_selected_action_masks();
    return create_numpy_view(&masks_array[0], n_envs_stride);
  }

  vec_cog_env<N> &get_env() { return env; }
};

template <size_t N> class py_vec_action_sampler {
private:
  constexpr const static std::array<ptrdiff_t, 1> n_envs_stride = {N};
  vec_action_sampler<N> &samplers;

public:
  py_vec_action_sampler(vec_action_sampler<N> &samplers_)
      : samplers{samplers_} {}
  py::array_t<ActionData> get_actions() {
    const std::array<ActionData, N> &act_arr = samplers.get_actions();
    return create_numpy_view(&act_arr[0], n_envs_stride);
  }
  void sample(const py::array_t<ActionMask> &am) {
    py::buffer_info buf = am.request();

    // Unsafe but also fast :D
    const auto &am_array =
        *reinterpret_cast<std::array<ActionMask, N> *>(buf.ptr);
    samplers.sample(am_array);
  }
  vec_action_sampler<N> &get_sampler() { return samplers; }
};

template <size_t N> class py_threaded_runner {
private:
  ThreadedRunner<N> runner;
  constexpr const static std::array<ptrdiff_t, 1> n_envs_stride = {N};

public:
  py_threaded_runner(std::optional<size_t> n_threads) : runner(n_threads) {}
  py_vec_env<N> get_envs() { return py_vec_env<N>(runner.get_envs()); }
  size_t get_n_threads() const { return runner.get_n_threads(); }
  void start_workers() { runner.start_workers(); }
  py_vec_action_sampler<N> get_samplers() {
    return py_vec_action_sampler<N>(runner.get_samplers());
  }
  void make_samplers(uint32_t seed) { runner.make_samplers(seed); }
  py::array_t<ActionData> get_actions() {

    auto &actions = runner.get_actions();
    return create_numpy_view(&actions[0], n_envs_stride);
  }
  py::array_t<ActionMask> get_action_masks() const {

    const auto &am = runner.get_action_masks();
    const auto *data = &am[0];
    return create_numpy_view(data, n_envs_stride);
  }
  void sample() { runner.sample(); }
  void step() { runner.step(); }
  void sample_seq() { runner.sample_seq(); }
  void step_seq() { runner.step_seq(); }
  void sync() { runner.sync(); }
  void sleep() { runner.sleep(); }
  void wake() { runner.wake(); }
};

template <size_t N> void bind_vec_env(py::module_ &m) {
  std::string num = std::to_string(N);
  std::string name = std::string(vec_env_cls) + num;
  std::string doc =
      R"pbdoc(
      Vectorized city of gold environment for )pbdoc" +
      num + R"pbdoc( environments.

      :meth:`)pbdoc" +
      name + R"pbdoc(.reset` must be called
      to initialize the environments before stepping.
      This also allows setting the parameters for
      random number generation, number of players, and map generation.

      Sets default game parameters:

      * seed: std::random_device()()
      * n_players: 4
      * n_pieces: 3
      * difficulty: Difficulty.EASY
      * max_steps: 100_000
      * render: False

      :return: The (uninitialized) vectorized environments.
      )pbdoc";

  py::class_<py_vec_env<N>>(m, name.c_str(), doc.c_str())
      .def("reset", py::overload_cast<>(&py_vec_env<N>::reset), R"pbdoc(
           Reset all environments, not modifying current parameters

           If no environment parameters have been previously set,
           the default parameters from when the instance was constructed
           are used.

           :return: None
      )pbdoc")
      .def("reset",
           py::overload_cast<uint32_t, u_char, u_char, Difficulty, unsigned int,
                             bool>(&py_vec_env<N>::reset),
           py::arg("seed"), py::arg("n_players"), py::arg("n_pieces"),
           py::arg("difficulty"), py::arg("max_steps"), py::arg("render"),
           R"pbdoc(
           Reset all environments using provided parameters

           :param seed: Set the rng seed
           :type seed: uint32_t
           :param n_players: The number of players, with the maximum of 4
           :type n_players: unsigned char
           :param n_pieces: Number of map pieces to be used between the starting piece and the end piece when generating the map for the game
           :type n_pieces: unsigned char
           :param difficulty: difficulty setting controlling which map pieces are allowed in map generation
           :type difficulty: :py:class:`city_of_gold.Difficulty`
           :param max_steps: Number of steps before forcing game end
           :type max_steps: unsigned int
           :param render: Set to true to render the game in the environments
           :type render: bool
           :return: None

           )pbdoc")
      .def("step", &py_vec_env<N>::step, py::arg("actions"),
           R"pbdoc(
           Advance the environment state according to chosen actions

           :param actions: Actions of the currently active agent in each environment
           :type actions: :py:class:`numpy.ndarray` of :py:class:`~city_of_gold.ActionData`
           :return: None

           )pbdoc")

      .def_property_readonly(
          "observations", &py_vec_env<N>::observe,
          py::return_value_policy::reference_internal,
          ":py:class:`numpy.ndarray` of :py:class:`~city_of_gold.ObsData`: "
          "Observations of currently active agents")

      .def_property_readonly("num_envs", &py_vec_env<N>::get_num_envs,
                             "int: Number of environments")
      .def_property_readonly("agent_selection",
                             &py_vec_env<N>::get_agent_selection,
                             py::return_value_policy::reference_internal,
                             ":py:class:`numpy.ndarray[unsigned char]`: "
                             "Current active player in each environment")
      .def_property_readonly(
          "selected_action_masks", &py_vec_env<N>::get_selected_action_masks,
          py::return_value_policy::reference_internal,
          ":py:class:`numpy.ndarray` of :py:class:`~city_of_gold.ActionMask`: "
          "Action masks of current active agents")
      .def_property_readonly("dones", &py_vec_env<N>::get_dones,
                             py::return_value_policy::reference_internal,
                             ":py:class:`numpy.ndarray[bool]`: "
                             "Flag specifying environments with ended games "
                             "after the previous step")
      .def_property_readonly("rewards", &py_vec_env<N>::get_rewards,
                             py::return_value_policy::reference_internal,
                             "2D :py:class:`numpy.ndarray[float]`: "
                             "Action masks of currently active agents")
      .def_property_readonly(
          "infos", &py_vec_env<N>::get_infos,
          py::return_value_policy::reference_internal,
          "2D :py:class:`numpy.ndarray` of :py:class:`~city_of_gold.Info`: "
          "Episode infos of all agents");
}

template <size_t N> void bind_vec_sampler(py::module_ &m) {
  std::string num = std::to_string(N);
  std::string name = std::string(vec_sampler_cls) + num;
  std::string doc = {
      "Vectorized random agent for " + num +
      " environments\n\n"
      ":param seed: (Optional) Set the random generator seed for the sampler. "
      "Unique seeds in the form seed+i are used to initialize the individual "
      "samplers, with i being the index of the sampler.\n"
      ":type seed: unsigned integer or None\n"
      ":return: The instantiated vector of action samplers"};

  py::class_<py_vec_action_sampler<N>>(m, name.c_str(), doc.c_str())
      .def("get_actions", &py_vec_action_sampler<N>::get_actions,
           py::return_value_policy::reference_internal,
           "Get a reference to the samplers internal vector of sampled "
           "actions.\n\n"
           "The contents are overwritten every time :meth:`sample` is called.\n"
           ":returns: The vector where sampled actions are placed\n"
           ":rtype: :py:class:`numpy.ndarray` of "
           ":py:class:`~city_of_gold.ActionData`")
      .def("sample", &py_vec_action_sampler<N>::sample, py::arg("action_mask"),
           "Generate a uniform sample of the valid action space for each "
           "environment.\n\n"
           "Update the contents of actions with a new sample masked using the "
           "input action mask.\n"
           ":param action_mask: mask specifying valid actions for each "
           "environment\n"
           ":type action_mask: :py:class:`numpy.ndarray` of "
           ":py:class:`~city_of_gold.ActionMask`\n"
           ":return: None");
}

template <size_t N> void bind_runner(py::module_ &m, py::module_ &m_base) {
  std::string parent_m_name = py::cast<std::string>(m_base.attr("__name__"));
  std::string::size_type pos = parent_m_name.find('.');
  assert(pos != std::string::npos);
  std::string base_name = parent_m_name.substr(0, pos);
  std::string module_name = py::cast<std::string>(m.attr("__name__"));
  std::string num = std::to_string(N);
  std::string name = std::string(vec_runner_cls) + num;
  std::string env_name = std::string(vec_env_cls) + num;
  std::string sampler_name = std::string(vec_sampler_cls) + num;
  std::string doc = "Threaded game runner for " + num + R"pbdoc( environments.

      Provides threaded wrappers to stepping environments in parallel,
      and to sampling random actions for benchmarking performance.
      Spawns threads that each are responsible for processing a continuous
      batch of environments.

      :param env: The vector of environments to run
      :type env: :py:class:`~)pbdoc" +
                    parent_m_name + ".env." + env_name + R"pbdoc(`
      :param sampler: The vector of environments to run
      :type sampler: :py:class:`~)pbdoc" +
                    parent_m_name + ".sampler." + sampler_name + R"pbdoc(`
      :param n_threads: Number of worker threads to spawn
      :type n_threads: unsigned int

      :return: The runner instance.

      )pbdoc";
  std::string get_envs_doc = {
      "Get a mutable reference to the underlying array of environments.\n\n"
      ":return: Reference to the environments managed by this object\n"
      ":rtype: :py:class:`~" +
      parent_m_name + ".env." + env_name + "`"};
  std::string get_n_threads_doc = {
      "Get the number of workers.\n\n"
      ":return: Number of workers spawned by this object\n"
      ":rtype: unsigned int"};
  std::string get_samplers_doc = {
      "Get a mutable reference to the underlying array of action "
      "samplers.\n\n"
      ":return: Reference to the samplers managed by this object\n"
      ":rtype: :py:class:`~" +
      parent_m_name + ".sampler." + sampler_name + "`"};
  std::string make_samplers_doc = {"Create action samplers.\n\n"
                                   ":param seed: Random seed\n"
                                   ":type seed: unsigned integer\n"
                                   ":return: None"};
  std::string get_actions_doc = {
      "Get a mutable reference to the underlying array of actions.\n\n"
      "Custom agents need to write to this array for their actions to "
      "be processed when advancing the environments via the :meth:`step` "
      "function.\n\n"
      ":return: Mutable reference to the action array read from when stepping "
      "the environments and written to when sampling actions\n"
      ":rtype: :py:class:`numpy.ndarray` of :py:class:`~" +
      base_name + ".ActionData`"};

  // TODO: make this true, currently the action mask is returned as mutable
  std::string get_am_doc = {
      "Get an immutable reference to action masks of currently active "
      "players\n\n"
      ":return: The action masks of currently active players\n"
      ":rtype: :py:class:`numpy.ndarray` of :py:class:`~" +
      base_name + ".ActionMask`"};
  std::string step_doc = {
      "Advance the environment state according to set actions\n\n"

      "Contrary to the sequential :meth:`step_seq`, stepping is performed in "
      "parallel. Each worker thread processes :math:`x` consecutive "
      "environments, where\n\n.. math::\n\n"
      "    \\frac{n_{\\text{envs}}}{n_{\\text{threads}}} < x < "
      "\\frac{n_{\\text{envs}}}{n_{\\text{threads}}} + 1\n\n"
      "Synchronizes environments with the main thread before returning. "
      "Each worker is guaranteed to always step and sample actions of the same "
      "environments.\n\n"
      ":return: None"};
  std::string step_seq_doc = {
      "Sequentially advance the environment states according to set actions\n\n"
      ":return: None"};
  std::string sync_doc = {"Blocks the main thread until all workers have "
                          "finished all queued tasks.\n\n"
                          ":return: None"};
  std::string sleep_doc = {"Releases worker threads from busy waiting.\n\n"
                           ":return: None"};
  std::string wake_doc = {
      "Wakes up all worker threads to poll for new tasks.\n\n"
      ":return: None"};
  std::string sample_doc = {
      "Generate a uniform sample of the valid action space for each "
      "environment.\n\n"
      "Update the contents of actions with a new sample masked using the "
      "current action masks of the managed environments."
      "Contrary to the sequential :meth:`sample_seq`, sampling is performed in "
      "parallel. The indices sampled by each thread are guaranteed to match "
      "the environments processed by the thread when :meth:`step` is "
      "called. No thread synchronization is applied automatically.\n\n"
      ":return: None"};
  std::string sample_seq_doc = {
      "Generate a uniform sample of the valid action space for each "
      "environment.\n\n"
      "Update the contents of actions with a new sample masked using the "
      "current action masks of the managed environments.\n\n"
      ":return: None"};
  std::string start_doc = {"Start worker threads for parallel execution\n\n"
                           ":return: None"};

  py::class_<py_threaded_runner<N>>(m, name.c_str(), doc.c_str())
      .def(py::init([](std::optional<size_t> n_threads) {
             return std::make_unique<py_threaded_runner<N>>(n_threads);
           }),
           py::arg("n_threads") = py::none(), "Initialize the runner")
      .def("get_envs", &py_threaded_runner<N>::get_envs,
           py::return_value_policy::reference, get_envs_doc.c_str())
      .def("get_n_threads", &py_threaded_runner<N>::get_n_threads,
           get_n_threads_doc.c_str())
      .def("get_samplers", &py_threaded_runner<N>::get_samplers,
           py::return_value_policy::reference, get_samplers_doc.c_str())
      .def("make_samplers", &py_threaded_runner<N>::make_samplers,
           py::arg("seed"), get_samplers_doc.c_str())
      .def("get_actions", &py_threaded_runner<N>::get_actions,
           py::return_value_policy::reference, get_actions_doc.c_str())
      .def("get_action_masks", &py_threaded_runner<N>::get_action_masks,
           py::return_value_policy::reference, get_am_doc.c_str())
      .def("step", &py_threaded_runner<N>::step, step_doc.c_str())
      .def("step_seq", &py_threaded_runner<N>::step_seq, step_seq_doc.c_str())
      .def("sync", &py_threaded_runner<N>::sync, sync_doc.c_str())
      .def("sleep", &py_threaded_runner<N>::sleep, sleep_doc.c_str())
      .def("wake", &py_threaded_runner<N>::wake, wake_doc.c_str())
      .def("sample", &py_threaded_runner<N>::sample, sample_doc.c_str())
      .def("sample_seq", &py_threaded_runner<N>::sample_seq,
           sample_seq_doc.c_str())
      .def("start_workers", &py_threaded_runner<N>::start_workers,
           start_doc.c_str());
}

void bind_vec_getters(py::module_ &m_parent, py::module_ &m_envs,
                      py::module_ &m_samplers, py::module_ &m_runners);

template <size_t N, size_t i = 1>
void bind_internal(py::module_ &m_parent, py::module_ &m_e, py::module_ &m_s,
                   py::module_ &m_r) {
  bind_vec_env<i>(m_e);
  bind_vec_sampler<i>(m_s);
  bind_runner<i>(m_r, m_parent);
  if constexpr (i >= N)
    return;
  else if constexpr (i >= 8)
    return bind_internal<N, i * 2>(m_parent, m_e, m_s, m_r);
  else
    return bind_internal<N, i + 1>(m_parent, m_e, m_s, m_r);
}

template <size_t N> void bind_runners(py::module_ &m_parent) {
  auto m_env = m_parent.def_submodule("env", "Vectorized environments");
  auto m_sam = m_parent.def_submodule("sampler", "Vectorized action samplers");
  auto m_run = m_parent.def_submodule("runner", "Vectorized runners");
  bind_internal<N>(m_parent, m_env, m_sam, m_run);
  bind_vec_getters(m_parent, m_env, m_sam, m_run);
}
