// //////////////////////////////////////////////////////////////////
// A pizza delivery task
//
// This is a c++ adaptation of
// 'https://github.com/pyrddlgym-project/rddlrepository/blob/93cc2fe7c26620c6f228fe583d28d624600364a4/rddlrepository/archive/rddlsim/Pizza/domain.rddl'
// originally by author(s):
// 		Tom Walsh (thomasjwalsh [at] gmail.com)
//
// Functional changes in this implementation:
//      - pizzas are modeled as their count and not their individual objects
//      - rewarm pizzas at shop mode
//      - the action dispose is automatically chosen, since cold pizzas have
//          no effect on the original game and rewarming cold pizzas is always the best option in rewarm mode
// //////////////////////////////////////////////////////////////////

#include <bits/atomic_base.h>

#include "../../../include/Games/MDPs/PizzaDelivery.h"
#include <iostream>
#include <cassert>
#include <complex>
#include <fstream>
#include <functional>
using namespace std;


namespace PIZZA_DELIVERY {
    [[nodiscard]] std::string repeat(const std::string &str, const int n) {
        std::stringstream ss;
        for (int i = 0; i < n; i++) {
            ss << str;
        }
        return ss.str();
    }

    constexpr int actions_per_truck = 3;
    int drive_goal_index(const int truck_nr) {
        return actions_per_truck * truck_nr + 0;
    }
    int deliver_nr_index(const int truck_nr) {
        return actions_per_truck * truck_nr + 1;
    }
    int load_nr_index(const int truck_nr) {
        return actions_per_truck * truck_nr + 2;
    }


    // Binomial Coefficient from 'https://www.geeksforgeeks.org/dsa/space-and-time-efficient-binomial-coefficient/':
    unsigned long long binomialCoefficient(const int n, int k) {
        unsigned long long res = 1;

        // Since C(n, k) = C(n, n-k)
        if (k > n - k)
            k = n - k;

        // Calculate value of
        // [n * (n-1) *---* (n-k+1)] / [k * (k-1) *----* 1]
        for (int i = 0; i < k; ++i) {
            res *= (n - i);
            res /= (i + 1);
        }

        return res;
    }

    double get_binomial_probability(const int n, const double p, const int v) {
        assert (0 <= n and n < 60);
        assert (0 <= v and v <= n);
        assert (0.0 <= p and p <= 1.0);
        return pow(p, v) * static_cast<double>(binomialCoefficient(n, v)) * pow(1 - p, n - v);
    }


    std::tuple<int, std::vector<int>, const std::vector<int>> init_quick_action_iterator(const std::vector<int> &action_shape) {
        std::vector<int> factors = std::vector(action_shape.size(), 0);
        int factor = 1;
        for (size_t i = 0; i < action_shape.size(); i++) {
            factors[i] = factor;
            factor *= action_shape[i];
        }
        return {0, std::vector(action_shape.size(), 0), factors};
    }
    int get(const std::tuple<int, std::vector<int>, const std::vector<int>> &a_action_factors, const size_t index) {
        return std::get<1>(a_action_factors)[index];
    }
    void set(std::tuple<int, std::vector<int>, const std::vector<int>> &a_action_factors, const size_t index, const int value) {
        std::get<0>(a_action_factors) += (value - (std::get<1>(a_action_factors)[index])) * std::get<2>(a_action_factors)[index];
        std::get<1>(a_action_factors)[index] = value;
    }
    int increment(std::tuple<int, std::vector<int>, const std::vector<int>> &a_action_factors, const size_t index) {
        std::get<0>(a_action_factors) += std::get<2>(a_action_factors)[index];
        return ++std::get<1>(a_action_factors)[index];
    }
    void reset(std::tuple<int, std::vector<int>, const std::vector<int>> &a_action_factors, const size_t index) {
        std::get<0>(a_action_factors) -= std::get<1>(a_action_factors)[index] * std::get<2>(a_action_factors)[index];
        std::get<1>(a_action_factors)[index] = 0;
    }
}


