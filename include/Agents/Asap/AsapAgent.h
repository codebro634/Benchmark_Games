#pragma once

#ifndef AsapAGENT_H
#define AsapAGENT_H
#include <chrono>
#include "../Agent.h"
#include "AsapNode.h"
#endif


namespace Asap
{
    struct AsapBudget{
        int amount;
        std::string quantity;
    };

    struct AsapSearchStats{
        AsapBudget budget;
        std::chrono::time_point<std::chrono::high_resolution_clock> start_time;
        int completed_iterations{};
        int total_forward_calls{};
        int max_depth{};

        std::map<int,gsToNodeMap<AsapNode*>>* layerMap;

        //For global std exploration factor
        double total_squared_v;
        double total_v;
        int global_num_vs = 0;

        //for asap
        int remaining_l;
    };

    struct AsapArgs
    {
        AsapBudget budget;
        std::vector<double> exploration_parameters = {2};
        double discount = 1.0;
        int num_rollouts = 1;
        int rollout_length = -1;
        bool dynamic_exploration_factor = true;

        bool group_partially_expanded_states = true;
        double epsilon_a = 0;
        double epsilon_t = 0;
        int l = 1;
        bool empirical_model = false;
        std::string framework = "asap"; //val,asap,asam,as

        //params for val framework only
        int min_group_visits = 0;
        int top_n_matches = 1;

    };

    class AsapAgent final : public Agent
    {
    public:
        explicit AsapAgent(const AsapArgs& args);
        int getAction(ABS::Model* model, ABS::Gamestate* state, std::mt19937& rng) override;

    private:
        std::vector<std::tuple<AsapNode*,int,double>> treePolicy(ABS::Model* model, AsapNode* node, std::mt19937& rng, AsapSearchStats& search_stats);
        AsapNode* selectNode(ABS::Model* model, AsapNode* node, bool& reached_leaf, int &chosen_action, double& reward, std::mt19937& rng, AsapSearchStats& search_stats);
        int selectAction(AsapNode* node, bool greedy, std::mt19937& rng, AsapSearchStats& search_stats);
        double rollout(ABS::Model* model, AsapNode* node, std::mt19937& rng) const;
        void backup(double value, std::vector<std::tuple<AsapNode*,int,double>>& path, AsapSearchStats& search_stats) const;
        bool areActionsApproxEquivalent(AsapNode* n1, int a1, AsapNode* n2, int a2, std::map<std::pair<AsapNode*,int>,std::map<AbstractNode*,double>>& abstract_transition_prob_map);
        void constructAsapActionAbstraction(AsapSearchStats &search_stats, std::map<int, std::vector<AbstractQNode *>> &abstract_q_nodes, int depth, std::map<std::pair<AsapNode*,int>,std::map<AbstractNode*,double>>& abstract_transition_prob_map);
        void constructNodeAbstraction(AsapSearchStats &search_stats, std::map<int, std::vector<AbstractNode *>> &abstract_nodes, std::map<int,std::vector<AbstractQNode*>> &abstract_q_nodes, int depth, std::map<std::pair<AsapNode*,int>,std::map<AbstractNode*,double>>& abstract_transition_prob_map);
        void constructAbstraction(AsapSearchStats& search_stats, std::map<int,std::vector<AbstractQNode*>> &abstract_q_nodes, std::map<int,std::vector<AbstractNode*>>& abstract_nodes,  std::mt19937& rng);
        void cleanupAbstractions(std::map<int,std::vector<AbstractQNode*>> &abstract_q_nodes, std::map<int,std::vector<AbstractNode*>>& abstract_nodes);
        void cleanupTree(AsapNode* root);
        void printAbstraction(ABS::Model* model, std::map<int,std::vector<AbstractQNode*>> &abstract_q_nodes, std::map<int,std::vector<AbstractNode*>>& abstract_nodes);

        bool ASEquivalence(AsapNode* n1, AsapNode* n2, std::map<std::pair<AsapNode*,int>,std::map<AbstractNode*,double>>& abstract_transition_prob_map);
        bool ASAMEquivalence(AsapNode* n1, AsapNode* n2, std::map<std::pair<AsapNode*,int>,std::map<AbstractNode*,double>>& abstract_transition_prob_map, std::map<int,int>& action_matching);
        bool ASAPNodeLeq(AsapNode* n1, AsapNode* n2);

        bool VALEquivalence(AsapNode* n1, AsapNode* n2);

        std::vector<double> exploration_parameters;
        double discount;
        AsapBudget budget;
        int num_rollouts;
        bool dynamic_exploration_factor;
        int rollout_length;

        //abstraction parameters
        bool group_partially_expanded_states;
        double epsilon_a, epsilon_t;
        int l;
        bool empirical_model;
        std::string framework;

        //param for val framework only
        int min_group_visits;
        int top_n_matches;

        constexpr static double TIEBREAKER_NOISE = 1e-6;
    };

}
