import os
import time

import pytest
import numpy as np
import city_of_gold
from city_of_gold import vec


def run_test(steps, n_envs, seed, threaded=False, threads=None):
    print(f"starting test for {steps} steps with {n_envs} envs, {seed} seed.")
    if threaded:
        print(f"Testing with multithreading on, using {threads} threads")
    runner_cls = vec.get_runner(n_envs)
    runner = runner_cls(threads)
    envs = runner.get_envs()
    runner.make_samplers(seed)
    samplers = runner.get_samplers()
    if threaded:
        step_fun = runner.step
        sample_fun = runner.sample
        runner.start_workers()
    else:
        step_fun = runner.step_seq
        sample_fun = runner.sample_seq

    envs.reset(seed, 4, 3, city_of_gold.Difficulty.EASY, 100000, False)
    print("reset envs")
    actions = samplers.get_actions()
    print("got actions")

    next_agents = np.expand_dims(
        envs.agent_selection, 1
    )  # reference, updates internally
    next_obs = envs.observations  # reference, updates internally
    am = next_obs["player_data"]["action_mask"]
    player_masks = envs.selected_action_masks
    current_rewards = envs.rewards  # reference, updates internally
    current_dones = envs.dones  # reference, updates internally
    current_infos = envs.infos  # reference, updates internally

    print("starting run")
    start = time.time()
    for i in range(steps):
        sample_fun()
        step_fun()
    if threaded:
        runner.sync()
    print("finished run\n")
    return time.time() - start


def time_tests(steps, sizes, repeats, seed, threaded, threads=None):
    times = np.empty((len(sizes), repeats), dtype=float)
    for i, s in enumerate(sizes):
        print(f"Size {s}, seed {seed}:")
        for j in range(repeats):
            taken = run_test(steps, s, seed, threaded, threads)
            times[i, j] = taken
            print(taken)
            seed += s
    return times


def main():
    test_import()
    run_test(10000, 16, 12345679)
    test_sequential()
    test_threaded()


def test_import():
    import city_of_gold

    difficulty = city_of_gold.Difficulty.HARD
    assert True


# fuzzing the different execution methods with randomly sampled actions
def test_sequential():
    run_test(10000, 16, 123456)
    assert True


def test_threaded():
    cores = os.cpu_count()
    if cores is not None and cores > 1:
        run_test(10000, 16, 123456, True, cores-1)
    else:
        pytest.skip("Cannot run multithreaded test on single core system!")
    assert True


if __name__ == "__main__":
    main()
