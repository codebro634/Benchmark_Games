// //////////////////////////////////////////////////////////////////
// Squares are moved to adjacent empty cells until they are arranged
// in specific target positions.
//
// This is a c++ adaptation of
// 'https://github.com/pyrddlgym-project/rddlrepository/blob/93098ed2e8de6cf9302d378c5fdcf8d1cf832c34/rddlrepository/archive/arcade/Eight/domain.rddl'
// originally by author(s):
// 		Mike Gimelfarb (mgimelfarb@yahoo.ca)
// //////////////////////////////////////////////////////////////////

#include <bits/atomic_base.h>

#include "../../../include/Games/MDPs/EightPuzzle.h"
#include <iostream>
#include <cassert>
#include <complex>
#include <fstream>
#include <functional>
#include <ranges>
#include <algorithm>
using namespace std;

namespace EIGHT_PUZZLE {

    int inversion_count(const std::vector<char> &flattened_map) { // Even inversion count = valid state for the classic 3x3 puzzle.
        int res = 0;
        for (size_t i = 0; i < flattened_map.size(); i++) {
            if (flattened_map[i] == ' ') continue;
            for (size_t j = i + 1; j < flattened_map.size(); j++) {
                if (flattened_map[j] == ' ') continue;
                if (flattened_map[i] > flattened_map[j])
                    res++;
            }
        }
        return res;
    }
}


using namespace EIGHT_PUZZLE;


Model::Model(const std::string& fileName) {
    std::ifstream file(fileName);

    if (!file.is_open()){
        std::cerr << "Could not open file " << fileName << std::endl;
        exit(1);
    }

    std::vector<std::vector<char>> filled_lines;
    std::string line;

    height = -1;
    while (std::getline(file, line)) {
        if (not line.empty()) {
            filled_lines.emplace_back(line.begin(), line.end());
        } else if (height == -1ul and filled_lines.size() != 0) {
            height = filled_lines.size();
        }
    }
    if (height == -1ul) height = filled_lines.size();

    if constexpr (false) for (auto line_vec : filled_lines) {
        for (auto c : line_vec) {
            std::cerr << '\'' << c << '\'' << ' ';
        }
        std::cerr << std::endl;
    };


    assert (filled_lines.size() > 0 and filled_lines.size() % height == 0); // Non-zero number of lines divisible by height expected. Check for first empty line after goal state!
    width = 0;
    for (auto line_vec : filled_lines)
        width = max(width, line_vec.size());
    for (auto &line_vec : filled_lines) {
        for (auto &c : line_vec)
            if (c == '_') c = ' ';
        while (line_vec.size() < width)
            line_vec.push_back(' ');
    }

    if constexpr (false) for (auto line_vec : filled_lines) {
        for (auto c : line_vec) {
            std::cerr << '\'' << c << '\'' << ' ';
        }
        std::cerr << std::endl;
    }

    flattened_start_maps = std::vector((filled_lines.size() / height) - 1, std::vector<char>());

    for (size_t y = 0; y < height; y++) {
        for (size_t x = 0; x < width; x++) {
            flattened_goal_map.push_back(filled_lines[y][x]);
            for (size_t i = 0; i < flattened_start_maps.size(); i++)
                flattened_start_maps[i].push_back(filled_lines[y + (i + 1) * height][x]);
        }
    }


    unordered_map<char, size_t> goal_char_counts;
    for (auto c : flattened_goal_map) {
        if (goal_char_counts.contains(c)) {
            goal_char_counts[c]++;
        } else {
            goal_char_counts[c] = 1;
        }
    }
    assert (goal_char_counts[' '] >= 1); // There has to be at least one empty space in the puzzle.


    for (auto flattened_start_map : flattened_start_maps) {
        unordered_map<char, size_t> required_counts = goal_char_counts;

        for (auto c : flattened_start_map) {
            if (not required_counts.contains(c)) {
                std::cerr << "Tile '" << c << "' of start position '"
                    << std::string(flattened_start_map.begin(), flattened_start_map.end()) << "' not found in goal position '"
                    << std::string(flattened_goal_map.begin(), flattened_goal_map.end()) << "'.\n";
                assert (false);
            }
            required_counts[c]--;
        }

        for ([[maybe_unused]] auto count: required_counts | views::values) {
            assert (count == 0); // Start and goal maps have to contain the same amount of each character.
        }

    }

}


ABS::Gamestate* Model::getInitialState(int num) {
    auto* res = new EIGHT_PUZZLE::Gamestate();
    assert (num >= 0 and num < static_cast<int>(flattened_start_maps.size()));
    res->flattened_state_map = flattened_start_maps[num];
    return res;
}


ABS::Gamestate* Model::getInitialState(std::mt19937& rng) {
    auto* state = new EIGHT_PUZZLE::Gamestate();

    state->flattened_state_map = flattened_goal_map;
    scramble(*state, rng, 1000);


    return state;
}



