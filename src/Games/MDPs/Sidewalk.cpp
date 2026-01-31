// //////////////////////////////////////////////////////////////////
// C++ adaptation of an example RDDL description for one or more people walking down a
// sidewalk with 2 "lanes" (top and bottom).  Both start out in the
// bottom lane.
//
// This is a c++ adaptation of
// 'https://github.com/pyrddlgym-project/rddlrepository/blob/9e0e0570ae149274323d4b109ae92a12da0bbf66/rddlrepository/archive/rddlsim/Sidewalk/domain.rddl'
// originally by author(s):
//      Tom Walsh thomasjwalsh@gmail.com
// //////////////////////////////////////////////////////////////////

#include <bits/atomic_base.h>

#include "../../../include/Games/MDPs/Sidewalk.h"
#include <iostream>
#include <cassert>
#include <complex>
#include <fstream>
#include <functional>
using namespace std;




using namespace SIDEWALK;


Model::Model(const std::string& fileName) {
    std::ifstream file(fileName);

    if (!file.is_open()){
        std::cerr << "Could not open file " << fileName << std::endl;
        exit(1);
    }

    std::unordered_map<std::string, std::vector<double>> named_segment_data;
    std::string segment;

    while (std::getline(file, segment, '#')) {
        std::string keyword = segment.substr(0, segment.find(':'));
        std::stringstream read_data(segment.substr(segment.find(':') + 1));
        std::vector<double> data;
        double number;
        while ((read_data >> number).good())
            data.push_back(number);
        named_segment_data.emplace(keyword, data);
        /* Debug print for reading in data
        std::cout << keyword << ":";
        for (const double d : data) std::cout << ' ' << d;
        std::cout << std::endl;
        //*/
    }

    assert (named_segment_data.contains("sidewalk-x-size"));
    assert (named_segment_data["sidewalk-x-size"].size() == 1);
    sidewalk_x_size = named_segment_data["sidewalk-x-size"].at(0);

    if (named_segment_data.contains("sidewalk-y-size")) {
        assert (named_segment_data["sidewalk-y-size"].size() == 1);
        sidewalk_y_size = named_segment_data["sidewalk-y-size"].at(0);
    }

    assert (named_segment_data.contains("starting-x-positions"));
    number_of_pedestrians = named_segment_data["starting-x-positions"].size();
    assert (named_segment_data.contains("starting-y-positions"));
    assert (named_segment_data["starting-y-positions"].size() == number_of_pedestrians);
    starting_positions = std::vector<std::pair<int, int>>();
    for (size_t i = 0; i < number_of_pedestrians; i++) {
        int x_pos = named_segment_data["starting-x-positions"].at(i),
            y_pos = named_segment_data["starting-y-positions"].at(i);
        assert (0 <= x_pos and x_pos < sidewalk_x_size);
        assert (0 <= y_pos and y_pos < sidewalk_y_size);
        starting_positions.emplace_back(x_pos, y_pos);
    }

    assert (named_segment_data.contains("goal-x-positions"));
    assert (named_segment_data["goal-x-positions"].size() == number_of_pedestrians);
    goal_x_positions = std::vector<int>();
    for (size_t i = 0; i < number_of_pedestrians; i++) {
        int goal_x_pos = named_segment_data["goal-x-positions"].at(i);
        assert (0 <= goal_x_pos and goal_x_pos < sidewalk_x_size);
        goal_x_positions.emplace_back(goal_x_pos);
    }

    assert (named_segment_data.contains("max-moving-pedestrians-per-step"));
    assert (named_segment_data["max-moving-pedestrians-per-step"].size() == 1);
    max_moving_pedestrians_per_step = named_segment_data["max-moving-pedestrians-per-step"].at(0);

    if (named_segment_data.contains("correct-collision-behavior")) {
        assert (named_segment_data["correct-collision-behavior"].size() == 1);
        correct_collision_behavior = named_segment_data["correct-collision-behavior"].at(0);
    }
}


