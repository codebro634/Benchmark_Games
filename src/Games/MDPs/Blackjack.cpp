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
// Functional changes:
// This version has a changed game-loop, since
//      in the original, the player had to decide their action before seeing the previously dealt card.
//      This version does apply the action before dealing a card, so there is no 'invisible' card deal to the player.
// This version can also handle finite decks:
//      When instantiating the Model, one may choose how often each card is in the deck.
//      'reoccurrences_of_cards = -1' results in an infinite deck with uniform distribution as in rddl.
// The dealer in this version draws cards until they have at least a many points as the player.
//      The rddl behavior can be enabled by 'stop_dealer_at_16=true'.
// On a tie, the reward is 'reward_on_tie'. In the rddl version, this would be 0.0.
// //////////////////////////////////////////////////////////////////

#include <bits/atomic_base.h>

#include "../../../include/Games/MDPs/Blackjack.h"
#include <iostream>
#include <cassert>
#include <complex>
#include <cstring>
#include <fstream>
#include <functional>
using namespace std;

namespace BLACKJACK {

    const auto all_card_types = "234567890BDKA";

    void add_card_value(int &stack_value, const char card) {
        if ('1' <= card and card <= '9') {
            stack_value += card - '0';
        } else if (card == 'A') {
            stack_value += 11;
            if (stack_value > 21) stack_value -= 10;
        } else {
            assert (card == '0' or card == 'B' or card == 'D' or card == 'K');
            stack_value += 10;
        }
    }

