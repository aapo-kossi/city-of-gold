import time
import multiprocessing as mp
import numpy as np
from scipy.stats import linregress
import matplotlib.pyplot as plt
import city_of_gold
from city_of_gold import vec


def fit_linear_regression(times, sizes):
    flat_times = times.flatten()
    flat_sizes = (sizes[:, np.newaxis] + 0 * times).flatten()
    slope, intercept, _, _, _ = linregress(flat_sizes, flat_times)
    return slope, intercept


def visualize_comparisons(results, sizes, steps):
    # Plotting the original times vs. environments
    plt.figure(figsize=(12, 6))
    overhead = np.empty(len(results), dtype=float)
    scaling = np.empty(len(results), dtype=float)
    labels = []
    for i, (times, label) in enumerate(results):
        slope, intercept = fit_linear_regression(times, sizes)
        overhead[i] = intercept
        scaling[i] = slope
        mean_times = times.mean(axis=1)

        cmap = plt.cm.Blues  # Continuous colormap for async modes
        sync_cmap = plt.cm.Blues  # Continuous colormap for async modes
        # Use continuous colormap for async and sync
        if label == "sequential":
            color = "k"
        elif "threads" in label:
            n_threads = int(label.split()[0])  # Get thread count
            color = cmap(n_threads / mp.cpu_count())  # Map to colormap
        elif "sync" in label:
            n_threads = int(label.split()[0])  # Get thread count
            color = sync_cmap(n_threads / mp.cpu_count())  # Map to colormap
        elif label == "std parallel":
            color = "g"
        else:
            raise Exception("Unknown label")
        labels.append(label)

        # Scatter plot of the data with smaller markers and color consistency
        plt.subplot(1, 3, 1)
        plt.scatter(sizes, mean_times, color=color, s=40)  # Reduced marker size
        plt.plot(sizes, intercept + slope * sizes, color=color, label=label)
        plt.xscale("log")  # Log scale for x-axis
        plt.yscale("log")  # Log scale for y-axis
        plt.xlabel("Environment count [a.u.]")
        plt.ylabel("Time [s]")
        plt.title(f"Scaling behaviour of vectorized environments")
        plt.legend()

    # Plotting the overhead and scaling on separate axes
    plt.subplot(1, 3, 2)

    plt.bar(range(len(overhead)), overhead, tick_label=labels)
    plt.xticks(rotation=90)
    plt.title("Execution overhead from linear fit time to execute 0 environments")
    plt.ylabel(f"Processing time for {steps//1000}k steps with 0 environments [s]")
    plt.xlabel("Execution method")

    base_scaling = scaling[next(i for i, l in enumerate(labels) if l == "sequential")]

    plt.subplot(1, 3, 3)
    plt.bar(range(len(scaling)), base_scaling / scaling, tick_label=labels)
    plt.title("Performance scaling relative to sequential")
    plt.ylabel("Steps per second relative to sequential [a.u.]")
    plt.xlabel("Execution method")
    plt.xticks(rotation=90)

    plt.tight_layout()
    plt.show()


def run_test(steps, n_envs, seed, threaded=False, threads=None):
    runner_cls = vec.get_runner(n_envs)
    runner = runner_cls(threads)
    runner.make_samplers(seed)
    envs = runner.get_envs()
    samplers = runner.get_samplers()
    if threaded:
        step_fun = runner.step
        sample_fun = runner.sample
        runner.start_workers()
    else:
        step_fun = runner.step_seq
        sample_fun = runner.sample_seq

    envs.reset(seed, 4, 3, city_of_gold.Difficulty.EASY, 100000, False)
    actions = samplers.get_actions()

    next_agents = np.expand_dims(
        envs.agent_selection, 1
    )  # reference, updates internally
    next_obs = envs.observations  # reference, updates internally
    am = next_obs["player_data"]["action_mask"]
    player_masks = envs.selected_action_masks
    current_rewards = envs.rewards  # reference, updates internally
    current_dones = envs.dones  # reference, updates internally
    current_infos = envs.infos  # reference, updates internally

    start = time.time()
    for i in range(steps):
        sample_fun()
        step_fun()
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
    seed = 0
    repeats = 5
    steps = 10_000
    sizes = np.array(
        [5, 6, 7, 8, 16, 32, 64, 128, 256, 128, 64, 32, 16, 8, 7, 6, 5, 4, 3, 2, 1]
    )

    # sizes, n_repeats, n_warmup, seed, threaded, n_threads, sync_threads
    opts = {
        "steps": steps,
        "sizes": sizes,
        "repeats": repeats,
        "seed": seed,
        "threaded": False,
        "threads": None,
    }

    results = []
    # warmup the cpu and cache
    time_tests(**opts)
    results.append((time_tests(**opts), "sequential"))

    n_cpu = mp.cpu_count()

    opts["threaded"] = True
    for n_threads in range(1, n_cpu):
        opts["threads"] = n_threads
        results.append((time_tests(**opts), f"{n_threads} threads"))

    visualize_comparisons(results, sizes, steps)


if __name__ == "__main__":
    main()