ABS::Gamestate* Model::getInitialState(int num) {
    auto* res = new SIDEWALK::Gamestate();
    switch (num) {
        case 0:
            res->positions = starting_positions; break;
        default:
            assert (false);
    }
    return res;
}


ABS::Gamestate* Model::getInitialState(std::mt19937& rng) {
    return getInitialState(0);
}



ABS::Gamestate* Model::copyState(ABS::Gamestate* uncasted_state) {
    const auto state = dynamic_cast<SIDEWALK::Gamestate*>(uncasted_state);
    assert (!!state);
    const auto new_state = new SIDEWALK::Gamestate();
    *new_state = *state;
    return new_state;
}



[[nodiscard]] std::string Gamestate::toString() const {
    std::stringstream ss;
    ss << "((";
    if (positions.size() > 0) {
        ss << positions[0].first << ',' << positions[0].second;
        for (size_t i = 1; i < positions.size(); i++)
            ss << ',' << positions[i].first << ',' << positions[i].second;
    }
    ss << ")," << ABS::Gamestate::toString() << ")";
    return ss.str();
}

ABS::Gamestate* Model::deserialize(std::string &ostring) const {
    vector<int> temp_nr_buffer;
    int temp_nr;
    std::istringstream iss(ostring);
    char c1, c2;
    iss >> c1 >> c2;
    assert (c1 == '(' and c2 == '(');
    do {
        iss >> temp_nr;
        temp_nr_buffer.push_back(temp_nr);
        iss >> c1;
    } while (c1 == ',');
    assert (c1 == ')');
    iss >> c1 >> c2;
    assert (c1 == ',' and c2 == '(');
    int turn, terminal;
    iss >> turn >> c1 >> terminal >> c2;
    assert (c1 == ',' and c2 == ')');
    iss >> c1;
    assert (c1 == ')');

    assert (temp_nr_buffer.size() % 2 == 0);

    unsigned int read_index = 0;
    auto* state = new SIDEWALK::Gamestate();

    state->positions = std::vector<std::pair<int, int>>();
    while (read_index < temp_nr_buffer.size()) {
        int x = temp_nr_buffer[read_index++];
        int y = temp_nr_buffer[read_index++];
        state->positions.emplace_back(x, y);
    }

    state->turn = turn;
    state->terminal = terminal;
    return state;
}


bool Gamestate::operator==(const ABS::Gamestate& other) const{
    const auto* other_checked = dynamic_cast<const SIDEWALK::Gamestate*>(&other);
    return (
        other_checked != nullptr &&
        this->positions == other_checked->positions
    );
}

size_t Gamestate::hash() const {
    size_t res = 0;
    for (auto [x, y] : positions) {
        constexpr std::hash<int> hasher;
        res ^= hasher(x) + 0x9e3779b9 + (res << 6) + (res >> 2);
        res ^= hasher(y) + 0x9e3779b9 + (res << 6) + (res >> 2);
    }
    return res;
}

double Model::getDistance(const ABS::Gamestate* a, const ABS::Gamestate* b) const {
    const auto* state_a = dynamic_cast<const SIDEWALK::Gamestate*>(a);
    assert (!!state_a);
    const auto* state_b = dynamic_cast<const SIDEWALK::Gamestate*>(b);
    assert (!!state_b);

    double res = 0;
    for (size_t i = 0; i < number_of_pedestrians; i++) {
        res += abs(state_a->positions[i].first - state_b->positions[i].first);
        res += abs(state_a->positions[i].second - state_b->positions[i].second);
    }

    return res;
}