ABS::Gamestate* Model::copyState(ABS::Gamestate* uncasted_state) {
    const auto state = dynamic_cast<EIGHT_PUZZLE::Gamestate*>(uncasted_state);
    assert (!!state);
    const auto new_state = new EIGHT_PUZZLE::Gamestate();
    *new_state = *state;
    return new_state;
}



[[nodiscard]] std::string Gamestate::toString() const {
    std::stringstream ss;
    ss << "((";
    for (const auto c : flattened_state_map) ss << c;
    ss << ")," << ABS::Gamestate::toString() << ")";
    return ss.str();
}

ABS::Gamestate* Model::deserialize(std::string &ostring) const {
    auto* state = new EIGHT_PUZZLE::Gamestate();
    //std::cerr << ostring << std::endl;

    state->flattened_state_map = std::vector(ostring.begin() + 2, ostring.begin() + 2 + width * height);
    //std::cerr << std::string(state->flattened_state_map.begin(), state->flattened_state_map.end()) << std::endl;

    std::istringstream iss(std::string(ostring.begin() + 2 + width * height + 3, ostring.end() - 2));
    char c1;
    int turn, terminal;
    iss >> turn >> c1 >> terminal;

    assert (c1 == ',');
    state->turn = turn;
    state->terminal = terminal;

    return state;
}


bool Gamestate::operator==(const ABS::Gamestate& other) const{
    const auto* other_checked = dynamic_cast<const EIGHT_PUZZLE::Gamestate*>(&other);
    return (
        other_checked != nullptr &&
        this->flattened_state_map == other_checked->flattened_state_map
    );
}

size_t Gamestate::hash() const {
    constexpr std::hash<char> hasher;
    size_t res = 0;
    for (const auto c : flattened_state_map)
        res ^= hasher(c) + 0x9e3779b9 + (res << 6) + (res >> 2);
    return res;
}

double Model::getDistance(const ABS::Gamestate* a, const ABS::Gamestate* b) const {
    const auto* state_a = dynamic_cast<const EIGHT_PUZZLE::Gamestate*>(a);
    assert (!!state_a);
    const auto* state_b = dynamic_cast<const EIGHT_PUZZLE::Gamestate*>(b);
    assert (!!state_b);

    double res = 0;
    for (size_t p = 0; p < width * height; p++)
        if (state_a->flattened_state_map[p] != state_b->flattened_state_map[p])
            res++;
    return res;
}



void Model::printState(ABS::Gamestate* state) {
    const Gamestate* checked_state = dynamic_cast<EIGHT_PUZZLE::Gamestate*>(state);
    assert (!!checked_state);

    size_t read = 0;
    std::cout << '+';
    for (size_t i = 0; i < width; i++) std::cout << '-';
    std::cout << '+' << std::endl;
    for (size_t y = 0; y < height; y++) {
        std::cout << '|';
        for (size_t x = 0; x < width; x++) {
            std::cout << checked_state->flattened_state_map[read++];
        }
        std::cout << '|' << std::endl;
    }
    std::cout << '+';
    for (size_t i = 0; i < width; i++) std::cout << '-';
    std::cout << '+' << std::endl << std::endl;
}





std::vector<int> Model::actionShape() const {
    return {1 + static_cast<int>(width) * static_cast<int>(height)};
}

int Model::encodeAction(int* decoded_action) {
    return decoded_action[0] + 1;
}

std::vector<int> Model::decodeAction(int encoded_action) {
    return {encoded_action - 1};
}

std::vector<int> Model::getActions_(ABS::Gamestate* uncasted_state) {
    const Gamestate* state = dynamic_cast<Gamestate*>(uncasted_state);
    assert (!!state);

    std::vector<int> actions;

    int space_position = 0;
    for (size_t y_space = 0; y_space < height; y_space++)
        for (size_t x_space = 0; x_space < width; x_space++)
            if (state->flattened_state_map[space_position++] == ' ') { // space_position is incremented after reading, since encoding adds 1 anyway.
                if (x_space > 0) actions.push_back(space_position - 1);
                if (x_space + 1 < width) actions.push_back(space_position + 1);
                if (y_space > 0) actions.push_back(space_position - width);
                if (y_space + 1 < height) actions.push_back(space_position + width);
            }
    size_t remaining_actions = 0;
    for (size_t i = 0; i < actions.size(); i++) {
        int action = actions[i];
        if (std::find(actions.begin(), actions.begin() + remaining_actions, action) != actions.begin() + remaining_actions)
            continue;
        if (state->flattened_state_map[action - 1] == ' ') // subtract 1 because the actions are already encoded
            continue;
        actions[remaining_actions++] = action;
    }
    actions.erase(actions.begin() + remaining_actions, actions.end());

    actions.push_back(0); // noop
    return actions;
}

