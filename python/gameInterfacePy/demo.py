import random
import game
import gym

#Create a Sailing Wind instance of size 15, a horizon length of 15 and an optional seeding of 42
game = gym.make("stochastic_game",model_name="Game Of Life", model_args={"map":"./resources/GameOfLifeMaps/1_IPPC.txt"}, horizon=50)

for _ in range(10):
    episode_return = 0

    #By calling reset, an initial state is sampled
    game.reset()

    while True:
        #Optionally, we print the gamestate at each step
        #game.printState()

        #Check for terminality
        if game.isTerminal():
            break

        # Apply action modifies the gamestate, by sampling a successor state and returning
        # the reward vector (one entry for each player, here just 1 entry) and the probability of this transition
        rews, prob = game.applyAction(random.choice(game.getActions()))

        episode_return += rews[0]

    print(f"Episode return: {episode_return}")

#Free up any allocated memory
game.close()