void Model::printState(ABS::Gamestate* state) {
    const Gamestate* checked_state = dynamic_cast<SIDEWALK::Gamestate*>(state);
    assert (!!checked_state);

    std::vector print_map(sidewalk_x_size, std::vector(sidewalk_y_size, ' '));
    std::vector wants_to_be_at_x(sidewalk_x_size, std::vector<char>());

    for (size_t i = 0; i < number_of_pedestrians; i++) {
        char name = '0' + i;
        auto [x, y] = checked_state->positions[i];
        print_map[x][y] = name;
        wants_to_be_at_x[goal_x_positions[i]].emplace_back(name);
    }

    for (int x = 0; x < sidewalk_x_size; x++) {
        for (int y = 0; y < sidewalk_y_size; y++) {
            std::cout << print_map[x][y];
        }
        std::cout << '|';
        for (const auto wants_to_be_here : wants_to_be_at_x[x]) {
            std::cout << wants_to_be_here;
        }
        std::cout << std::endl;
    }
}





std::vector<int> Model::actionShape() const {
    return std::vector(number_of_pedestrians, 5);
}

int Model::encodeAction(int* decoded_action) {
    int res = 0;
    for (int i = number_of_pedestrians - 1; i >= 0; i--) {
        res *= 5;
        res += decoded_action[i];
    }
    return res;
}

std::vector<int> Model::decodeAction(int action) {
    std::vector res(number_of_pedestrians, 0);
    for (size_t i = 0; i < number_of_pedestrians; i++) {
        res[i] = action % 5;
        action /= 5;
    }
    return res;
}

std::vector<int> Model::getActions_(ABS::Gamestate* uncasted_state) {
    const Gamestate* state = dynamic_cast<Gamestate*>(uncasted_state);
    assert (!!state);

    std::vector<int> actions = {};

    std::vector<int> power_of_five = std::vector(number_of_pedestrians, 0);
    for (size_t i = 0, pow = 1; i < number_of_pedestrians; i++) {
        power_of_five[i] = pow; pow *= 5;
    }

    // for all allowed numbers of moving pedestrians:
    for (int num_move_ped = 0; num_move_ped < max_moving_pedestrians_per_step; ++num_move_ped) {

        // for all choices of moving pedestrians:
        std::vector<size_t> moving_pedestrians = std::vector<size_t>(num_move_ped, 0ul);
        for (size_t p = 0; p < num_move_ped; ++p) moving_pedestrians[p] = p;
        bool more_pedestrians_needed = false;
        while (not more_pedestrians_needed) {

            // for all (non-zero) moves of these pedestrians:
            std::vector<int> moves = std::vector(num_move_ped, 1);
            bool no_more_different_moves = false;
            while (not no_more_different_moves) {

                // encode action
                int action = 0;
                for (size_t i = 0; i < num_move_ped; ++i) {
                    action += moves[i] * power_of_five[moving_pedestrians[i]];
                }
                actions.push_back(action);

                for (size_t p = 0; p <= num_move_ped; ++p) {
                    if (p == num_move_ped) {
                        no_more_different_moves = true;
                        break;
                    }
                    if (++moves[p] < 5) break;
                    moves[p] = 1;
                }
            }

            for (size_t p = 0; p <= num_move_ped; ++p) {
                if (p == num_move_ped) {
                    more_pedestrians_needed = true;
                    break;
                }
                size_t limit = number_of_pedestrians;
                if (p + 1 < num_move_ped) limit = moving_pedestrians[p + 1];
                if (++moving_pedestrians[p] < limit) break;
                moving_pedestrians[p] = p;
            }
        }

    }

    return actions;
}