using namespace PIZZA_DELIVERY;



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
        /* Debug print for reading in the data for the model
        std::cout << keyword << ":";
        for (const double d : data) std::cout << ' ' << d;
        std::cout << std::endl;
        // */
    }

    assert (named_segment_data.contains("location_of_shop"));
    assert (named_segment_data["location_of_shop"].size() == 1);
    location_of_shop = static_cast<int>(named_segment_data["location_of_shop"][0]);
    assert (named_segment_data["location_of_shop"][0] == location_of_shop);

    assert (named_segment_data.contains("initial_nr_of_pizzas_at_shop"));
    assert (named_segment_data["initial_nr_of_pizzas_at_shop"].size() == 1);
    initial_nr_of_pizzas_at_shop = static_cast<int>(named_segment_data["initial_nr_of_pizzas_at_shop"][0]);
    assert (named_segment_data["initial_nr_of_pizzas_at_shop"][0] == initial_nr_of_pizzas_at_shop);

    assert (named_segment_data.contains("capacity_of_truck"));
    for (const double capacity : named_segment_data["capacity_of_truck"]) {
        assert (capacity == static_cast<int>(capacity));
        capacity_of_truck.push_back(static_cast<int>(capacity));
    }

    assert (named_segment_data.contains("drive_prob_of_truck"));
    drive_prob_of_truck = named_segment_data["drive_prob_of_truck"];
    assert (capacity_of_truck.size() == drive_prob_of_truck.size());

    assert (named_segment_data.contains("orders_at_location"));
    for (const double orders : named_segment_data["orders_at_location"]) {
        assert (orders == static_cast<int>(orders));
        orders_at_location.push_back(static_cast<int>(orders));
    }


    assert (named_segment_data.contains("location_connections_list"));
    for (const double connected : named_segment_data["location_connections_list"]) {
        assert (connected == 0 or connected == 1);
        location_connections_list.push_back(static_cast<bool>(connected));
    }
    assert (location_connections_list.size() == orders_at_location.size() * orders_at_location.size());

    if (named_segment_data.contains("rewarm_pizzas_at_shop")) {
        assert (named_segment_data["rewarm_pizzas_at_shop"].size() == 1);
        if (named_segment_data["rewarm_pizzas_at_shop"][0] == 0.0) {
            rewarm_pizzas_at_shop = false;
        }
        else if (named_segment_data["rewarm_pizzas_at_shop"][0] == 1.0) {
            rewarm_pizzas_at_shop = true;
        } else assert (false);  // 'rewarm_pizzas_at_shop' can only be '0' or '1'.
    }


    // Check if the action-space is small enough to fit into an int:
    unsigned int action_space_size = 1;
    for (const int a : PIZZA_DELIVERY::Model::actionShape()) {
        assert (action_space_size < numeric_limits<unsigned int>::max() / a);
        action_space_size *= a;
    }




    // The implementation of get_binomial_probability is inaccurate with n > 50
    // This limitation should be reasonable, since the example maps are all below 30 pizzas
    assert (min(initial_nr_of_pizzas_at_shop, *std::max_element(capacity_of_truck.begin(), capacity_of_truck.end())) <= 40);
    // To remove this limitation, get_binomial_probability has to be implemented with less loss of accuracy.
    // Use the code below to check the performance of the new implementation:
    if (false) {
        for (int n = 0; n < 100; n++) for (int d = 0; d < 100; d++) {
            double sum = 0.0;
            for (int i = 0; i <= n; i++)
                sum += get_binomial_probability(n, 1.0 - d / (d + 1.0), i);
            std::cerr << "n=" << n << " d=" << d << " sum=" << sum << std::endl;
            assert (abs(sum - 1.0) < 1e-6);
        }
        assert (false);
    }
}


