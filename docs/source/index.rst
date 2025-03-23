City of Gold documentation
==========================

Welcome to the documentation for the City of Gold reinforcement learning environment!

This package contains the implementation of a board game with a hexagonal
tile-based map where up to 4 players compete to reach the goal tiles before
their competitors. For training AI agents, the environment interface is designed
for multi-core parallelism using multithreading in C++ to run multiple independent
environments concurrently. Runner classes for different levels of parallelism
are available with :py:func:`city_of_gold.vec.get_runner`. The observation data
contains in contiguous memory the current states of each environment, including
an action mask to inform agents what actions are currently valid.

The data structures of the environments are exposed as memory views through
numpy_ structured arrays to prevent unnecessary copies and python function calls.
If using a memory buffer in a training algorithm, you need to copy the internal
data to a larger buffer. If using PyTorch_, the TensorDict_ module provides
fantastic tools to manage the heterogenous data structures that compose the
environment information, and can directly copy the structured arrays from the
cpu memory to VRAM.

See the :doc:`usage` section for :ref:`installation <installation>`
instructions and a :ref:`usage example <example>`, or find the
complete reference documentation in the :doc:`api`.


.. toctree::
    :maxdepth: 3
    :caption: Contents:

    usage
    api


.. _numpy: https://numpy.org/

.. _PyTorch: https://pytorch.org/

.. _TensorDict: https://pytorch.org/tensordict/stable/index.html