std::pair<std::vector<double>,double> Model::applyAction_(ABS::Gamestate* uncasted_state, int action, std::mt19937& rng, std::vector<std::pair<int,int>>* decision_outcomes) {
    auto* state = dynamic_cast<SIDEWALK::Gamestate*>(uncasted_state);
    assert (!!state);
    assert(action >= 0);

    const std::vector decoded_action = decodeAction(action);

    //std::cout << action << " ="; for (const auto action_part : decoded_action) std::cout << " " << action_part; std::cout << std::endl;

    // Check, if the action is correct:
    int moving_pedestrians = 0;
    for (const auto move : decoded_action) if (move != 0) moving_pedestrians++;
    assert (moving_pedestrians <= max_moving_pedestrians_per_step);


    // Calculate reward on pre-state (as in rddl):
    double reward = 0.0;
    for (size_t i = 0; i < number_of_pedestrians; i++) {
        if (state->positions[i].first == goal_x_positions[i])
            reward += 1.0;
    }


    // Calculate next positions (only accounting for collisions with borders):
    std::vector<std::pair<int, int>> next_positions = state->positions;
    for (size_t i = 0; i < number_of_pedestrians; i++) {
        switch (decoded_action[i]) {
            case 1: // up
                next_positions[i].second = min(next_positions[i].second + 1, sidewalk_y_size - 1);
                break;
            case 2: // left
                next_positions[i].first = min(next_positions[i].first + 1, sidewalk_x_size - 1);
                break;
            case 3: // down
                next_positions[i].second = max(next_positions[i].second - 1, 0);
                break;
            case 4: // right
                next_positions[i].first = max(next_positions[i].first - 1, 0);
                break;
            default:
                assert (decoded_action[i] == 0);
                break;
        }
    }

    // Check for collisions of next positions:
    std::map<std::pair<int, int>, int> count_next_position_at{};
    for (auto next_position : next_positions)
        count_next_position_at[next_position]++;


    // Don't move pedestrians, if they try to collide:
    for (size_t i = 0; i < number_of_pedestrians; i++) {
        if (count_next_position_at[next_positions[i]] > 1)
            next_positions[i] = state->positions[i];
    }


    if (correct_collision_behavior) {

        std::map<std::pair<int, int>, size_t> ped_at{};
        for (size_t p = 0; p < number_of_pedestrians; ++p) {
            assert (not ped_at.contains(state->positions[p])); // correct_collision_behavior should ensure no two pedestrians on one place!
            ped_at[state->positions[p]] = p;
        }

        std::vector<int> ped_state(number_of_pedestrians, 0); // 0: unclear, 1: can move, -1: is stuck / in progress
        for (size_t p = 0; p < number_of_pedestrians; ++p)
            if (not ped_at.contains(next_positions[p]))
                ped_state[p] = 1;

        std::vector<size_t> back_track_stack{};
        back_track_stack.reserve(number_of_pedestrians);

        for (size_t start_from = 0; start_from < number_of_pedestrians; ++start_from) {
            size_t head;
            for (head = start_from; ped_state[head] == 0; head = ped_at[next_positions[head]]) {
                back_track_stack.push_back(head);
                ped_state[head] = -1;
            }
            if (ped_state[head] == 1)
                for (const size_t p : back_track_stack)
                    ped_state[p] = 1;
            back_track_stack.clear();
        }

        for (size_t p = 0; p < number_of_pedestrians; ++p)
            if (ped_state[p] == 1)
                state->positions[p] = next_positions[p];

    } else {
        state->positions = next_positions;
    }


    return {{reward}, 1.0};
}



double Model::getMinV(int steps) const {
    return 0;
}

double Model::getMaxV(int steps) const {
    return steps * number_of_pedestrians;
}



std::vector<int> Model::obsShape() const {
    return {static_cast<int>(number_of_pedestrians), sidewalk_x_size, sidewalk_y_size};
}

void Model::getObs(ABS::Gamestate* uncasted_state, int* obs) {
    const Gamestate* casted_state = dynamic_cast<SIDEWALK::Gamestate*>(uncasted_state);
    assert (casted_state != nullptr);

    for (int i = 0; i < static_cast<int>(number_of_pedestrians) * sidewalk_x_size * sidewalk_y_size; i++)
        obs[i] = 0;

    for (size_t p = 0; p < number_of_pedestrians; p++) {
        obs[(p * sidewalk_x_size + casted_state->positions[p].first) * sidewalk_y_size + casted_state->positions[p].second] = 1;
    }
}