ABS::Gamestate* Model::getInitialState(int num) {
    auto* res = new PIZZA_DELIVERY::Gamestate();
    switch (num) {
        case 0:
            res->nr_of_pizzas_at_shop = initial_nr_of_pizzas_at_shop;
            res->trucks = std::vector(nr_of_trucks(), FluentTruckData {
                .current_location = location_of_shop,
                .hot_pizzas_loaded = 0,
                .cold_pizzas_loaded = 0,
            });
            res->locations = std::vector(nr_of_locations(), FluentLocationData {
                .delivered_hot_pizzas = 0,
                .delivered_cold_pizzas = 0,
            });
            break;
        default:
            assert (false);
    }
    return res;
}


ABS::Gamestate* Model::getInitialState(std::mt19937& rng) {
    return getInitialState(0); // no random initialization implemented
}



ABS::Gamestate* Model::copyState(ABS::Gamestate* uncasted_state) {
    const auto state = dynamic_cast<PIZZA_DELIVERY::Gamestate*>(uncasted_state);
    assert (!!state);
    const auto new_state = new PIZZA_DELIVERY::Gamestate();
    *new_state = *state;
    return new_state;
}



[[nodiscard]] std::string Gamestate::toString() const {
    std::stringstream ss;
    ss << "((";
    ss << nr_of_pizzas_at_shop;
    for (const auto [current_location, hot_pizzas_loaded, cold_pizzas_loaded] : trucks) {
        ss << ',' << current_location;
        ss << ',' << hot_pizzas_loaded;
        ss << ',' << cold_pizzas_loaded;
    }
    for (const auto [delivered_hot_pizzas, delivered_cold_pizzas] : locations) {
        ss << ',' << delivered_hot_pizzas << ',' << delivered_cold_pizzas;
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

    assert (static_cast<int>(temp_nr_buffer.size()) == 1 + nr_of_trucks() * 3 + nr_of_locations() * 2);

    unsigned int read_index = 0;
    auto* state = new PIZZA_DELIVERY::Gamestate();

    state->nr_of_pizzas_at_shop = temp_nr_buffer[read_index++];
    state->trucks = {};
    state->trucks.reserve(nr_of_trucks());
    for (int i = 0; i < nr_of_trucks(); i++) {
        state->trucks.push_back(FluentTruckData {
            .current_location = temp_nr_buffer[read_index++],
            .hot_pizzas_loaded = temp_nr_buffer[read_index++],
            .cold_pizzas_loaded = temp_nr_buffer[read_index++],
        });
    }
    state->locations = {};
    state->locations.reserve(nr_of_trucks());
    for (int i = 0; i < nr_of_locations(); i++) {
        state->locations.push_back(FluentLocationData {
            .delivered_hot_pizzas = temp_nr_buffer[read_index++],
            .delivered_cold_pizzas = temp_nr_buffer[read_index++],
        });
    }

    state->turn = turn;
    state->terminal = terminal;
    return state;
}


bool Gamestate::operator==(const ABS::Gamestate& other) const{
    const auto* other_checked = dynamic_cast<const PIZZA_DELIVERY::Gamestate*>(&other);
    return (
        other_checked != nullptr &&
        this->nr_of_pizzas_at_shop == other_checked->nr_of_pizzas_at_shop &&
        this->trucks == other_checked->trucks &&
        this->locations == other_checked->locations
    );
}

size_t Gamestate::hash() const {
    constexpr std::hash<int> hasher;
    size_t res = hasher(nr_of_pizzas_at_shop) + 0x9e3779b9;
    for (const auto [current_location, hot_pizzas_loaded, cold_pizzas_loaded] : trucks) {
        res ^= hasher(current_location) + 0x9e3779b9 + (res << 6) + (res >> 2);
        res ^= hasher(hot_pizzas_loaded) + 0x9e3779b9 + (res << 6) + (res >> 2);
        res ^= hasher(cold_pizzas_loaded) + 0x9e3779b9 + (res << 6) + (res >> 2);
    }
    for (const auto [delivered_hot_pizzas, delivered_cold_pizzas] : locations) {
        res ^= hasher(delivered_hot_pizzas) + 0x9e3779b9 + (res << 6) + (res >> 2);
        res ^= hasher(delivered_cold_pizzas) + 0x9e3779b9 + (res << 6) + (res >> 2);
    }
    return res;
}

double Model::getDistance(const ABS::Gamestate* a, const ABS::Gamestate* b) const {
    const auto* state_a = dynamic_cast<const PIZZA_DELIVERY::Gamestate*>(a);
    assert (!!state_a);
    const auto* state_b = dynamic_cast<const PIZZA_DELIVERY::Gamestate*>(b);
    assert (!!state_b);

    double res = abs(state_a->nr_of_pizzas_at_shop - state_b->nr_of_pizzas_at_shop);
    for (int t = 0; t < nr_of_trucks(); t++) {
        const auto [current_location_a, hot_pizzas_loaded_a, cold_pizzas_loaded_a] = state_a->trucks[t];
        const auto [current_location_b, hot_pizzas_loaded_b, cold_pizzas_loaded_b] = state_b->trucks[t];
        if (current_location_a != current_location_b) res += 1;
        res += abs(hot_pizzas_loaded_a - hot_pizzas_loaded_b);
        res += 0.5 * abs(cold_pizzas_loaded_a - cold_pizzas_loaded_b);
    }
    for (int l = 0; l < nr_of_locations(); l++) {
        const auto la = state_a->locations[l];
        const auto lb = state_b->locations[l];
        res += abs(la.total_delivered_pizzas() - lb.total_delivered_pizzas());
        res += abs(la.delivered_hot_pizzas - lb.delivered_hot_pizzas);
    }
    return res;
}



void Model::printState(ABS::Gamestate* state) {
    const Gamestate* checked_state = dynamic_cast<PIZZA_DELIVERY::Gamestate*>(state);
    assert (!!checked_state);

    for (int i = 0; i < nr_of_locations(); i++) {
        std::cout << "L" << i << ":";
        std::cout << repeat("O", checked_state->locations[i].delivered_hot_pizzas)
                  << repeat("o", checked_state->locations[i].delivered_cold_pizzas)
                  << repeat("_", orders_at_location[i] - checked_state->locations[i].total_delivered_pizzas())
                  << repeat("?", checked_state->locations[i].total_delivered_pizzas() - orders_at_location[i])
                  << std::endl;
        if (i == location_of_shop) {
            std::cout << "S:" << repeat("O", checked_state->nr_of_pizzas_at_shop)
                << repeat("_", initial_nr_of_pizzas_at_shop - checked_state->nr_of_pizzas_at_shop)<< std::endl;
        }
        std::cout << " ";
        for (int t = 0; t < nr_of_trucks(); t++) {
            const auto truck = checked_state->trucks[t];
            if (truck.current_location == i) {
                std::cout << "T" << t << ": "
                    << repeat("O", truck.hot_pizzas_loaded) << repeat("o", truck.cold_pizzas_loaded)
                    << repeat("_", capacity_of_truck[t] - truck.cold_pizzas_loaded - truck.hot_pizzas_loaded)
                    << "  ";
            }
        }
        std::cout << std::endl;
    }
}





std::vector<int> Model::actionShape() const {
    std::vector res(nr_of_trucks() * actions_per_truck, initial_nr_of_pizzas_at_shop + 1);
    for (int i = 0; i < nr_of_trucks(); i++) {
        const int pizza_load_or_deliver_limit = max(initial_nr_of_pizzas_at_shop, capacity_of_truck[i]) + 1;
        res[drive_goal_index(i)] = nr_of_locations() + 1; // drive action can be -1, if not taken
        res[deliver_nr_index(i)] = pizza_load_or_deliver_limit;
        res[load_nr_index(i)] = pizza_load_or_deliver_limit;
    }
    return res;
}

int Model::encodeAction(int* decoded_action) {
    int res = 0;
    const auto action_shape = actionShape();
    for (int i = static_cast<int>(action_shape.size() - 1); i >= 0; i--) {
        res *= action_shape[i];
        res += decoded_action[i];
        if (i % actions_per_truck == drive_goal_index(0)) res += 1; // drive action can be -1, if not taken
    }
    return res;
}

std::vector<int> Model::decodeAction(int action) {
    const auto action_shape = actionShape();
    std::vector res(action_shape.size(), 0);
    for (size_t i = 0; i < action_shape.size(); i++) {
        res[i] = action % action_shape[i];
        if (i % actions_per_truck == static_cast<size_t>(drive_goal_index(0))) res[i] -= 1; // drive action can be -1, if not taken
        action /= action_shape[i];
    }
    assert (action == 0); // there should be nothing unencoded left
    return res;
}



bool Model::delivery_ended(const Gamestate* state) const {

    // True if all orders are fulfilled:
    bool any_open_orders = false;
    for (int location = 0; location < nr_of_locations(); location++) {
        if (state->locations[location].total_delivered_pizzas() < orders_at_location[location]) {
            any_open_orders = true;
            break;
        }
    }
    if (not any_open_orders) return true;

    // False if there is any hot pizza left to fulfill the order:
    if (state->nr_of_pizzas_at_shop > 0) return false;
    for (const auto truck : state->trucks)
        if (truck.hot_pizzas_loaded > 0) return false;
    // cold pizzas can still be used in rewarm mode
    if (rewarm_pizzas_at_shop) {
        for (const auto truck : state->trucks)
            if (truck.cold_pizzas_loaded > 0)
                return false;
    }

    // True otherwise (open orders but no more hot pizzas left)
    return true;
}

std::vector<int> Model::getActions_(ABS::Gamestate* uncasted_state) {
    const Gamestate* state = dynamic_cast<Gamestate*>(uncasted_state);
    assert (!!state);

    if (delivery_ended(state))
        return {0};

    std::vector<int> actions = {};

    auto action_iterator = init_quick_action_iterator(actionShape());

    std::vector load_max_for_truck(nr_of_trucks(), 0);
    for (int i = 0; i < nr_of_trucks(); i++) {
        const int location = state->trucks[i].current_location;
        load_max_for_truck[i] = min(
            (location == location_of_shop)? state->nr_of_pizzas_at_shop : 0,
            capacity_of_truck[i] - state->trucks[i].hot_pizzas_loaded - state->trucks[i].cold_pizzas_loaded
        );
    }

    std::vector<int> delivering_trucks = std::vector(nr_of_trucks(), -1);
    std::vector<int> loadable_trucks = std::vector(nr_of_trucks(), -1);

    int total_loaded_pizzas = 0;

    // for all combinations of truck movements: (stop, when the movement update runs out of trucks)
    for (
        int update_movement_of_truck = -1 /*run at least once*/;
        update_movement_of_truck < nr_of_trucks();
        /*movement update at the end of the loop*/
    ) {

        // pre-compute lists of trucks that can deliver / be loaded, so the following loops don't have to iterate over all trucks
        delivering_trucks.clear(); loadable_trucks.clear();
        for (int truck = 0; truck < nr_of_trucks(); truck++)
            if (get(action_iterator, drive_goal_index(truck)) == 0) /* 0 is no movement */ {
                if (state->trucks[truck].hot_pizzas_loaded > 0)
                    delivering_trucks.push_back(truck);
                if (load_max_for_truck[truck] > 0)
                    loadable_trucks.push_back(truck);
            }

        // for all delivery options of the delivering trucks: (stop, when the delivery update runs out of trucks)
        for (
            int update_delivery_of_delivering_truck_index = -1 /*run at least once*/;
            update_delivery_of_delivering_truck_index < static_cast<int>(delivering_trucks.size());
            /*delivery update at the end of the loop*/
        ) {

            // for all load options of the standing trucks: (stop, when the load update runs out of trucks)
            for (
                int update_load_of_loadable_truck_index = -1 /*run at least once*/;
                update_load_of_loadable_truck_index < static_cast<int>(loadable_trucks.size());
                /*load update at the end of the loop*/
            ) {


                // add action, if the total number of loaded pizzas is possible (is ensured in the load update below):
                assert (total_loaded_pizzas <= state->nr_of_pizzas_at_shop);
                actions.push_back(std::get<0>(action_iterator));


                // load update:
                for (
                    update_load_of_loadable_truck_index = 0;
                    update_load_of_loadable_truck_index < static_cast<int>(loadable_trucks.size());
                    update_load_of_loadable_truck_index++
                ) {
                    const int loadable_truck = loadable_trucks[update_load_of_loadable_truck_index];

                    // Try to load one more pizzas into the truck (change total_loaded_pizzas accordingly):
                    total_loaded_pizzas++; increment(action_iterator, load_nr_index(loadable_truck));
                    // update is done, if the truck can load the pizza, and the store has the pizza:
                    if (get(action_iterator, load_nr_index(loadable_truck)) <= load_max_for_truck[loadable_truck]
                        and total_loaded_pizzas <= state->nr_of_pizzas_at_shop)
                        break;
                    // Else, reset the load of the truck, and try the next one:
                    total_loaded_pizzas -= get(action_iterator, load_nr_index(loadable_truck));
                    reset(action_iterator, load_nr_index(loadable_truck));
                }

            }


            // delivery update:
            for (
                update_delivery_of_delivering_truck_index = 0;
                update_delivery_of_delivering_truck_index < static_cast<int>(delivering_trucks.size());
                update_delivery_of_delivering_truck_index++
            ) {
                const int delivering_truck = delivering_trucks[update_delivery_of_delivering_truck_index];

                // Try to deliver more currently hot pizzas from the truck:
                if (increment(action_iterator, deliver_nr_index(delivering_truck))
                    <= state->trucks[delivering_truck].hot_pizzas_loaded)
                    break;
                reset(action_iterator, deliver_nr_index(delivering_truck));
            }

        }


        // movement update:
        for (
            update_movement_of_truck = 0;
            update_movement_of_truck < nr_of_trucks();
            update_movement_of_truck++
        ) {

            // Try to drive to different connected location:
            const int prev_location = state->trucks[update_movement_of_truck].current_location;
            int goal_position = get(action_iterator, drive_goal_index(update_movement_of_truck)) - 1; // 0 is no movement
            // Find next connected location:
            while ((++goal_position) < nr_of_locations()) {
                if (connected(prev_location, goal_position))
                    break;
            }
            if (goal_position < nr_of_locations()) {
                // If one is found, the movement update is done
                set(action_iterator, drive_goal_index(update_movement_of_truck), goal_position + 1); // 0 is no movement
                break;
            } else {
                // Else, reset the movement of this truck, and try updating another trucks movement
                reset(action_iterator, drive_goal_index(update_movement_of_truck)); // 0 is no movement
            }
        }

    }

    return actions;
}



std::tuple<int, double> Model::get_binomial_sample_with_prob(
        const int n, const double p,
        std::vector<std::pair<int,int>>* &decision_outcomes,
        size_t &decision_point,
        std::mt19937& rng
) {
    assert (0.0 <= p and p <= 1.0);
    assert (n >= 0);

    if (n == 0) return { 0, 1.0 };

    int res;
    if (decision_outcomes != nullptr) {
        res = getDecisionPoint(decision_point, 0, n, decision_outcomes);
    } else {
        res = std::binomial_distribution(n, p)(rng);
    }

    return { res, get_binomial_probability(n, p, res) };
}



std::pair<std::vector<double>,double> Model::applyAction_(ABS::Gamestate* uncasted_state, int action, std::mt19937& rng, std::vector<std::pair<int,int>>* decision_outcomes) {
    auto* state = dynamic_cast<PIZZA_DELIVERY::Gamestate*>(uncasted_state);
    assert (!!state);
    assert(action >= 0);

    const std::vector decoded_action = decodeAction(action);

    //std::cout << action << " ="; for (const auto action_part : decoded_action) std::cout << " " << action_part; std::cout << std::endl;

    // check, that the action is valid
    int total_pizzas_to_load = 0;
    for (int truck = 0; truck < nr_of_trucks(); truck++) {
        // trucks are only allowed to drive to connected locations
        assert (decoded_action[drive_goal_index(truck)] == -1 or connected(state->trucks[truck].current_location, decoded_action[drive_goal_index(truck)]));
        // trucks can only deliver pizzas, that are (currently) hot
        assert (0 <= decoded_action[deliver_nr_index(truck)] and decoded_action[deliver_nr_index(truck)] <= state->trucks[truck].hot_pizzas_loaded);
        // trucks can only load pizzas from the shop
        assert (decoded_action[load_nr_index(truck)] == 0 or state->trucks[truck].current_location == location_of_shop);
        // trucks can only load as many pizzas as they currently have space left
        assert (0 <= decoded_action[load_nr_index(truck)] and decoded_action[load_nr_index(truck)] <= capacity_of_truck[truck] - state->trucks[truck].hot_pizzas_loaded - state->trucks[truck].cold_pizzas_loaded);
        // trucks can only drive or do a combination of other actions
        assert (decoded_action[drive_goal_index(truck)] == -1 or decoded_action[deliver_nr_index(truck)] + decoded_action[load_nr_index(truck)] == 0);
        total_pizzas_to_load += decoded_action[load_nr_index(truck)];
    }
    // Trucks cant load more numbers in total than there are pizzas at the shop
    assert (total_pizzas_to_load <= state->nr_of_pizzas_at_shop);


    // calculate reward on pre-state:
    double reward = 0.0;
    for (int l = 0; l < nr_of_locations(); l++) {
        reward -= orders_at_location[l];
        if (state->locations[l].total_delivered_pizzas() <= orders_at_location[l])
            reward += state->locations[l].delivered_hot_pizzas;
    }


    // Update state, calculate probability for outcome:
    double probability = 1.0;
    size_t decision_point = 0;

    for (int truck = 0; truck < nr_of_trucks(); truck++) {
        if (state->trucks[truck].current_location == location_of_shop and decoded_action[drive_goal_index(truck)] == -1) {
            if (rewarm_pizzas_at_shop) // in rewarm mode, cold pizzas are not discarded, but added back as warm pizzas at the shop:
                state->nr_of_pizzas_at_shop += state->trucks[truck].cold_pizzas_loaded;
            state->trucks[truck].cold_pizzas_loaded = 0;
        }
    }


    // Try to deliver currently hot pizzas while they might cool down:
    for (int truck = 0; truck < nr_of_trucks(); truck++) {
        // probability for cooling down only depends on hot pizzas in the truck at beginning of time step
        const int previously_hot_pizzas = state->trucks[truck].hot_pizzas_loaded;
        const double probability_for_staying_hot = previously_hot_pizzas / (1.0 + previously_hot_pizzas);

        // Deliver pizzas:
        const int deliver_nr = decoded_action[deliver_nr_index(truck)];
        state->trucks[truck].hot_pizzas_loaded -= deliver_nr;
        auto [hot_delivered_pizzas, delivery_outcome_prob] = get_binomial_sample_with_prob(
            deliver_nr, probability_for_staying_hot,
            decision_outcomes, decision_point, rng
        );
        probability *= delivery_outcome_prob;
        state->locations[state->trucks[truck].current_location].delivered_hot_pizzas += hot_delivered_pizzas;
        state->locations[state->trucks[truck].current_location].delivered_cold_pizzas += deliver_nr - hot_delivered_pizzas;

        // Cool down pizzas in truck:
        auto [cooled_down_pizzas, continued_transport_outcome_prob] = get_binomial_sample_with_prob(
            state->trucks[truck].hot_pizzas_loaded, 1 - probability_for_staying_hot,
            decision_outcomes, decision_point, rng
        );
        probability *= continued_transport_outcome_prob;
        state->trucks[truck].hot_pizzas_loaded -= cooled_down_pizzas;
        state->trucks[truck].cold_pizzas_loaded += cooled_down_pizzas;
    }


    // Load hot pizzas from shop:
    for (int truck = 0; truck < nr_of_trucks(); truck++) {
        const int load_nr = decoded_action[load_nr_index(truck)];
        state->nr_of_pizzas_at_shop -= load_nr;
        state->trucks[truck].hot_pizzas_loaded += load_nr;
    }

    // Check for trucks attempting to use the same connection:
    std::map<int, int> single_trucks_same_movement_count = {};
    for (int truck = 0; truck < nr_of_trucks(); truck++) {
        const int goal_location = decoded_action[drive_goal_index(truck)];
        if (goal_location == -1) continue;
        single_trucks_same_movement_count[
            nr_of_locations() * state->trucks[truck].current_location + goal_location
        ]++;
    }
    // Select trucks to make their move:
    std::vector make_next(nr_of_trucks(), false);
    for (int truck = 0; truck < nr_of_trucks(); truck++) {
        const int goal_location = decoded_action[drive_goal_index(truck)];
        if (goal_location == -1) continue; // Trucks don't move if they don't want to
        if (single_trucks_same_movement_count[
            nr_of_locations() * state->trucks[truck].current_location + goal_location
        ] != 1) continue; // If two or more trucks try to move along the same connection, they can't
        if (decision_outcomes != nullptr) {
            make_next[truck] = getDecisionPoint(decision_point, 0, 1, decision_outcomes);
        } else {
            make_next[truck] = std::bernoulli_distribution(drive_prob_of_truck[truck])(rng);
        }
        probability *= make_next[truck]? drive_prob_of_truck[truck] : 1 - drive_prob_of_truck[truck];
    }
    // Move the selected trucks:
    for (int truck = 0; truck < nr_of_trucks(); truck++) if (make_next[truck]) {
        const int goal_location = decoded_action[drive_goal_index(truck)];
        state->trucks[truck].current_location = goal_location;
    }



    return {{reward}, probability};
}



double Model::getMinV(int steps) const {
    return steps * -std::reduce(orders_at_location.begin(), orders_at_location.end());
}

double Model::getMaxV(int steps) const {
    return 0.0;
}



std::vector<int> Model::obsShape() const {
    return {nr_of_locations(), 1 + nr_of_trucks(), 2};
}

void Model::getObs(ABS::Gamestate* uncasted_state, int* obs) {
    const Gamestate* casted_state = dynamic_cast<PIZZA_DELIVERY::Gamestate*>(uncasted_state);
    assert (casted_state != nullptr);
    size_t write_index = 0;
    for (int location = 0; location < nr_of_locations(); location++) {
        obs[write_index++] = casted_state->locations[location].delivered_hot_pizzas;
        obs[write_index++] = casted_state->locations[location].delivered_cold_pizzas;
        for (int truck = 0; truck < nr_of_trucks(); truck++) {
            if (casted_state->trucks[truck].current_location == location) {
                obs[write_index++] = casted_state->trucks[truck].hot_pizzas_loaded;
                obs[write_index++] = casted_state->trucks[truck].cold_pizzas_loaded;
            } else {
                obs[write_index++] = -1; obs[write_index++] = -1;
            }
        }
    }
}
