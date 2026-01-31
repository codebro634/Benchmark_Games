#pragma once

#ifndef SOKOBAN_GAME_H
#define SOKOBAN_GAME_H
#include <vector>

#include "../Gamestate.h"
#endif

namespace SOKOBAN_GAME
{

    struct Gamestate: public ABS::Gamestate{
        std::vector<char> map;

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
        std::vector<int> decodeAction(int action) override;



        [[nodiscard]] double getMinV(int steps) const override;
        [[nodiscard]] double getMaxV(int steps) const override;
        [[nodiscard]] double getDistance(const ABS::Gamestate* a, const ABS::Gamestate* b) const override;

    private:
        size_t x_size, y_size;
        std::vector<char> initial_map;

        double completion = 10.0, step_penalty = 0.1, bonus_stored = 1.0;


        char get(const Gamestate *state, const size_t x, const size_t y) const { return state->map[x + y * x_size]; }
        char& get(Gamestate *state, const size_t x, const size_t y) const { return state->map[x + y * x_size]; }

        inline void apply_push(Gamestate *state, size_t x, size_t y, size_t dx, size_t dy) const;


    protected:
        std::pair<std::vector<double>,double> applyAction_(ABS::Gamestate* uncasted_state, int action, std::mt19937& rng, std::vector<std::pair<int,int>>* decision_outcomes) override;
        std::vector<int> getActions_(ABS::Gamestate* uncasted_state) override;
    };

}