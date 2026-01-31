// //////////////////////////////////////////////////////////////////
// In the classic game of blackjack (or 21), cards are randomly drawn
// from a deck (with replacement) and added to the player's hand.
// The player decides whether to continue drawing or stop. Upon
// stopping, the player receives a score equal to the total value
// of all cards in the player's hand, or zero if the value exceeds 21.
//
// This is a c++ adaptation of
// 'https://github.com/pyrddlgym-project/rddlrepository/blob/ff5fac09d3796a07276b69c67630bddb643c70cd/rddlrepository/archive/arcade/Blackjack/domain.rddl'
// originally by author(s):
//    Mike Gimelfarb (mgimelfarb@yahoo.ca)
//
// Functional differences to the original rddl-specification:
//      - The reward is given one turn after the game has ended, in order to only depend on the pre-state. (deterministic reward)
//        This is in contrast to the rddl-specification, which immediately returns the reward, and thus depends on the random card draw.
// //////////////////////////////////////////////////////////////////

#include <bits/atomic_base.h>

#include "../../../include/Games/MDPs/BlackjackRddl.h"
#include <iostream>
#include <cassert>
#include <complex>
#include <cstring>
#include <fstream>
#include <functional>
using namespace std;

namespace BLACKJACK_RDDL {
    int get_random_card(
        double &probability,
        std::mt19937& rng,
        size_t &decision_point, std::vector<std::pair<int,int>>* decision_outcomes
    ) {
        int card;
        if (decision_outcomes != nullptr) {
            card = Model::getDecisionPoint(decision_point, 1, 10, decision_outcomes);
        } else {
            card = min(10, std::uniform_int_distribution(1, 13)(rng));
        }
        probability *= (card == 10? 4 : 1) / 13.0;
        return card;
    }

    void add_card_value(int &stack_value, const int card) {
        stack_value += card;
        if (card == 1 and stack_value + 10 < 21) stack_value += 10;
    }

    bool card_for_player(const Stage stage) {
        return stage == P1 or stage == P2 or stage == PN;
    }

    bool player_decision(const Stage stage) {
        return stage == D2 or stage == PN;
    }

    bool preparing(const Stage stage) {
        return stage != PN and stage != DN;
    }
}


using namespace BLACKJACK_RDDL;



ABS::Gamestate* Model::getInitialState(int num) {
    auto* res = new BLACKJACK_RDDL::Gamestate();
    switch (num) {
        default:
            assert (false);
    }
    return res;
}


ABS::Gamestate* Model::getInitialState(std::mt19937& rng) {
    auto* state = new BLACKJACK_RDDL::Gamestate();

    state->current_stage = P1;
    state->player_value_sum = 0;
    state->dealer_value_sum = 0;

    return state;
}



ABS::Gamestate* Model::copyState(ABS::Gamestate* uncasted_state) {
    const auto state = dynamic_cast<BLACKJACK_RDDL::Gamestate*>(uncasted_state);
    assert (!!state);
    const auto new_state = new BLACKJACK_RDDL::Gamestate();
    *new_state = *state;
    return new_state;
}



[[nodiscard]] std::string Gamestate::toString() const {
    std::stringstream ss;
    ss << "((";
    ss << static_cast<int>(current_stage) << ',' << player_value_sum << ',' << dealer_value_sum;
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

    assert (temp_nr_buffer.size() == 3);

    auto* state = new BLACKJACK_RDDL::Gamestate();

    state->current_stage = static_cast<Stage>(temp_nr_buffer[0]);
    state->player_value_sum = temp_nr_buffer[1];
    state->dealer_value_sum = temp_nr_buffer[2];

    state->turn = turn;
    state->terminal = terminal;
    return state;
}


bool Gamestate::operator==(const ABS::Gamestate& other) const{
    const auto* other_checked = dynamic_cast<const BLACKJACK_RDDL::Gamestate*>(&other);
    return (
        other_checked != nullptr &&
        this->current_stage == other_checked->current_stage &&
        this->player_value_sum == other_checked->player_value_sum &&
        this->dealer_value_sum == other_checked->dealer_value_sum
    );
}

size_t Gamestate::hash() const {
    constexpr std::hash<int> hasher;
    size_t res = 0;
    res ^= hasher(current_stage) + 0x9e3779b9 + (res << 6) + (res >> 2);
    res ^= hasher(player_value_sum) + 0x9e3779b9 + (res << 6) + (res >> 2);
    res ^= hasher(dealer_value_sum) + 0x9e3779b9 + (res << 6) + (res >> 2);
    return res;
}

