#include "pybind/vectorized.h"
#include <string_view>

std::string_view vec_env_cls = "vec_cog_env_";
std::string_view vec_sampler_cls = "vec_sampler_";
std::string_view vec_runner_cls = "vec_runner_";

void bind_vec_getters(py::module_ &m_parent, py::module_ &m_envs,
                      py::module_ &m_samplers, py::module_ &m_runners) {
  m_parent.def(
      "get_runner",
      [m_runners](size_t N) -> py::object {
        std::string name = std::string(vec_runner_cls) + std::to_string(N);
        return m_runners.attr(name.c_str());
      },
      py::arg("N"),
      (R"pbdoc(
       Get the threaded runner class for N environments

       This is a helper utility for instantiating differently
       sized environment runners at runtime, as each number of
       environments corresponds to a specialization of a class
       template internally.

       :param N: number of environments
       :type N: unsigned int
       :return: :py:class:`)pbdoc" +
       py::cast<std::string>(m_runners.attr("__name__")) + "." +
       std::string(vec_runner_cls) + "\\<N\\>` class object")
          .c_str());
  m_parent.def(
      "get_vec_env",
      [m_envs](size_t N) -> py::object {
        std::string name = std::string(vec_env_cls) + std::to_string(N);
        return m_envs.attr(name.c_str());
      },
      py::arg("N"),
      (R"pbdoc(
       Get the vectorized environment class for N environments

       This is a helper utility for instantiating differently
       sized environments at runtime, as each number of
       environments corresponds to a specialization of a class
       template internally.

       :param N: number of environments
       :type N: unsigned int
       :return: :py:class:`)pbdoc" +
       py::cast<std::string>(m_envs.attr("__name__")) + "." +
       std::string(vec_env_cls) + "\\<N\\>` class object")
          .c_str());
  m_parent.def(
      "get_vec_sampler",
      [m_samplers](size_t N) -> py::object {
        std::string name = std::string(vec_sampler_cls) + std::to_string(N);
        return m_samplers.attr(name.c_str());
      },
      py::arg("N"),
      (R"pbdoc(
       Get the vectorized random action sampler class for N environments

       This is a helper utility for instantiating differently
       sized environments at runtime, as each number of
       environments corresponds to a specialization of a class
       template internally.

       :param N: number of environments
       :type N: unsigned int
       :return: :py:class:`)pbdoc" +
       py::cast<std::string>(m_samplers.attr("__name__")) + "." +
       std::string(vec_sampler_cls) + "\\<N\\>` class object.\n")
          .c_str());
};
