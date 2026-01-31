// //////////////////////////////////////////////////////////////////
// In the game of Sokoban, a person pushes boxes in a warehouse to
// designated storage areas. The goal is to move all the boxes to
// one of these areas. A difficult aspect of the problem is the
// presence of rapid dead ends, i.e. pushing a box into a corner
// means the task will never be completed as pulling the box is not
// permitted.
//
// This is a c++ adaptation of
// 'https://github.com/pyrddlgym-project/rddlrepository/blob/aa8f378f07130cd03aeacee4a0fbef163e2b87ba/rddlrepository/archive/arcade/Sokoban/domain.rddl'
// originally by author(s):
// 		Mike Gimelfarb (mgimelfarb@yahoo.ca)
//
// //////////////////////////////////////////////////////////////////

#include <bits/atomic_base.h>

#include "../../../include/Games/MDPs/Sokoban.h"
#include <iostream>
#include <cassert>
#include <complex>
#include <fstream>
#include <functional>
using namespace std;

namespace SOKOBAN_GAME {

    // all code for the tile encodings should be here. No magical constants out of this scope!
    // encoding is not allowed to use ')' for serialization.

    inline bool valid_tile(const char c) {
        //       no object,     wall,      player,       box
        return (c == '?' or c == 'W' or c == 'P' or c == 'B'  //  on storage field
             or c == '_' or c == 'w' or c == 'p' or c == 'b');  // not a storage field
    }

    inline bool is_in_storage(const char c) {
        return c < '_';
    }

    inline char out_of_storage(const char c) {
        return is_in_storage(c)? c + ' ' : c;
    }

    inline char in_to_storage(const char c) {
        return is_in_storage(c)? c : c - ' ';
    }

    inline char transfer_tile(const char tile, const char storage) {
        return is_in_storage(storage)? in_to_storage(tile) : out_of_storage(tile);
    }

    inline int boxes_not_in_storage(const std::vector<char> &map) {
        int count = 0;
        for (const char c : map)
            if (c == 'b') ++count;
        return count;
    }

    inline void apply_push_kernel(char &c1, char &c2, char &c3) {
        const char c1oos = out_of_storage(c1), c2oos = out_of_storage(c2), c3oos = out_of_storage(c3);

        if (c1oos != 'p') return;

        if (c2oos == 'b') {
            if (c3oos != '_') return;
            c3 = transfer_tile('b', c3);

        } else if (c2oos != '_') return;


        c1 = transfer_tile('_', c1); c2 = transfer_tile('p', c2);
    }

}


using namespace SOKOBAN_GAME;


Model::Model(const std::string& fileName) {
    std::ifstream file(fileName);

    if (!file.is_open()){
        std::cerr << "Could not open file " << fileName << std::endl;
        exit(1);
    }

    x_size = -1;
    initial_map = std::vector<char>();
    std::string line;
    while (std::getline(file, line) and not line.empty()) {
        initial_map.insert(initial_map.end(), line.begin(), line.end());
        if (x_size == -1ul) {
            x_size = line.size();
        } else {
            assert (line.size() == x_size);
        }
        for (const char c : line) {
            assert (valid_tile(c));
        }
    }

    assert (x_size != -1ul and initial_map.size() % x_size == 0);

    y_size = initial_map.size() / x_size;

}


ABS::Gamestate* Model::getInitialState(int num) {
    auto* res = new SOKOBAN_GAME::Gamestate();
    switch (num) {
        case 0:
            res->map = initial_map; break;
        default:
            assert (false);
    }
    return res;
}


ABS::Gamestate* Model::getInitialState(std::mt19937& rng) {
    return getInitialState(0);
}



ABS::Gamestate* Model::copyState(ABS::Gamestate* uncasted_state) {
    const auto state = dynamic_cast<SOKOBAN_GAME::Gamestate*>(uncasted_state);
    assert (!!state);
    const auto new_state = new SOKOBAN_GAME::Gamestate();
    *new_state = *state;
    return new_state;
}



[[nodiscard]] std::string Gamestate::toString() const {
    std::stringstream ss;
    ss << "((";
    for (const char c : map) ss << c;
    ss << ")," << ABS::Gamestate::toString() << ")";
    return ss.str();
}

ABS::Gamestate* Model::deserialize(std::string &ostring) const {
    auto* state = new SOKOBAN_GAME::Gamestate();
    state->map = std::vector<char>();

    std::istringstream iss(ostring);
    char c1, c2;
    iss >> c1 >> c2;
    assert (c1 == '(' and c2 == '(');
    while (true) {
        iss >> c1;
        if (c1 == ')') break;
        state->map.push_back(c1);
    };
    assert (c1 == ')');
    iss >> c1 >> c2;
    assert (c1 == ',' and c2 == '(');
    int turn, terminal;
    iss >> turn >> c1 >> terminal >> c2;
    assert (c1 == ',' and c2 == ')');
    iss >> c1;
    assert (c1 == ')');

    assert (state->map.size() == x_size * y_size);

    state->turn = turn;
    state->terminal = terminal;
    return state;
}