double Model::getDistance(const ABS::Gamestate* a, const ABS::Gamestate* b) const {
    const auto* state_a = dynamic_cast<const BLACKJACK_RDDL::Gamestate*>(a);
    assert (!!state_a);
    const auto* state_b = dynamic_cast<const BLACKJACK_RDDL::Gamestate*>(b);
    assert (!!state_b);

    double res = 0;
    if (state_a->current_stage != state_b->current_stage) res += 10.0;
    res += abs(state_a->player_value_sum - state_b->player_value_sum);
    res += abs(state_a->dealer_value_sum - state_b->dealer_value_sum);
    return res;
}



void Model::printState(ABS::Gamestate* state) {
    const Gamestate* checked_state = dynamic_cast<BLACKJACK_RDDL::Gamestate*>(state);
    assert (!!checked_state);

    std::cout << "Player hand value: " << checked_state->player_value_sum << std::endl;
    std::cout << "Dealer hand value: " << checked_state->dealer_value_sum << std::endl;
    if (player_decision(checked_state->current_stage)) std::cout << "Hit?";
    if (checked_state->current_stage == DONE) {
        if (
            checked_state->player_value_sum <= 21 and
            (checked_state->dealer_value_sum > 21 or checked_state->player_value_sum > checked_state->dealer_value_sum)
        ) std::cout << "Player won!";
        else std::cout << "Dealer won!";
    }
    std::cout << std::endl;

}





std::vector<int> Model::actionShape() const {
    return {2};
}

int Model::encodeAction(int* decoded_action) {
    return *decoded_action;
}
std::vector<int> Model::decodeAction(int action) {
    return {action};
}

std::vector<int> Model::getActions_(ABS::Gamestate* uncasted_state) {
    const auto* state = dynamic_cast<BLACKJACK_RDDL::Gamestate*>(uncasted_state);
    assert (!!state);

    if (player_decision(state->current_stage))
        return {0, 1};
    else
        return {0};
}

std::pair<std::vector<double>,double> Model::applyAction_(ABS::Gamestate* uncasted_state, int action, std::mt19937& rng, std::vector<std::pair<int,int>>* decision_outcomes) {
    auto* state = dynamic_cast<BLACKJACK_RDDL::Gamestate*>(uncasted_state);
    assert (!!state);
    assert(action == 0 or action == 1);

    if (state->current_stage == DONE) {
        state->terminal = true;
        double reward = (
            state->player_value_sum <= 21 and
            (state->dealer_value_sum > 21 or state->player_value_sum > state->dealer_value_sum)
        )? 1.0 : 0.0;
        return {{reward}, 1.0};
    }


    double probability = 1.0;
    size_t decision_point = 0;

    const int current_card = get_random_card(probability, rng, decision_point, decision_outcomes);

    add_card_value(card_for_player(state->current_stage)? state->player_value_sum : state->dealer_value_sum, current_card);

    // Updating this after the cards means, that the action has to be chosen one turn early.
    //              But the rddl specification is written like this.
    switch (state->current_stage) {
        case P1:
            state->current_stage = P2; break;
        case P2:
            state->current_stage = D1; break;
        case D1:
            state->current_stage = D2; break;
        case PN:
            if (state->player_value_sum > 21) {
                state->current_stage = DONE; break;
            } [[fallthrough]];
        case D2:
            if (action == 1) {
                state->current_stage = PN; break;
            } [[fallthrough]];
        case DN:
            if (state->dealer_value_sum < 17) {
                state->current_stage = DN; break;
            } [[fallthrough]];
        case DONE:
            state->current_stage = DONE;
    }

    return {{0.0}, probability};
}



double Model::getMinV(int steps) const {
    return 0.0;
}

double Model::getMaxV(int steps) const {
    return 1.0;
}



std::vector<int> Model::obsShape() const {
    return {2};
}


void Model::getObs(ABS::Gamestate* uncasted_state, int* obs) {
    const Gamestate* casted_state = dynamic_cast<BLACKJACK_RDDL::Gamestate*>(uncasted_state);
    assert (casted_state != nullptr);
    obs[0] = casted_state->player_value_sum;
    obs[1] = casted_state->dealer_value_sum;
}
