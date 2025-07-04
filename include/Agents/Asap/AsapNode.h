#pragma once

#ifndef AsapNODE_H
#define AsapNODE_H
#include <map>
#include <random>
#include <unordered_set>
#include <vector>

#include "../../Arena.h"

namespace Asap
{
    struct GSHash {
        size_t operator()(const ABS::Gamestate* p) const {
            return p==nullptr? -1 : p->hash();
        }
    };

    struct GSCompare {
        bool operator()(const ABS::Gamestate* lhs, const ABS::Gamestate* rhs) const {
            return (lhs == nullptr && rhs == nullptr) || (lhs != nullptr && rhs != nullptr && *lhs == *rhs);
        }
    };

    class AsapNode;

    template<class T>
    using gsToNodeMap = std::unordered_map<ABS::Gamestate*, T, GSHash, GSCompare>;

    inline long global_id = 0;


    class AsapNode;

    struct AbstractNode {
        std::set<AsapNode*> ground_nodes;
        AsapNode* representant = nullptr;
    };

    struct AbstractQNode {
        std::set<std::pair<AsapNode*,int>> ground_q_nodes;
        std::pair<AsapNode*,int> representant = {nullptr,-1};
        double value;
        int visits;
    };

    class AsapNode
    {

    public:
        AsapNode(
            ABS::Model* model,
            ABS::Gamestate* state,
            int depth,
            std::mt19937& rng
        );

        int popUntriedAction();

        void addVisit();
        void addActionVisit(int action);
        void addActionValue(int action, double value);

        [[nodiscard]] double getActionValue(int action);
        [[nodiscard]] double getAbstractActionValue(int action);
        [[nodiscard]] int getAbstractActionVisits(int action);

        [[nodiscard]] ABS::Model* getModel() const;
        [[nodiscard]] ABS::Gamestate* getStateCopy() const;
        [[nodiscard]] const ABS::Gamestate* getState() const;
        [[nodiscard]] std::map<int, gsToNodeMap<AsapNode*>>* getChildren();
        [[nodiscard]] int getPlayer() const;

        [[nodiscard]] bool isAbstracted(int action);
        [[nodiscard]] int getDepth() const;
        [[nodiscard]] int getVisits() const;
        [[nodiscard]] int getActionVisits(int action);
        [[nodiscard]] bool isFullyExpanded() const;
        [[nodiscard]] bool isTerminal() const;
        [[nodiscard]] std::vector<int>* getTriedActions();

        ~AsapNode() = default;

        long id = Asap::global_id++;

        //For asap
        std::map<int,double> immediate_action_rewards;
        std::map<int,gsToNodeMap<double>> transition_probs;
        AbstractNode* abstract_node = nullptr;
        std::map<int, AbstractQNode*> abstract_q_nodes;
        std::map<int, gsToNodeMap<int>> children_sample_counts; //If case we only allow using an empirical model (action,outcome_state) -> count

        std::map<std::pair<AsapNode*,int>,double> parents_transition_probs;
        std::map<std::pair<AsapNode*,int>,int> visits_per_parent;

    private:
        // Model related stats
        ABS::Model* model;
        ABS::Gamestate* state;
        std::map<int, gsToNodeMap<AsapNode*>> children;

        // Standard Mcts stats
        int depth;
        int visits;
        std::map<int, int> action_visits;
        std::map<int, double> action_values;
        std::vector<int> tried_actions;
        std::vector<int> untried_actions;
    };

}

#endif
