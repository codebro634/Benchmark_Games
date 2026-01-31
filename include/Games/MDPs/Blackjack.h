#pragma once

#ifndef BLACKJACK_H
#define BLACKJACK_H
#include <vector>

#include "../Gamestate.h"
#endif

namespace BLACKJACK
{


    enum Stage {
        START, P1, P2, D1, D2, PN, DN, DONE
    };

    struct Gamestate: public ABS::Gamestate{
        Stage current_stage;
        std::vector<char> player_cards;
        std::vector<char> dealer_cards;

        bool operator==(const ABS::Gamestate& other) const override;
        [[nodiscard]] size_t hash() const override;
        [[nodiscard]] std::string toString() const override;

        [[nodiscard]] int dealt_cards_of_type(char card_type) const;
    };

    class Model: public ABS::Model
    {
    public:
        ~Model() override = default;
        explicit Model(int reoccurrences_of_cards = -1, bool stop_dealer_at_16 = false, double reward_on_tie = 0.0);
        void printState(ABS::Gamestate* state) override;
        ABS::Gamestate* getInitialState(std::mt19937& rng) override;
        ABS::Gamestate* getInitialState(int num) override;
        ABS::Gamestate* copyState(ABS::Gamestate* uncasted_state) override;
        int getNumPlayers() override {return 1;}
        bool hasTransitionProbs() override {return true;}

        [[nodiscard]] ABS::Gamestate* deserialize(std::string &ostring) const override;

        [[nodiscard]] std::vector<int> obsShape() const override;
        void getObs(ABS::Gamestate* uncasted_state, int* obs) override;
        [[nodiscard]] std::vector<int> actionShape() const override;
        [[nodiscard]] int encodeAction(int* decoded_action) override;
        [[nodiscard]] std::vector<int> decodeAction(int encoded_action) override;



        [[nodiscard]] double getMinV(int steps) const override;
        [[nodiscard]] double getMaxV(int steps) const override;
        [[nodiscard]] double getDistance(const ABS::Gamestate* a, const ABS::Gamestate* b) const override;

    private:
        int reoccurrences_of_cards = -1;
        bool stop_dealer_at_16 = false;
        double reward_on_tie = 0.5;
        char get_random_card(
            const Gamestate *state,
            double &probability,
            std::mt19937& rng,
            size_t &decision_point, std::vector<std::pair<int,int>>* decision_outcomes
        ) const;

        [[nodiscard]] bool dealer_hit(int player_value, int dealer_value) const;


    protected:
        std::pair<std::vector<double>,double> applyAction_(ABS::Gamestate* uncasted_state, int action, std::mt19937& rng, std::vector<std::pair<int,int>>* decision_outcomes) override;
        std::vector<int> getActions_(ABS::Gamestate* uncasted_state) override;
    };

}