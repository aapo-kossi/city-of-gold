import numpy as np
import city_of_gold as cg

import pytest

def test_renderloop(capfd):

    try:
        env = cg.cog_env(123, 4, 4, cg.Difficulty.HARD, 100, True)
        sampler = cg.action_sampler(987)
        obs = cg.ObsData()
        act = cg.ActionData()
        mask = cg.ActionMask()
        info = cg.Info()
        rew = np.zeros(4)
        env.init(obs, info, rew, mask)
        env.reset()
        env.render()
        step = 0
        while (step != 1000):
            if env.get_done():
                env.reset()
            act = sampler.sample(mask)
            env.step(act)
            env.render()
            step += 1

        assert True
    except RuntimeError as e:
        cap = capfd.readouterr()
        err = cap.err
        out = cap.out
        if any("No available video device" in x for x in [out, err]):
            pytest.xfail(reason=err)
        raise e
    return


