from asv_runner.benchmarks.mark import SkipNotImplemented
from city_of_gold import vec
import city_of_gold

N_STEPS = 10_000

class TimeEnvs:
    timeout = 300
    params = (
        [1,2,3,4,5,6,7,8,16,32,64,128,256],
        [12345],
        [1,2,3,4,5],
        ["sequential", "threaded"],
    )

    def setup(self, n, seed, threads, mode):
        if ((mode == "sequential") and (threads > 1)): raise NotImplementedError()
        self.threaded = mode != "sequential"
        runner = vec.get_runner(n)(threads)
        runner.make_samplers(seed)
        envs = runner.get_envs()
        envs.reset(seed, 4, 3, city_of_gold.Difficulty.EASY, 100000, False)
        self.am = envs.selected_action_masks
        if self.threaded:
            self.sample = runner.sample
            self.step_func = runner.step
            self.sync_fun = runner.sync
            runner.start_workers()
        else:
            self.step_func = runner.step_seq
            self.sample = runner.sample_seq
            self.sync_fun = lambda: None
        self.reset = envs.reset

    def time_run(self, *_):
        for _ in range(N_STEPS):
            self.sample()
            self.step_func()
        self.sync_fun()

    def time_sample(self, *_):
        for _ in range(N_STEPS):
            self.sample()
            self.sync_fun()
        self.sync_fun()

    def time_reset(self, *_):
        if self.threaded:
            raise SkipNotImplemented
        for _ in range(N_STEPS//10):
            self.reset()

    def peakmem_runner(self, *_):
        self.sample()
        self.step_func()
        self.sync_fun()