bool Gamestate::operator==(const ABS::Gamestate& other) const{
    const auto* other_checked = dynamic_cast<const SOKOBAN_GAME::Gamestate*>(&other);
    return (
        other_checked != nullptr &&
        this->map == other_checked->map
    );
}

size_t Gamestate::hash() const {
    constexpr std::hash<char> hasher;
    size_t res = 0;
    for (const char c : map)
        res ^= hasher(c) + 0x9e3779b9 + (res << 6) + (res >> 2);
    return res;
}

double Model::getDistance(const ABS::Gamestate* a, const ABS::Gamestate* b) const {
    const auto* state_a = dynamic_cast<const SOKOBAN_GAME::Gamestate*>(a);
    assert (!!state_a);
    const auto* state_b = dynamic_cast<const SOKOBAN_GAME::Gamestate*>(b);
    assert (!!state_b);

    double res = 0;

    for (size_t i = 0; i < x_size * y_size; i++)
        if (state_a->map[i] != state_b->map[i])
            res += 1.0;
    return res;
}



void Model::printState(ABS::Gamestate* state) {
    const Gamestate* checked_state = dynamic_cast<SOKOBAN_GAME::Gamestate*>(state);
    assert (!!checked_state);

    for (size_t y = 0; y < y_size; y++) {
        for (size_t x = 0; x < x_size; x++) {
            std::cout << get(checked_state, x, y);
        }
        std::cout << std::endl;
    }
}





std::vector<int> Model::actionShape() const {
    return {5};
}

int Model::encodeAction(int* decoded_action) {
    return *decoded_action;
}

std::vector<int> Model::decodeAction(int action) {
    return {action};
}

std::vector<int> Model::getActions_(ABS::Gamestate* uncasted_state) {
    return {0, 1, 2, 3, 4};
}


inline void Model::apply_push(Gamestate *state, const size_t x, const size_t y, const size_t dx, const size_t dy) const {
    char &c1 = get(state, x, y), &c2 = get(state, x + dx, y + dy), &c3 = get(state, x + 2 * dx, y + 2 * dy);

    apply_push_kernel(c1, c2, c3);
}



std::pair<std::vector<double>,double> Model::applyAction_(ABS::Gamestate* uncasted_state, int action, std::mt19937& rng, std::vector<std::pair<int,int>>* decision_outcomes) {
    auto* state = dynamic_cast<SOKOBAN_GAME::Gamestate*>(uncasted_state);
    assert (!!state);
    assert (action >= 0 and action < 5);


    const int prev_not_stored_boxes = boxes_not_in_storage(state->map);



    if (action == 0) { // none
    } else if (action == 1) { // left
        for (size_t y = 0; y < y_size; ++y)
            for (size_t x = 2; x < x_size; ++x)
                apply_push(state, x, y, -1, 0);
    } else if (action == 2) { // right
        for (size_t y = 0; y < y_size; ++y)
            for (signed long x = x_size - 3; x >= 0; --x)
                apply_push(state, x, y, +1, 0);
    } else if (action == 3) { // up
        for (size_t y = 2; y < y_size; ++y)
            for (size_t x = 0; x < x_size; ++x)
                apply_push(state, x, y, 0, -1);
    } else if (action == 4) { // down
        for (signed long y = y_size - 3; y >= 0; --y)
            for (size_t x = 0; x < x_size; ++x)
                apply_push(state, x, y, 0, +1);
    }


    double reward = 0.0;


    // completion is checked on the pre-state:
    if (prev_not_stored_boxes == 0) {
        reward = completion;
        state->terminal = true;
    } else {
        const int post_not_stored_boxes = boxes_not_in_storage(state->map);
        const int stored_boxes_delta = prev_not_stored_boxes - post_not_stored_boxes;
        const int stored_boxes_delta_sign = (stored_boxes_delta > 0 ) - (stored_boxes_delta < 0);
        reward = stored_boxes_delta_sign * bonus_stored - step_penalty;
    }


    return {{reward}, 1.0};
}



double Model::getMinV(int steps) const {
    const double min_reward_per_step = -step_penalty - 1e-6;
    return steps * min_reward_per_step;
}

double Model::getMaxV(int steps) const {
    const double max_reward_per_step = completion + 1e-6;
    return steps * max_reward_per_step;
}



std::vector<int> Model::obsShape() const {
    return {static_cast<int>(x_size), static_cast<int>(y_size)};
}

void Model::getObs(ABS::Gamestate* uncasted_state, int* obs) {
    const Gamestate* casted_state = dynamic_cast<SOKOBAN_GAME::Gamestate*>(uncasted_state);
    assert (casted_state != nullptr);

    for (size_t i = 0; i < x_size * y_size; ++i) {
        obs[i] = static_cast<unsigned char>(casted_state->map[i]);
    }
}
