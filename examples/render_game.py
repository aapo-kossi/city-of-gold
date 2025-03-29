import time
from argparse import ArgumentParser

import numpy as np

import city_of_gold as cg


def parse_args():

    p = ArgumentParser("Demonstrate city_of_gold environment rendering")
    p.add_argument("-s", "--seed", type=int, default=12345, help="Random seed")
    p.add_argument("-p", "--players", type=int, default=4, help="Number of players")
    p.add_argument("-n", "--pieces", type=int, default=3, help="Number of map pieces")
    p.add_argument("-d", "--difficulty", default="HARD", help="Game difficulty")
    p.add_argument(
        "-m",
        "--max-steps",
        type=int,
        default=1000,
        help="Maximum number of steps before game is automatically ended",
    )
    p.add_argument(
        "-t", "--delay", type=float, default=0.0, help="wait duration between steps"
    )
    p.add_argument(
        "-T",
        "--steps",
        type=int,
        default=-1,
        help="Number of steps, if negative run indefinitely",
    )
    return p.parse_args()


def main(args):

    difficulties = cg.Difficulty.__members__
    diff = difficulties[args.difficulty]
    env = cg.cog_env(args.seed, args.players, args.pieces, diff, args.max_steps, True)
    sampler = cg.action_sampler(args.seed)
    obs = cg.ObsData()
    act = cg.ActionData()
    mask = cg.ActionMask()
    info = cg.Info()
    rew = np.zeros(4)
    env.init(obs, info, rew, mask)
    env.reset()
    env.render()
    selected = env.agent_selection
    step = 0
    while (step != args.steps):
        if env.get_done():
            env.reset()
        # print(obs.shared.phase)
        # print(f"hand: {obs.player_data[env.agent_selection].obs.hand}")
        # print(f"mask: [{" ".join(str(int(b)) for b in mask.play)}]")
        act = sampler.sample(mask)
        env.step(act)
        # print(f"act:  [{act.play} {act.play_special} {act.remove} {act.move} {act.get_from_shop}]")
        env.render()
        # print("rendered")
        time.sleep(args.delay)
        step += 1

    return


if __name__ == "__main__":
    main(parse_args())