    int total_card_value(const std::vector<char>& cards) {
        int value = 0;
        for (const auto card : cards) {
            add_card_value(value, card);
        }
        return value;
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


using namespace BLACKJACK;

int Gamestate::dealt_cards_of_type(const char card_type) const {
    int res = 0;
    for (const auto c : player_cards)
        if (c == card_type) res++;
    for (const auto c : dealer_cards)
        if (c == card_type) res++;
    return res;
}

char Model::get_random_card(
    const Gamestate *state,
    double &probability,
    std::mt19937& rng,
    size_t &decision_point, std::vector<std::pair<int,int>>* decision_outcomes
) const {
    // can return '!', if the decision_outcomes try to choose a card with probability 0.0 !
    char card;
    if (decision_outcomes != nullptr) {
        card = all_card_types[Model::getDecisionPoint(decision_point, 0, static_cast<int>(std::strlen(all_card_types)) - 1, decision_outcomes)];
    } else {
        do {
            card = all_card_types[std::uniform_int_distribution(0, static_cast<int>(std::strlen(all_card_types)) - 1)(rng)];
        } while (reoccurrences_of_cards > 0 and std::uniform_int_distribution(1, reoccurrences_of_cards)(rng) <= state->dealt_cards_of_type(card));
        //       ^ reoccurrences_of_cards == 0 would mean infinite deck size, so we don't need to check for availability.
    }
    if (reoccurrences_of_cards == 0) /* reoccurrences_of_cards == 0 means infinite deck size */ {
        probability /= static_cast<double>(std::strlen(all_card_types));
        return card;
    } else {
        const size_t cards_in_deck = reoccurrences_of_cards * std::strlen(all_card_types) - (state->player_cards.size() + state->dealer_cards.size());
        const size_t cards_of_type = max(0, reoccurrences_of_cards - state->dealt_cards_of_type(card));
        const double p = static_cast<double>(cards_of_type) / static_cast<double>(cards_in_deck);
        probability *= p;

        // Wierd edge case: some cards might have 0.0 probability, but getDecisionPoint can choose it.
        // The resulting state would have UB, so we return '!', to signal an invalid decision
        if (cards_of_type == 0) {
            return '!';
        }
        return card;
    }
}

bool Model::dealer_hit(const int player_value, const int dealer_value) const {
    if (player_value > 21 or dealer_value >= 21) return false;
    if (stop_dealer_at_16) {
        return dealer_value <= 16;
    } else {
        return dealer_value < player_value;
    }
}



BLACKJACK::Model::Model(const int reoccurrences_of_cards, const bool stop_dealer_at_16, const double reward_on_tie) {
    this->reoccurrences_of_cards = reoccurrences_of_cards;
    this->stop_dealer_at_16 = stop_dealer_at_16;
    this->reward_on_tie = reward_on_tie;
}

ABS::Gamestate* Model::getInitialState(int num) {
    auto* res = new BLACKJACK::Gamestate();
    switch (num) {
        default:
            assert (false);
    }
    return res;
}


ABS::Gamestate* Model::getInitialState(std::mt19937& rng) {
    auto* state = new BLACKJACK::Gamestate();

    state->current_stage = START;
    state->player_cards = {};
    state->dealer_cards = {};

    return state;
}



ABS::Gamestate* Model::copyState(ABS::Gamestate* uncasted_state) {
    const auto state = dynamic_cast<BLACKJACK::Gamestate*>(uncasted_state);
    assert (!!state);
    const auto new_state = new BLACKJACK::Gamestate();
    *new_state = *state;
    return new_state;
}



[[nodiscard]] std::string Gamestate::toString() const {
    std::stringstream ss;
    ss << "((";
    ss << static_cast<int>(current_stage) << ',';
    for (const auto player_card : player_cards) ss << player_card;
    ss << ',';
    for (const auto dealer_card : dealer_cards) ss << dealer_card;
    ss << ")," << ABS::Gamestate::toString() << ")";
    return ss.str();
}

ABS::Gamestate* Model::deserialize(std::string &ostring) const {

    auto* state = new BLACKJACK::Gamestate();

    int temp_nr;
    std::istringstream iss(ostring);
    char c1, c2;
    iss >> c1 >> c2;
    assert (c1 == '(' and c2 == '(');
    iss >> temp_nr;
    state->current_stage = static_cast<Stage>(temp_nr);
    iss >> c1;
    assert (c1 == ',');

    do {
        iss >> c1;
        if (c1 != ',')
            state->player_cards.emplace_back(c1);
    } while (c1 != ',');

    do {
        iss >> c1;
        if (c1 != ')')
            state->dealer_cards.emplace_back(c1);
    } while (c1 != ')');

    iss >> c1 >> c2;
    assert (c1 == ',' and c2 == '(');
    int turn, terminal;
    iss >> turn >> c1 >> terminal >> c2;
    assert (c1 == ',' and c2 == ')');
    iss >> c1;
    assert (c1 == ')');


    state->turn = turn;
    state->terminal = terminal;
    return state;
}


bool Gamestate::operator==(const ABS::Gamestate& other) const{
    const auto* other_checked = dynamic_cast<const BLACKJACK::Gamestate*>(&other);
    return (
        other_checked != nullptr &&
        this->current_stage == other_checked->current_stage &&
        this->player_cards == other_checked->player_cards &&
        this->dealer_cards == other_checked->dealer_cards
    );
}

size_t Gamestate::hash() const {
    constexpr std::hash<int> hasher;
    size_t res = 0;
    res ^= hasher(current_stage) + 0x9e3779b9 + (res << 6) + (res >> 2);
    for (const auto player_card : player_cards)
        res ^= hasher(player_card) + 0x9e3779b9 + (res << 6) + (res >> 2);
    for (const auto dealer_card : dealer_cards)
        res ^= hasher(dealer_card) + 0x9e3779b9 + (res << 6) + (res >> 2);
    return res;
}

double Model::getDistance(const ABS::Gamestate* a, const ABS::Gamestate* b) const {
    const auto* state_a = dynamic_cast<const BLACKJACK::Gamestate*>(a);
    assert (!!state_a);
    const auto* state_b = dynamic_cast<const BLACKJACK::Gamestate*>(b);
    assert (!!state_b);

    double res = 0;
    if (state_a->current_stage != state_b->current_stage) res += 10.0;
    res += abs(total_card_value(state_a->player_cards) - total_card_value(state_b->player_cards));
    res += abs(total_card_value(state_a->dealer_cards) - total_card_value(state_b->dealer_cards));
    return res;
}



void Model::printState(ABS::Gamestate* state) {
    const Gamestate* checked_state = dynamic_cast<BLACKJACK::Gamestate*>(state);
    assert (!!checked_state);

    std::cout << "Player hand: ";
    for (const auto player_card : checked_state->player_cards) std::cout << player_card;
    std::cout << std::endl;

    std::cout << "Dealer hand: ";
    for (const auto dealer_card : checked_state->dealer_cards) std::cout << dealer_card;
    std::cout << std::endl;

    if (player_decision(checked_state->current_stage)) std::cout << "Hit?";
    if (checked_state->current_stage == DONE) {
        const int player_value_sum = total_card_value(checked_state->player_cards);
        const int dealer_value_sum = total_card_value(checked_state->dealer_cards);

        if (player_value_sum > 21) std::cout << "DEALER WON: player over 21";
        else if (dealer_value_sum > 21) std::cout << "PLAYER WON: dealer over 21";
        else if (player_value_sum > dealer_value_sum) std::cout << "PLAYER WON: player over dealer";
        else if (player_value_sum < dealer_value_sum) std::cout << "DEALER WON: dealer over player";
        else std::cout << "TIE: equal card value";
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
    const auto* state = dynamic_cast<BLACKJACK::Gamestate*>(uncasted_state);
    assert (!!state);

    if (player_decision(state->current_stage))
        return {0, 1};
    else
        return {0};
}

std::pair<std::vector<double>,double> Model::applyAction_(ABS::Gamestate* uncasted_state, int action, std::mt19937& rng, std::vector<std::pair<int,int>>* decision_outcomes) {
    auto* state = dynamic_cast<BLACKJACK::Gamestate*>(uncasted_state);
    assert (!!state);
    assert(action >= 0);


    int player_value_sum = total_card_value(state->player_cards),
        dealer_value_sum = total_card_value(state->dealer_cards);


    // If any end-condition was met during the last round, end the game and calculate who won:
    // (Reward couldn't be given in the last round, since it depended on the random card draw, and we want a deterministic reward)
    if (state->current_stage == DONE) {
        state->terminal = true;

        double reward = 0.0;

        if (player_value_sum > 21) reward = -1.0;
        else if (dealer_value_sum > 21) reward = 1.0;
        else if (player_value_sum > dealer_value_sum) reward = 1.0;
        else if (player_value_sum < dealer_value_sum) reward = -1.0;
        else reward = reward_on_tie;

        return {{reward}, 1.0};
    }


    // Determine the stage of the game: Who gets the card of this turn (if anyone)?
    switch (state->current_stage) {
        case START:
            state->current_stage = P1; break;
        case P1:
            state->current_stage = P2; break;
        case P2:
            state->current_stage = D1; break;
        case D1:
            state->current_stage = D2; break;
        case PN:
            if (player_value_sum > 21) {
                state->current_stage = DONE; break;
            } [[fallthrough]];
        case D2:
            if (action == 1) {
                state->current_stage = PN; break;
            } [[fallthrough]];
        case DN:
            if (dealer_hit(player_value_sum, dealer_value_sum)) {
                state->current_stage = DN; break;
            } [[fallthrough]];
        case DONE:
            state->current_stage = DONE;
    }

    double probability = 1.0;

    // Deal the card to the player or the dealer:
    if (state->current_stage != DONE) {
        size_t decision_point = 0;

        const char current_card = get_random_card(state, probability, rng, decision_point, decision_outcomes);
        if (current_card == '!') {
            // the decision_outcomes have tried to choose a card that is not in the deck.
            assert (probability == 0.0);
            // Reset the state, to avoid UB:
            state->current_stage = START;
            state->player_cards = {};
            state->dealer_cards = {};
            return {{0.0}, probability};
        }
        if (card_for_player(state->current_stage)) {
            state->player_cards.emplace_back(current_card);
            add_card_value(player_value_sum, current_card);
        } else {
            state->dealer_cards.emplace_back(current_card);
            add_card_value(dealer_value_sum, current_card);
        }

        // Recheck end-conditions, since they might have changed since they got checked in the determination of the stage:
        if (player_value_sum > 21 or (state->current_stage == DN and not dealer_hit(player_value_sum, dealer_value_sum)))
            state->current_stage = DONE;
    }

    return {{0.0}, probability};
}



double Model::getMinV(int steps) const {
    return min(-1.0, reward_on_tie);
}

double Model::getMaxV(int steps) const {
    return max(1.0, reward_on_tie);
}



std::vector<int> Model::obsShape() const {
    return {2};
}


void Model::getObs(ABS::Gamestate* uncasted_state, int* obs) {
    const Gamestate* casted_state = dynamic_cast<BLACKJACK::Gamestate*>(uncasted_state);
    assert (casted_state != nullptr);
    obs[0] = total_card_value(casted_state->player_cards);
    obs[1] = total_card_value(casted_state->dealer_cards);
}
