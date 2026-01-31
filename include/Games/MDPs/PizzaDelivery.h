#pragma once

#ifndef PIZZA_DELIVERY_H
#define PIZZA_DELIVERY_H
#include <vector>

#include "../Gamestate.h"
#endif

namespace PIZZA_DELIVERY
{

    struct FluentTruckData {
        int current_location;
        int hot_pizzas_loaded;
        int cold_pizzas_loaded;

        bool operator==(const FluentTruckData &other) const {
            return (
                current_location == other.current_location &&
                hot_pizzas_loaded == other.hot_pizzas_loaded &&
                cold_pizzas_loaded == other.cold_pizzas_loaded
            );
        }
    };

    struct FluentLocationData {
        int delivered_hot_pizzas;
        int delivered_cold_pizzas;

        [[nodiscard]] int total_delivered_pizzas() const {
            return delivered_hot_pizzas + delivered_cold_pizzas;
        }

        bool operator==(const FluentLocationData &other) const {
            return (
                delivered_hot_pizzas == other.delivered_hot_pizzas &&
                delivered_cold_pizzas == other.delivered_cold_pizzas
            );
        }
    };

    struct Gamestate: public ABS::Gamestate{
        int nr_of_pizzas_at_shop;
        std::vector<FluentTruckData> trucks;
        std::vector<FluentLocationData> locations;

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
        bool hasTransitionProbs() override { return false; }

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
        int location_of_shop;
        int initial_nr_of_pizzas_at_shop;
        std::vector<int> capacity_of_truck;
        std::vector<double> drive_prob_of_truck;
        [[nodiscard]] int nr_of_trucks() const { return static_cast<int>(capacity_of_truck.size()); }
        std::vector<int> orders_at_location;
        [[nodiscard]] int nr_of_locations() const { return static_cast<int>(orders_at_location.size()); }

        std::vector<bool> location_connections_list;
        [[nodiscard]] bool connected(const int location1, const int location2) const {
            return location_connections_list[nr_of_locations() * location1 + location2];
        };

        bool rewarm_pizzas_at_shop = false;

        static std::tuple<int, double> get_binomial_sample_with_prob(
            int n, double p,
            std::vector<std::pair<int,int>>* &decision_outcomes,
            size_t &decision_point,
            std::mt19937& rng
        );


        bool delivery_ended(const Gamestate* state) const;


    protected:
        std::pair<std::vector<double>,double> applyAction_(ABS::Gamestate* uncasted_state, int action, std::mt19937& rng, std::vector<std::pair<int,int>>* decision_outcomes) override;
        std::vector<int> getActions_(ABS::Gamestate* uncasted_state) override;
    };

}