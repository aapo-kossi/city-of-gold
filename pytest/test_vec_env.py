import time
import multiprocessing as mp
import numpy as np
import city_of_gold
from city_of_gold import vec

def run_test(steps, n_envs, seed, threaded=False, threads=None, sync=False):
    print(f"starting test for {steps} steps with {n_envs} envs, {seed} seed.")
    if threaded:
        print(
            f"Testing with multithreading on, using {threads} threads,"
            f"{'' if sync else 'not'} syncing envs between each step."
        )
    env_cls = vec.get_vec_env(n_envs)
    sampler_cls = vec.get_vec_sampler(n_envs)
    envs = env_cls()
    print("instantiated env")
    samplers = sampler_cls(seed)
    print("instantiated samplers")
    if threaded:
        runner_cls = vec.get_runner(n_envs)
        runner = runner_cls(envs, samplers, threads)
        if sync:
            step_fun = lambda _: runner.step_sync()
        else:
            step_fun = lambda _: runner.step()
        sample_fun = lambda _: runner.sample()
        envs = runner.get_envs()
        print("instantiated runner")
    else:
        step_fun = envs.step
        sample_fun = samplers.sample

    # sync parameter should not be set with sequential execution
    # as it only affects functionality with multithreading on
    assert not ((not threaded) and sync)

    envs.reset(seed, 4, 3, city_of_gold.Difficulty.EASY, 100000, False)
    print("reset envs")
    actions = samplers.get_actions()
    print("got actions")

    next_agents = np.expand_dims(envs.agent_selection, 1)  # reference, updates internally
    next_obs = envs.observations  # reference, updates internally
    am = next_obs["player_data"]["action_mask"]
    player_masks = envs.selected_action_masks
    current_rewards = envs.rewards  # reference, updates internally
    current_dones = envs.dones  # reference, updates internally
    current_infos = envs.infos  # reference, updates internally

    print("starting run")
    start = time.time()
    for i in range(steps):
        sample_fun(player_masks)
        step_fun(actions)
    if threaded:
        runner.sync()
    print("finished run\n")
    return time.time() - start

def time_tests(steps, sizes, repeats, seed, threaded, threads=None, sync=False):
    times = np.empty((len(sizes), repeats), dtype=float)
    for i, s in enumerate(sizes):
        print(f"Size {s}, seed {seed}:")
        for j in range(repeats):
            taken = run_test(steps, s, seed, threaded, threads, sync)
            times[i, j] = taken
            print(taken)
            seed += s
    return times

def main():
    test_sequential()
    test_async()
    test_sync()

# fuzzing the different execution methods with randomly sampled actions
def test_sequential():
    run_test(10000, 16, 123456)
    assert True

def test_async():
    run_test(10000, 16, 123456, True, 4)
    assert True

def test_sync():
    run_test(10000, 16, 123456, True, 4, True)
    assert True

if __name__ == "__main__":
    main()

