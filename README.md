# Benchmark Games

## Purpose
Discrete planning research, such as improvements to Monte Carlo Tree Search, usually requires an algorithm's evaluation on a variety of test problems. Furthermore, during development, one might conduct test runs on a few environments but with several hundred or even thousand parameter combinations. This requires being able to efficiently query numerous test problem models.
To solve this issue, researchers either implement a few problems manually or use a slow simulator for an array of test problems that are written in a high-level descriptive language such as RDDL (by [Scott Sanner, 2011](https://users.cecs.anu.edu.au/~ssanner/IPPC_2011/RDDL.pdf))

To address this issue and to enable faster and more thorough research, we manually implemented 33 planning tasks that can be formalized as Stochastic Games (SG). These 33 tasks are composed of 22 single-agent planning tasks and 11 two-player games. We implemented each environment as efficiently as possible in C++, employing domain-specific optimisations such as bit manipulation when suitable. Since we aim to make this as accessible as possible, we fully avoided using any non-standard-library C++ packages, thus enabling a trivial setup of this project.

Each SG fulfils an interface which is described in more detail later that provides the essentials for planners

## Installation and Build

To build the project from source, you will need a C++ compiler supporting the C++20 standard or higher (a lower standard probably works too but we have not tested that). The project
is self-contained and does not require any additional installation.

To compile with [CMake](https://cmake.org/) you need to have CMake installed on your system. A `CMakeLists.txt` file is already provided for configuring the build.

**Steps:**

1. **Clone the repository:**
    ```bash
    git clone https://github.com/codebro634/Benchmark_Games.git
    cd Benchmark_Games
    ```

2. **Create a build directory (optional but recommended):**
    ```bash
    mkdir build
    cd build
    ```

3. **Generate build files using CMake:**
    ```bash
    cmake -DCMAKE_CXX_COMPILER=/path/to/your/c++-compiler -DCMAKE_C_COMPILER=/path/to/your/c-compiler ..
    ```

4. **Compile the project:**
    ```bash
    cmake --build .
    ```
   *This will invoke the underlying build system (e.g., `make` or `ninja`) to compile the source code.*

If no errors occur, two compiled binaries `BenchmarkGamesDebug` and `BenchmarkGamesRelease` should now be available in the `build` directory. The former has been compiled with debug
compiler flags and the latter with aggressive optimization. Additionally, this creates a shared library for the Python interface.

## Supported Planning Methods
Each environment implemented the `ABS::Model` (located in `Gamestate.h`) interface, which provides the essential methods for sampling an initial state, copying states, obtaining the set of legal actions for a state, applying an action to a state, iterating over all successors of a state-action pair, obtaining the sample probability for any state-action pair successor (except for Wildlife Preserve and Push Your Luck), hashing a state, or testing any two states for equality.

Each Gamestate implemented the `ABS::Gamestate` (located in `Gamestate.h`) interface, which provides the essential methods for obtaining the current player and checking for terminality.

## Example Usages

In the following examples, the will show how write and/or evaluate agents satisfying the `Agent` interface on any model that satisfies the `ABS::Model` interface.

### Manual evaluation of a random agent
Firstly, we will demonstrate in the following code snippet how to run a random agent for a single episode on a Game of Life instance and output the total reward obtained. 

```cpp
  //We require a seeded number generator for reproducibility
  std::mt19937 rng(static_cast<unsigned int>(42));

  //We create a Game of Life Model and wrap it in a finite horizon model thus ensuring that an episode ends after 50 steps
  auto base_model = new  GOL::Model("../resources/GameOfLifeMaps/3_Anand.txt", GOL::ActionMode::ALL);
  auto model = new FINITEH::Model(base_model, 50, true);

  //We will be using a random agent to play Game of Life
  auto agent = new RandomAgent();

  //Sample an initial state. For any Game of Life instance, there is only a single initial state
  auto state = model->getInitialState(rng);
  double reward_sum = 0;
  while (!state->terminal) {

      //Query agent for an action in the current state
      int action = agent->getAction(model, state, rng);

      //Apply action modifies 'state' and returns the reward and the probability of the sampled transition
      auto [rewards,prob] = model->applyAction(state, action, rng);

      //Since the ABS::Model interface supports multiplayer games, rewards is a vector. Since Game of Life is a single player game, rewards has size 1
      reward_sum += rewards[0];
  }
  std::cout << "Total reward: " << reward_sum << std::endl;

  //Cleanup pointers
  delete state;
  delete model;
  delete agent;
```

### Writing and evaluating a One-Step-Lookahead agent

Next, we will show to write a simple One-Step-Lookahead agent and evaluate it with a prewritten evaluation function. First, create the files
`OneStepLookaheadAgent.h` and `OneStepLookaheadAgent.cpp`. This agent inherits from `Agent` and needs to implement the `getAction` method. 

`OneStepLookaheadAgent.h`:
```cpp
#include "Agent.h"

namespace OSLA
{
    class OneStepLookaheadAgent: public Agent {

    public:
        int getAction(ABS::Model* model, ABS::Gamestate* gamestate, std::mt19937& rng) override;

    };

}
```

`OneStepLookaheadAgent.cpp`
```cpp
#include "../../include/Agents/OneStepLookahead.h"

using namespace OSLA;

int OneStepLookaheadAgent::getAction(ABS::Model* model, ABS::Gamestate* gamestate, std::mt19937& rng) {

    //We extract the player we are choosing an action for. This is needed to determine which reward we are maximizing
    int player = gamestate->turn;

    //Keep track of the best action so far
    int best_action = -1;
    double best_value = -std::numeric_limits<double>::infinity();

    //Loop over all legal actions in this state
    for (int action :  model->getActions(gamestate)) {

        //We copy the state as applying any action to the state will modify it
        auto copy = model->copyState(gamestate);

        //Apply the action to the copied state
        auto [rewards, prob] = model->applyAction(copy, action, rng);

        //To avoid memory leaks, close copy
        delete copy;

        //Trick to efficiently break ties
        rewards[player] += 1e-6 * std::uniform_real_distribution<double>(0, 1.0)(rng);

        //Update the best action if the reward is better
        if (rewards[player] > best_value) {
            best_value = rewards[player];
            best_action = action;
        }
    }

    return best_action;
}
```

Since we satisfy, the `Agent` interface, we can now evaluate this agent using the `playGames` function from `Arena.h`. 

```cpp
 //We require a seeded number generator for reproducibility
 std::mt19937 rng(static_cast<unsigned int>(42));

 //We create a Triangle Tireworld instance
 auto base_model = new TRT::Model("../resources/TriangleTireworlds/5_IPPC.txt",true,true,true);

 //We will be using a One-Step-Lookahead agent to play Triangle Tireworld
 auto agent = new OSLA::OneStepLookaheadAgent();

 /*
  * We evaluate the One-Step-Lookahead agent on 100 episodes with 50 steps each.
  * The agent is given a planning horizon of 20 steps, however, in this case the agent only requires only a planning horizon of 1 step.
  * Note that we did not the model in a finite horizon model. This is automatically done by playGames.
  */
 playGames(*base_model, 100, {agent}, rng, VERBOSE, {50,20});

 //Cleanup poiners
 delete base_model;
 delete agent;
```

## Preimplemented agents

The project comes with a few preimplemented C++ agents that can be used for evaluation. These agents are located in the `src/Agents` directory. The following agents are available:

1. **Random**: This agent selects a random action from the set of legal actions in the current state.
2. **Human**: This agent allows a human to play the game by inputting actions through the console.
3. **OneStepLookahead**: This agent selects the action that maximizes the reward in the next state. It is a simple one-step lookahead agent.
4. **Mcts**: This agent uses the UCT algorithm to select actions.
5. **ASAP**: This agent is an implementation of ASAP-UCT [Anand et al., 2015](https://www.ijcai.org/Proceedings/15/Papers/216.pdf)
6. **Parss**: This agent is an implementation of Progressive-Abstraction-Refinement-Sparse-Sampling by [Hostetler et al., 2017](https://web.engr.oregonstate.edu/~tgd/publications/hostetler-fern-dietterich-progressive-abstraction-refinement-for-sparse-sampling-uai2015.pdf).
7. **SparseSampling**: An implementation of the Sparse Sampling algorithm (i.e. MaxiMax in the case of no stochastic-branching)
8. **FFHindsightAgent**: An implementation of the FF-Hindsight algorithm by [Yoon et al., 2008](https://cdn.aaai.org/AAAI/2008/AAAI08-160.pdf).

## Preimplemented models

The project comes with a few preimplemented models that can be used for evaluation. These models are located in the `src/Models` directory. The following models are available:

### Single Player Games

1. **AcademicAdvising**
2. **CooperativeRecon**
3. **Crossing Traffic**
4. **Earth Observation**
5. **Elevators**
6. **Game of Life**
7. **JoinFive**
8. **Manufacturer**
9. **MultiArmedBandit**
10. **Navigation**
11. **PushYourLuck**
12. **RaceTrack**
13. **RedFinnedBlueEye**
14. **SailingWind**
15. **Saving**
16. **SkillsTeaching**
17. **SysAdmin**
18. **Tamarisk**
19. **ToySoccer**
20. **Triangle Tireworld**
21. **Wildfire**
22. **WildlifePreserve**

### Multiplayer Games

1. **CaptureTheFlag**
2. **Chess**
3. **Connect4**
4. **Constrictor**
5. **KillTheKing**
6. **NumbersRace**
7. **Othello**
8. **Pusher**
9. **Pylos**
10. **Quarto**
11. **TicTacToe**


## Python support

The project also comes with a Python interface that allows you to interact with the models and agents. The Python interface is built using `ctypes` and the shared C++ library first
which can be obtained by following the above-described installation steps. The following
demonstration shows how to use the Python interface to evaluate a random agent on a Sailing Wind instance.

```python
import random
import game

#Create a Game of Life instance with a horizon length of 50
game = game.GameState(model_name="Game Of Life", model_args={"map":"1_IPPC.txt"}, horizon=50)

#Alternatively using the gymnasium interface: gymnasium.make("stochastic_game", model_name="Game Of Life", model_args={"map":"1_IPPC.txt"}, horizon=50).unwrapped


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
```

The Python interfaces supports the same methods as the C++ `ABS::Model` (located in `Gamestate.h`) interface for model based search agents. The Python interface is located at  `python/gameInterfacePy/game.py`.

### Reinforcement learning support

For convenience and as hinted at in the above sample-code, our python-side interface implements
Farama's [Gymnasium](https://gymnasium.farama.org/) interface, thus enable a potential usage
for reinforcement learning. In particular, each model has implemented its own observation function.



