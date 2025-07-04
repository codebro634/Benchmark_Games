#pragma once

#ifndef PARSSNODE_H
#define PARSSNODE_H
#include "GroundNode.h"
#include <random>
#include <vector>
#endif

namespace PARSS {

    struct ParssBudget{
        int amount;
        std::string quantity;
    };

    struct ParssArgs {
        ParssBudget budget;
        int C;
        int depth;
        double init_group_ratio = 0.0;
        std::string abstract_action_space = "intersection"; //intersection, union
        std::string abstract_terminality = "union";
        std::string select_strategy = "bfs"; //bfs,variance
        bool clip_bounds = true; //false in PARSS pseudocode
        bool allow_refine_with_optimal_parent = false; //true in PARSS pseudocode
        double discount=1.0;
        bool test_mode=false;
        bool prune_closed_nodes = false; //trick to save memory
        bool empirical_sampling = false; //true im Paper
        std::string upsample_unexpanded_policy = "keep"; //keep im Paper [keep,leaf]
        bool zero_at_leaf = true; //false im Paper, if true take env-dependent heuristics value
        bool separate_terminals = true; //true im Paper
    };

    class ParssNode
    {
    public:
        ParssNode(ParssNode* parent, const gnMap<double>& ground_states, ABS::Model* model, ParssArgs* args);
        ParssNode(ParssNode* tosplit, ParssNode* parent, const gnSet& ground_states_to_keep, ABS::Model* model, std::mt19937& rng);
        ~ParssNode();
        void delete_gamestates(bool recursively=true);

        //Getter and Setter
        [[nodiscard]] bool isSSTreeLeaf() const {return terminal || actions.empty() || args->depth == ss_depth;}
        [[nodiscard]] bool isExpanded() const {return expanded;}
        [[nodiscard]] double getReward(int action) const;
        [[nodiscard]] double getHeuristicalVForLeaf(ABS::Model *model) const;
        [[nodiscard]] double getUpperBound() const {return ub;}
        [[nodiscard]] int getDepth() const {return ss_depth;}
        void setUpperBound(double ub) {this->ub = ub;}
        [[nodiscard]] double getLowerBound() const {return lb;}
        void setLowerBound(double lb) {this->lb = lb;}
        int getNumGroundSuccessors(int action);
        [[nodiscard]] ParssNode* getParent() const {return parent;}
        [[nodiscard]] int getNumGroundStates() const {return ground_states.size();}
        [[nodiscard]] double getVBound(ABS::Model* model, bool lower, bool after_action) const;

        //Utilities
        void expand(ABS::Model* model, std::mt19937& rng);
        void expand(int action, ABS::Model* model, bool override_emp_sampling, std::mt19937& rng);
        void sample(ABS::Model* model, int abstract_action, bool override_emp_sampling, std::mt19937& rng);
        std::tuple<int,int,int> split(ABS::Model* model, std::mt19937& rng);
        [[nodiscard]] std::string toString(int spaces=0);

        //For debugging
        void test_bounds(ABS::Model *model);
        void test_succ_numbers();
        void test_split();
        void test_empty_successors();
        int size();

        std::vector<int> actions;
        std::map<int,double> actions_lb, actions_ub;
        std::map<int,std::vector<ParssNode*>> successors; //first index: action, second index: successor
        std::map<int,ParssNode*> terminal_succ; //first index: action, second index: terminal successor state (only used when abstract_terminality is 'separate')
        gnMap<double> ground_states; //constituent ground states and their sample-probabilities

    private:
        /*
         * In case another attribute is added, make sure it is copied in the split constructor!
         */
        ParssNode* parent;
        double lb{},ub{};
        bool expanded{};
        bool terminal{};
        int ss_depth{};
        int remaining_episode_steps{};
        ParssArgs* args;
        bool freed_gamestates = false;

        void update(ABS::Model * model); //updates reward and actions based on ground states


    };
}