std::pair<std::vector<double>,double> Model::applyAction_(ABS::Gamestate* uncasted_state, int action, std::mt19937& rng, std::vector<std::pair<int,int>>* decision_outcomes) {
    auto* state = dynamic_cast<EIGHT_PUZZLE::Gamestate*>(uncasted_state);
    assert (!!state);
    assert(action >= 0 and static_cast<size_t>(action) <= width * height);



    double reward;
    if constexpr (false) { // alternative reward based on number of correct tiles:
        reward = 1.0;
        for (size_t i = 0; i < height * width; i++) {
            if (state->flattened_state_map[i] == ' ') continue;
            if (state->flattened_state_map[i] != flattened_goal_map[i])
                reward -= 1.0 / (height * width);
        }

    } else if constexpr (false) { // alternative reward based on distances to correct positions:
        reward = 1.0;
        for (size_t i = 0; i < height * width; i++) {
            if (state->flattened_state_map[i] == ' ') continue;
            int min_dist = width + height;
            for (size_t j = 0; j < height * width; j++) {
                if (state->flattened_state_map[i] == flattened_goal_map[j]) {
                    int new_dist = abs(static_cast<int>(i % width) - static_cast<int>(j % width))
                        + abs(static_cast<int>(i / width) - static_cast<int>(j / width));
                    min_dist = min(min_dist, new_dist);
                }
            }
            reward -= min_dist / (width + height);
        }
        reward /= height * width;
    } else reward = (state->flattened_state_map == flattened_goal_map)? 1.0 : 0.0;

    double probability = 1.0;
    size_t decision_point = 0;
    if (action != 0) {
        const int swap_position = action - 1;
        assert (state->flattened_state_map[swap_position] != ' ');
        const size_t swap_x = swap_position % width, swap_y = swap_position / width;
        int space_positions[4] = {-1, -1, -1, -1}, possible_spaces = 0;
        if (swap_x > 0) space_positions[0] = swap_position - 1;
        if (swap_x + 1 < width) space_positions[1] = swap_position + 1;
        if (swap_y > 0) space_positions[2] = swap_position - width;
        if (swap_y + 1 < height) space_positions[3] = swap_position + width;
        for (size_t i = 0; i < 4; i++)
            if (space_positions[i] != -1 and state->flattened_state_map[space_positions[i]] == ' ')
                space_positions[possible_spaces++] = space_positions[i];
        assert (possible_spaces);

        int space_position;
        if (decision_outcomes == nullptr) {
            space_position = space_positions[uniform_int_distribution<int>(0, possible_spaces - 1)(rng)];
        } else {
            space_position = space_positions[getDecisionPoint(decision_point, 0, possible_spaces - 1, decision_outcomes)];
        }
        probability /= possible_spaces;

        state->flattened_state_map[space_position] = state->flattened_state_map[swap_position];
        state->flattened_state_map[swap_position] = ' ';
    }

    return {{reward}, probability};
}



double Model::getMinV(int steps) const {
    constexpr double min_reward_per_step = 0.0;
    return steps * min_reward_per_step;
}

double Model::getMaxV(int steps) const {
    constexpr double max_reward_per_step = 1.0;
    return steps * max_reward_per_step;
}



std::vector<int> Model::obsShape() const {
    return {static_cast<int>(height), static_cast<int>(width), static_cast<int>(height), static_cast<int>(width)};
}

void Model::getObs(ABS::Gamestate* uncasted_state, int* obs) {
    const Gamestate* casted_state = dynamic_cast<EIGHT_PUZZLE::Gamestate*>(uncasted_state);
    assert (casted_state != nullptr);
    size_t write = 0;
    for (const auto symbol_at_state_position : casted_state->flattened_state_map) {
        for (const auto symbol_at_goal_position : flattened_goal_map) {
            obs[write++] = (symbol_at_state_position == symbol_at_goal_position)? 1.0 : 0.0;
        }
    }
}



void Model::scramble(EIGHT_PUZZLE::Gamestate &state, std::mt19937& rng, const int iterations) const {
    int space_position = static_cast<int>(ranges::find(state.flattened_state_map, ' ') - state.flattened_state_map.begin());
    int prev_position = -1;
    for (int i = 0; i < iterations; i++) {
        const size_t x_space = space_position % width;
        const size_t y_space = space_position / width;
        int possible_actions[4] = {
            (x_space > 0)? space_position - 1 : -1,
            (x_space + 1 < width)? space_position + 1 : -1,
            (y_space > 0)? static_cast<int>(space_position - width) : -1,
            (y_space + 1 < height)? static_cast<int>(space_position + width) : -1,
        };
        int valid_actions_count = 0;
        for (int j = 0; j < 4; j++) {
            const int read_action = possible_actions[j];
            if (read_action == -1 or read_action == prev_position) continue;
            possible_actions[valid_actions_count++] = read_action;
        }
        const int next_position = possible_actions[uniform_int_distribution(0, valid_actions_count - 1)(rng)];
        state.flattened_state_map[space_position] = state.flattened_state_map[next_position];
        state.flattened_state_map[next_position] = ' ';
        prev_position = space_position;
        space_position = next_position;
    }
}
