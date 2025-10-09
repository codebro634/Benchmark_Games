import ctypes
import math
from collections import defaultdict
from ctypes import c_char_p, c_int, POINTER, CDLL, c_double, cast
import ctypes as ct
from pathlib import Path
import sys
import random
from typing import List,Tuple
import gymnasium as gym
import numpy as np

from gymnasium.envs.registration import register

class CPPLibrary:
    def __init__(
            self,
    ):
        base_dir = Path(__file__).resolve().parent
        lib_path = str(base_dir / Path("../../cmake-build-debug/libliblink.so")) if sys.platform != "win32" else str(base_dir / Path("../../cmake-build-debug/libliblink.dll"))
        self.fns = CDLL(lib_path)
        self._init_functions()

    def _init_functions(self):

        self.fns.get_outcomes.argtypes = [ct.c_void_p, ct.c_void_p, ct.c_int, ct.POINTER(ct.POINTER(ct.POINTER(ct.c_double))), ct.POINTER(ct.POINTER(ct.c_double)), ct.POINTER(ct.c_int)]
        self.fns.get_outcomes.restype = ct.POINTER(ct.c_void_p)
        self.fns.free_outcomes.argtypes = [ct.POINTER(ct.POINTER(ct.c_double)),ct.POINTER(ct.c_double),ct.c_int]
        self.fns.free_outcomes.restype = None

        self.fns.create_model.argtypes = [ct.POINTER(ct.c_char_p),ct.c_int]
        self.fns.create_model.restype = ct.c_void_p
        self.fns.create_rng.argtypes = [ct.c_int]
        self.fns.create_rng.restype = ct.c_void_p

        self.fns.close_model.argtypes = [ct.c_void_p]
        self.fns.close_model.restype = None
        self.fns.close_state.argtypes = [ct.c_void_p]
        self.fns.close_state.restype = None
        self.fns.close_rng.argtypes = [ct.c_void_p]
        self.fns.close_rng.restype = None

        # model_get_initial_state: takes a model pointer, returns a pointer to Gamestate.
        self.fns.get_initial_state.argtypes = [ct.c_void_p, ct.c_void_p]
        self.fns.get_initial_state.restype = ct.c_void_p

        # model_print_state: takes a model pointer and a gamestate pointer, returns nothing.
        self.fns.print_state.argtypes = [ct.c_void_p, ct.c_void_p]
        self.fns.print_state.restype = None

        # model_copy_state: takes a model pointer and a gamestate pointer, returns a new gamestate pointer.
        self.fns.copy_state.argtypes = [ct.c_void_p, ct.c_void_p]
        self.fns.copy_state.restype = ct.c_void_p

        # model_apply_action: takes a model pointer, a gamestate pointer, and an int (action)
        self.fns.apply_action.argtypes = [ct.c_void_p, ct.c_void_p, ct.POINTER(ct.c_int), ct.POINTER(ct.c_double), ct.POINTER(ct.c_double), ct.c_void_p, ct.c_bool]
        self.fns.apply_action.restype = None

        self.fns.get_available_actions.argtypes = [ct.c_void_p, ct.c_void_p, ct.POINTER(ct.c_int), ct.POINTER(ct.c_int), ct.c_bool]
        self.fns.get_available_actions.restype = ct.POINTER(ct.c_int)
        self.fns.free_action_ptr.argtypes = [ct.POINTER(ct.c_int)]
        self.fns.free_action_ptr.restype = None

        self.fns.is_action_legal.argtypes = [ct.c_void_p, ct.c_void_p, ct.POINTER(ct.c_int)]
        self.fns.is_action_legal.restype = ct.c_bool

        self.fns.get_obs_shape.argtypes = [ct.c_void_p, ct.POINTER(ct.c_int), ct.POINTER(ct.c_bool)]
        self.fns.get_obs_shape.restype = ct.POINTER(ct.c_int)
        self.fns.free_obs_shape.argtypes = [ct.POINTER(ct.c_int)]
        self.fns.free_obs_shape.restype = None

        self.fns.get_action_shape.argtypes = [ct.c_void_p,  ct.POINTER(ct.c_int)]
        self.fns.get_action_shape.restype = ct.POINTER(ct.c_int)
        self.fns.free_action_shape.argtypes = [ct.POINTER(ct.c_int)]
        self.fns.free_action_shape.restype = None

        self.fns.get_obs.argtypes = [ct.c_void_p, ct.c_void_p ,ct.POINTER(ct.c_int)]
        self.fns.get_obs.restype = None

        self.fns.is_terminal.argtypes = [ct.c_void_p]
        self.fns.is_terminal.restype = ct.c_bool

        self.fns.equality.argtypes = [ct.c_void_p, ct.c_void_p]
        self.fns.equality.restype = ct.c_bool

        self.fns.hash.argtypes = [ct.c_void_p]
        self.fns.hash.restype = ct.c_int

        self.fns.num_players.argtypes = [ct.c_void_p]
        self.fns.num_players.restype = ct.c_int

        self.fns.player_at_turn.argtypes = [ct.c_void_p]
        self.fns.player_at_turn.restype = ct.c_int

        self.fns.idx_to_multi_discrete.argtypes = [ct.c_void_p, ct.c_int, ct.POINTER(ct.c_int)]
        self.fns.idx_to_multi_discrete.restype = None

