#pragma once

#ifndef EIGHT_PUZZLE_H
#define EIGHT_PUZZLE_H
#include <vector>

#include "../Gamestate.h"
#endif

namespace EIGHT_PUZZLE
{

    struct Gamestate: public ABS::Gamestate{
        std::vector<char> flattened_state_map;

        bool operator==(const ABS::Gamestate& other) const override;
        [[nodiscard]] size_t hash() const override;
        [[nodiscard]] std::string toString() const override;
    };

    class Model: public ABS::Model
    {
    public:
        ~Model() override = default;
        explicit Model(const std::string& fileName);
        void printState(ABS::Gamestate* state) override;
        ABS::Gamestate* getInitialState(std::mt19937& rng) override;
        ABS::Gamestate* getInitialState(int num) override;
        ABS::Gamestate* copyState(ABS::Gamestate* uncasted_state) override;
        int getNumPlayers() override { return 1; }
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

        void scramble(EIGHT_PUZZLE::Gamestate &state, std::mt19937& rng, int iterations = 10000) const;

    private:
        size_t width, height;
        std::vector<std::vector<char>> flattened_start_maps;
        std::vector<char> flattened_goal_map;


    protected:
        std::pair<std::vector<double>,double> applyAction_(ABS::Gamestate* uncasted_state, int action, std::mt19937& rng, std::vector<std::pair<int,int>>* decision_outcomes) override;
        std::vector<int> getActions_(ABS::Gamestate* uncasted_state) override;
    };

}