class GameState(gym.Env):

    lib = CPPLibrary()
    global_model_id = -1
    id_counts = defaultdict(int)

    def __init__(
            self,

            #Copy constructor
            to_copy_state: "GameState" = None,
            state_ptr = None,

            #Factory constructor
            model_name: str = None,
            model_args: dict = None,
            horizon: int = None,
            seed:int = None, #if not specified a random seed is chosen
            flatten_obs: bool = None,
    ):
        super().__init__()

        assert not (model_name is not None and to_copy_state is not None), "Cannot both call the copy constructor and the factory constructor."

        """ Factory constructor """
        gym_interface_available = True
        if model_name is not None:
            arg_strings = [model_name] + [f"{k}={v}" for k, v in model_args.items()]
            args_array_type = c_char_p * len(arg_strings)
            c_strings = args_array_type(*[s.encode('utf-8') if s is not None else None for s in arg_strings])
            try:
                model_ptr = GameState.lib.fns.create_model(c_strings, len(arg_strings), horizon)
            except:
                print("Model creation unsuccessful. Exiting.")
                exit(1)

            rng_seed = seed if seed is not None else random.randint(-(10**9),10**9)
            rng_ptr = GameState.lib.fns.create_rng(rng_seed)

            GameState.global_model_id+=1
            id = GameState.global_model_id

            #Set gym spaces
            obs_shape_size = c_int(0)
            obs_implemented = ctypes.c_bool(False)
            obs_shape = GameState.lib.fns.get_obs_shape(model_ptr,ct.byref(obs_shape_size),ct.byref(obs_implemented))

            if obs_implemented:
                obs_tuple = tuple([obs_shape[i] for i in range(obs_shape_size.value)])
                GameState.lib.fns.free_obs_shape(obs_shape)
                prod = 1
                for dim in obs_tuple:
                    prod *= dim
                observation_space = gym.spaces.Box(low=-math.inf, high=math.inf, shape=(prod,) if flatten_obs else obs_tuple, dtype=np.float32)

                action_shape_size = c_int()
                action_shape = GameState.lib.fns.get_action_shape(model_ptr,ct.byref(action_shape_size))
                action_list = [action_shape[i] for i in range(action_shape_size.value)]
                GameState.lib.fns.free_action_shape(action_shape)
                action_space = gym.spaces.MultiDiscrete(action_list)
            else:
                action_space, observation_space = None, None
                gym_interface_available = False


        """ Copy constructor """
        self.inited = False if to_copy_state is None else to_copy_state.inited
        self.id = to_copy_state.id if to_copy_state is not None else id
        GameState.id_counts[self.id]+=1
        self.model_ptr = to_copy_state.model_ptr if to_copy_state is not None else model_ptr
        self.rng_ptr = to_copy_state.rng_ptr if to_copy_state is not None else rng_ptr
        self.state_ptr = state_ptr
        self.num_players = GameState.lib.fns.num_players(self.model_ptr)
        self.observation_space = to_copy_state.observation_space if to_copy_state is not None else observation_space
        self.action_space = to_copy_state.action_space if to_copy_state is not None else action_space
        self.gym_interface_available = to_copy_state.gym_interface_available if to_copy_state is not None else gym_interface_available
        assert flatten_obs is not None or to_copy_state is not None
        self.flatten_obs = to_copy_state.flatten_obs if to_copy_state is not None else flatten_obs

    @staticmethod
    def create_rng(seed):
        return GameState.lib.fns.create_rng(seed)

    @staticmethod
    def close_rng(rng_ptr):
        GameState.lib.fns.close_rng(rng_ptr)

    """
        Returns the current state of the gamestate as a numpy array.
    """
    def getObservation(self) -> np.ndarray:
        assert self.inited, "Gamestate has to be inited before calling getObservation."
        assert self.gym_interface_available, "Gym interface is not available for this model."
        obs_arr = np.zeros(self.observation_space.shape, dtype=np.int32)
        obs_p = obs_arr.ctypes.data_as(ctypes.POINTER(ctypes.c_int32))
        GameState.lib.fns.get_obs(self.model_ptr,self.state_ptr,obs_p)
        raw_obs = obs_arr.astype(np.float32)
        if self.flatten_obs:
            return raw_obs.flatten()
        else:
            return raw_obs

    """
        action: An element from self.action_space
        returns: A tuple of (observation, reward, done, info).

        reward: is the reward for the player at turn.
        info: contains the reward for all players, the probability of this transition (if available), and whether the action was illegal.
        If the action was illegal the underlying state remains unchanged.
    """
    def step(self, action, rng = None) -> Tuple[np.ndarray, float, bool,bool, dict]:
        assert self.inited and (isinstance(action, tuple) or isinstance(action, np.ndarray)), "Action must be an tuple or numpy array."
        assert self.gym_interface_available, "Gym interface is not available for this model. Call applyAction instead."

        if isinstance(action, np.ndarray):
            action = tuple(action.tolist())

        action_ptr = cast((c_int * len(action))(*action), POINTER(c_int))
        if not GameState.lib.fns.is_action_legal(self.model_ptr, self.state_ptr, action_ptr):
            return np.zeros(self.observation_space.shape, dtype=self.observation_space.dtype), 0 , False, False, {"illegal": True}

        rewards, prob = self.applyAction(action, rng = rng)
        info = {
            "reward": rewards,
            "prob": prob,
            "illegal": False
        }
        terminal = self.isTerminal()
        return self.getObservation(), rewards[self.playerAtTurn()], terminal, terminal, info

    """
        Samples a random successor of this state-action pair. This operation modifies the gamestate object and
        returns a reward vector (a reward of for each player) and the probability of this transition.
    """
    def applyAction(self, action: np.ndarray, rng = None) -> Tuple[List[float],float]:
        assert self.inited
        prob_ptr = c_double()
        reward_array = (c_double * self.numPlayers())()
        reward_ptr = cast(reward_array, POINTER(c_double))
        action_array = (c_int * len(action))(*action)
        action_ptr = cast(action_array, POINTER(c_int))
        GameState.lib.fns.apply_action(self.model_ptr,self.state_ptr,action_ptr, reward_ptr,ct.byref(prob_ptr), rng if rng is not None else self.rng_ptr, ct.c_bool(self.gym_interface_available))
        return [reward_array[i] for i in range(self.numPlayers())],prob_ptr.value

    """
        Frees the memory this gamestate allocated.
    """
    def close(self):
        if self.inited:
            GameState.lib.fns.close_state(self.state_ptr)

        GameState.id_counts[self.id]-=1
        if GameState.id_counts[self.id]==0:
            GameState.lib.fns.close_model(self.model_ptr)
            GameState.lib.fns.close_rng(self.rng_ptr)
            del GameState.id_counts[self.id]

    """
        Resets the gamestate to a randomly sampled initial state.
        Reset has to be called at least once before any operations can be performed on this gamestate.

        Returns the initial observation
    """
    def reset(self, seed = None, rng = None, options = None) -> Tuple[np.ndarray, dict]:
        assert seed is None, "Seed is not supported for this interface. Use the rng parameter instead."

        if self.inited:
            GameState.lib.fns.close_state(self.state_ptr)
            
        self.state_ptr = GameState.lib.fns.get_initial_state(self.model_ptr,rng if rng is not None else self.rng_ptr)
        self.inited = True
        return self.getObservation() if self.gym_interface_available else None, {}

    """
        Prints the current gamestate.
    """
    def render(self):
        assert self.inited
        GameState.lib.fns.print_state(self.model_ptr,self.state_ptr)

    """
        Creates a deepcopy of this gamestate.
    """
    def copy(self) -> "GameState":
        assert self.inited
        state_ptr = GameState.lib.fns.copy_state(self.state_ptr)
        return GameState(to_copy_state=self,state_ptr=state_ptr)

    """
        Tests for equality with another gamestate.
    """
    def __eq__(self, game: "GameState") -> bool:
        assert self.inited
        return GameState.lib.fns.equality(self.state_ptr,game.state_ptr)

    """
        Hashes this gamestate.
    """
    def __hash__(self):
        assert self.inited
        return GameState.lib.fns.hash(self.state_ptr)

    """
        Returns a list of all legal actions in this gamestate as a list of integers.
    """
    def getActions(self) -> List[tuple]:
        assert self.inited
        num_actions = c_int()
        action_dim = c_int()
        actions = GameState.lib.fns.get_available_actions(self.model_ptr,self.state_ptr,ct.byref(num_actions),ct.byref(action_dim),ct.c_bool(self.gym_interface_available))
        result = [ tuple([actions[i * action_dim.value + j] for j in range(action_dim.value)]) for i in range(num_actions.value)]
        GameState.lib.fns.free_action_ptr(actions,num_actions)
        return result

    """
        Returns the number of the player currently at turn.
    """
    def playerAtTurn(self) -> int:
        assert self.inited
        return GameState.lib.fns.player_at_turn(self.state_ptr)

    """
        Returns the number of players for the model.
    """
    def numPlayers(self) -> int:
        assert self.inited
        return self.num_players

    """
        Returns whether the current state is a terminal state.
    """
    def isTerminal(self) -> bool:
        assert self.inited
        return GameState.lib.fns.is_terminal(self.state_ptr)

    """
        An iterator over all possible successors of a state-action pair.
        Each call returns a Gamestate,Reward,Probabiliy triplet.
    """
    def iterateOutcomes(self,action:int):
        assert self.inited

        size = ct.c_int()
        reward_ptr = POINTER(POINTER(c_double))()
        probs_ptr = POINTER(c_double)()
        outcomes_ptr = GameState.lib.fns.get_outcomes(self.model_ptr,self.state_ptr,action, ct.byref(reward_ptr), ct.byref(probs_ptr), ct.byref(size))

        try:
            for i in range(size.value):
                rewards = [reward_ptr[i][p] for p in range(self.numPlayers())]
                yield GameState(to_copy_state=self, state_ptr=outcomes_ptr[i]), rewards, probs_ptr[i]
        finally:
            GameState.lib.fns.free_outcomes(reward_ptr,probs_ptr,size)

    def index_to_multi_discrete(self, idx: int):
        dims = len(self.action_space.nvec)
        action_array = (c_int * dims)()
        action_ptr = cast(action_array, POINTER(c_int))
        GameState.lib.fns.idx_to_multi_discrete(self.model_ptr, idx, action_ptr)
        return tuple([action_array[i] for i in range(dims)])

if 'stochastic_game' not in gym.envs.registry:
    register(
        id='stochastic_game',  # unique name for the environment
        entry_point=GameState,  # where the class is located
    )

if __name__ == "__main__":
    game = gym.make("stochastic_game",model_name="Navigation",model_args={"map":"1_IPPC.txt"}, horizon=50, flatten_obs = False).unwrapped
    #game = GameState(model_name="ktk", model_args={"map":"standard.txt", "zero_sum": 1}, horizon=50)

    game.reset()
    #print(game.observation_space.shape)
    game.render()
    print(game.getActions())
    game.applyAction(game.getActions()[1])
    game.render()
    # print(game.getObservation())
    # print("-------")
    print(game.step((0,)))
    game.close()
