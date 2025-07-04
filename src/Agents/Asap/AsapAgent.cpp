#include "../../../include/Agents/Asap/AsapAgent.h"

#include <iostream>
#include <cassert>
#include <cmath>
#include <chrono>
#include <queue>
#include <fstream>
#include <ranges>
#include <algorithm>

using namespace Asap;

AsapAgent::AsapAgent(const AsapArgs& args):
    exploration_parameters(args.exploration_parameters),
    discount(args.discount),
    budget(args.budget),
    num_rollouts(args.num_rollouts),
    dynamic_exploration_factor(args.dynamic_exploration_factor),
    rollout_length(args.rollout_length),
    group_partially_expanded_states(args.group_partially_expanded_states),
    epsilon_a(args.epsilon_a),
    epsilon_t(args.epsilon_t),
    l(args.l),
    empirical_model(args.empirical_model),
    framework(args.framework),
    min_group_visits(args.min_group_visits),
    top_n_matches(args.top_n_matches)
{}

double progress(AsapSearchStats& search_stats) {
    if(search_stats.budget.quantity == "iterations")
        return (double) search_stats.completed_iterations / search_stats.budget.amount;
    else if(search_stats.budget.quantity == "forward_calls")
        return (double) search_stats.total_forward_calls / search_stats.budget.amount;
    else if (search_stats.budget.quantity == "milliseconds")
        return (std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - search_stats.start_time).count()) / search_stats.budget.amount;
    else
        throw std::runtime_error("Invalid budget quantity");
}

void AsapAgent::printAbstraction(ABS::Model* model, std::map<int,std::vector<AbstractQNode*>> &abstract_q_nodes, std::map<int,std::vector<AbstractNode*>>& abstract_nodes) {
    std::cout << "-------------------"  << std::endl;
    for(auto& [depth, absNs] : abstract_nodes) {
        std::cout << "Abs Nodes at Depth: " << depth << std::endl;
        for(auto absN : absNs) {
            std::cout << "[" ;
            for(auto node : absN->ground_nodes) {
                std::cout << " " << node << ", ";
                //if(absN->ground_nodes.size() > 1)
                //    model->printState(node->getStateCopy());
            }
            std::cout << "], ";
        }
        std::cout << std::endl;
    }
    std::cout << std::endl;
    for(auto& [depth, absQs] : abstract_q_nodes) {
        std::cout << "Abs Q nodes at Depth: " << depth << std::endl;
        for(auto absQ : absQs) {
            std::cout << "[" ;
            for(auto& [node, action] : absQ->ground_q_nodes) {
                std::cout << " (" << node << ", " << action << "), ";
                //if(absQ->ground_q_nodes.size() > 1)
                 //   model->printState(node->getStateCopy());
            }
            std::cout << "], ";
        }
        std::cout << std::endl;
    }
}

bool AsapAgent::areActionsApproxEquivalent(AsapNode* n1, int a1, AsapNode* n2, int a2, std::map<std::pair<AsapNode*,int>,std::map<AbstractNode*,double>>& abstract_transition_prob_map) {
    //Action reward condition
    if(std::fabs(n2->immediate_action_rewards.at(a2) - n1->immediate_action_rewards.at(a1)) > epsilon_a + 1e-6)
        return false;

    //Transition prob condition
    double trans_dist = 0;
    auto probs = abstract_transition_prob_map[{n1,a1}];
    auto probs_comp = abstract_transition_prob_map[{n2,a2}];
    std::set<AbstractNode*> union_set;
    for(auto& [abs_node, prob] : probs)
        union_set.insert(abs_node);
    for(auto& [abs_node, prob] : probs_comp)
        union_set.insert(abs_node);
    for(auto abs_node : union_set)
        trans_dist += std::fabs(probs[abs_node] - probs_comp[abs_node]);

    return trans_dist <= epsilon_t + 1e-6;
}

void AsapAgent::constructAsapActionAbstraction(AsapSearchStats &search_stats, std::map<int, std::vector<AbstractQNode *>> &abstract_q_nodes, int depth, std::map<std::pair<AsapNode*,int>,std::map<AbstractNode*,double>>& abstract_transition_prob_map) {

    std::vector<AsapNode*> defined_order_nodes;
    for(auto& [_, node] : (*search_stats.layerMap)[depth])
        defined_order_nodes.push_back(node);
    std::sort(defined_order_nodes.begin(), defined_order_nodes.end(),[](const AsapNode* lhs, const AsapNode* rhs) {return lhs->id < rhs->id; });  // Sort by ID

    for(auto& node : defined_order_nodes) {
        for(auto& [action, children] : *node->getChildren()) {

            for(auto absQ : abstract_q_nodes[depth]) {
                assert (!absQ->ground_q_nodes.empty());
                auto [cmp_node, cmp_action] = absQ->representant;
                if(areActionsApproxEquivalent(node,action,cmp_node,cmp_action,abstract_transition_prob_map)) {
                    absQ->ground_q_nodes.insert({node,action});
                    node->abstract_q_nodes[action] = absQ;
                    absQ->value += node->getActionValue(action);
                    absQ->visits += node->getActionVisits(action);
                    break;
                }
            }

            //Node becomes its own abstract state as it couldnt be matched
            if(!node->abstract_q_nodes.contains(action)) {
                auto* new_absQ = new AbstractQNode({{node,action}},{node,action},node->getActionValue(action),node->getActionVisits(action));
                node->abstract_q_nodes[action] = new_absQ;
                abstract_q_nodes[depth].push_back(new_absQ);
            }
        }
    }
}

bool AsapAgent::ASEquivalence(AsapNode* node1, AsapNode* node2, std::map<std::pair<AsapNode*,int>,std::map<AbstractNode*,double>>& abstract_transition_prob_map) {
    assert (node1->isFullyExpanded()  && node2->isFullyExpanded());

    //check for equal action sets
    if(node1->getTriedActions()->size() != node2->getTriedActions()->size())
        return false;
    std::set<int> union_set;
    for(auto action : *node1->getTriedActions())
        union_set.insert(action);
    for(auto action : *node2->getTriedActions())
        union_set.insert(action);
    if(union_set.size() != node1->getTriedActions()->size())
        return false;

    //check for pairwise equivalence
    for(auto action : *node1->getTriedActions()) {
        if(!areActionsApproxEquivalent(node1,action,node2,action,abstract_transition_prob_map)) {
            return false;
        }
    }
    return true;
}

bool AsapAgent::ASAMEquivalence(AsapNode *n1, AsapNode *n2, std::map<std::pair<AsapNode*,int>,std::map<AbstractNode*,double>>& abstract_transition_prob_map, std::map<int,int>& action_matching) {

    if(n1->getTriedActions()->size() != n2->getTriedActions()->size())
        return false;

    //check for bijection of equivalences
    std::set<int> unmatched_actions;
    for(auto action : *n2->getTriedActions())
        unmatched_actions.insert(action);
    for(auto action : *n1->getTriedActions()) {
        bool found_match = false;
        for(int unmatched : unmatched_actions) {
            if(areActionsApproxEquivalent(n1,action,n2,unmatched,abstract_transition_prob_map)) {
                unmatched_actions.erase(unmatched);
                found_match = true;
                action_matching[action] = unmatched;
                break;
            }
        }
        if(!found_match)
            return false;
    }
    return true;

}

bool AsapAgent::ASAPNodeLeq(AsapNode *n1, AsapNode *n2) {
    for(auto action : *n1->getTriedActions()) {
        bool found_match = false;
        for(int cmp_action : *n2->getTriedActions()) {
            if(n1->abstract_q_nodes.at(action) == n2->abstract_q_nodes.at(cmp_action)) {
                found_match = true;
                break;
            }
        }
        if(!found_match)
            return false;
    }
    return true;
}

//match if
bool AsapAgent::VALEquivalence(AsapNode* n1, AsapNode* n2) {
    assert (n1->isFullyExpanded() && n2->isFullyExpanded());

    if (n1->getVisits() < min_group_visits || n2->getVisits() < min_group_visits)
        return false;

    std::vector<std::pair<int,double>> top_n_actions = {};
    for(auto action : *n1->getTriedActions()) {
        double Q = n1->getActionValue(action) / (double) n1->getActionVisits(action);
        top_n_actions.emplace_back(action,Q);
    }
    std::sort(top_n_actions.begin(),top_n_actions.end(),[](const std::pair<int,double>& lhs, const std::pair<int,double>& rhs) {return lhs.second > rhs.second;});

    std::vector<std::pair<int,double>> top_n_actions_cmp = {};
    for(auto action : *n2->getTriedActions()) {
        double Q = n2->getActionValue(action) / (double) n2->getActionVisits(action);
        top_n_actions_cmp.emplace_back(action,Q);
    }
    std::sort(top_n_actions_cmp.begin(),top_n_actions_cmp.end(),[](const std::pair<int,double>& lhs, const std::pair<int,double>& rhs) {return lhs.second > rhs.second;});

    for (int i = 0; i < std::min(top_n_matches,std::min(static_cast<int>(n1->getTriedActions()->size()),static_cast<int>(n2->getTriedActions()->size()))); i++) {

        bool found_match = false;
        for(int cmp_action : *n2->getTriedActions()) {
            if(n1->abstract_q_nodes.at(top_n_actions[i].first) == n2->abstract_q_nodes.at(cmp_action)) {
                found_match = true;
                break;
            }
        }
        if(!found_match)
            return false;

        found_match = false;
        for(int cmp_action : *n1->getTriedActions()) {
            if(n2->abstract_q_nodes.at(top_n_actions_cmp[i].first) == n1->abstract_q_nodes.at(cmp_action)) {
                found_match = true;
                break;
            }
        }
        if(!found_match)
            return false;
    }

    return true;
}

void AsapAgent::constructNodeAbstraction(AsapSearchStats &search_stats, std::map<int, std::vector<AbstractNode *>> &abstract_nodes,
    std::map<int,std::vector<AbstractQNode*>> &abstract_q_nodes, int depth,
    std::map<std::pair<AsapNode*,int>,std::map<AbstractNode*,double>>& abstract_transition_prob_map) {

    AbstractNode* partially_expanded_node = nullptr;
    AbstractNode* terminal_node = nullptr;

    assert (abstract_nodes[depth].empty());
    for(auto& [state, node] : (*search_stats.layerMap)[depth]) {

        if(state->terminal) {
            if (terminal_node == nullptr) {
                terminal_node = new AbstractNode({node}, node);
                abstract_nodes[depth].push_back(terminal_node);
            }
            terminal_node->ground_nodes.insert(node);
            node->abstract_node = terminal_node;
            continue;
        }

        if(!node->isFullyExpanded()) {
            if(group_partially_expanded_states) {
                if (partially_expanded_node == nullptr) {
                    partially_expanded_node = new AbstractNode({node}, node);
                    abstract_nodes[depth].push_back(partially_expanded_node);
                }
                partially_expanded_node->ground_nodes.insert(node);
                node->abstract_node = partially_expanded_node;
            }else {
                auto* new_absN = new AbstractNode({node}, node);
                node->abstract_node = new_absN;
                abstract_nodes[depth].push_back(new_absN);
            }
            continue;
        }

        node->abstract_node = nullptr;
        for(auto absNode : abstract_nodes[depth]) {
            if (absNode == partially_expanded_node || absNode == terminal_node || !absNode->representant->isFullyExpanded())
                continue;
            assert (!absNode->ground_nodes.empty());
            bool compatible;
            auto cmp_node = absNode->representant;
            if(framework == "asam" || framework == "as") {
                std::map<int,int> action_matching;
                compatible = framework == "asam"? ASAMEquivalence(node,cmp_node,abstract_transition_prob_map,action_matching) : ASEquivalence(node,cmp_node,abstract_transition_prob_map);
                if(compatible) {
                    for(auto action : *node->getTriedActions()) {
                        int cmp_action = framework == "asam"? action_matching.at(action) : action;
                        assert (cmp_node->abstract_q_nodes.contains(cmp_action));
                        AbstractQNode* absQ = cmp_node->abstract_q_nodes[cmp_action];
                        absQ->value += node->getActionValue(action);
                        absQ->visits += node->getActionVisits(action);
                        absQ->ground_q_nodes.insert({node,action});
                        node->abstract_q_nodes[action] = absQ;
                    }
                }
            }else if(framework == "asap") {
                compatible = ASAPNodeLeq(node,cmp_node) & ASAPNodeLeq(cmp_node,node);
            } else if (framework == "val") {
                compatible = VALEquivalence(node,cmp_node);
            }
            else {
                throw std::runtime_error("Invalid framework");
            }

            if(compatible) {
                absNode->ground_nodes.insert(node);
                node->abstract_node = absNode;
                break;
            }
        }

        if(node->abstract_node == nullptr) {
            auto* new_absN = new AbstractNode({node}, node);
            node->abstract_node = new_absN;
            abstract_nodes[depth].push_back(new_absN);
            if(framework == "asam" || framework == "as") {
                for(auto action : *node->getTriedActions()) {
                    auto* new_absQ = new AbstractQNode({{node,action}},{node,action},node->getActionValue(action),node->getActionVisits(action));
                    node->abstract_q_nodes[action] = new_absQ;
                    abstract_q_nodes[depth].push_back(new_absQ);
                }
            }
        }

    }

}

void AsapAgent::constructAbstraction(AsapSearchStats& search_stats, std::map<int,std::vector<AbstractQNode*>> &abstract_q_nodes, std::map<int,std::vector<AbstractNode*>>& abstract_nodes,  std::mt19937& rng) {
    int max_depth = search_stats.layerMap->rbegin()->first;

    for(int depth = max_depth; depth >= 0; depth--) {
        //Compute transition probabilities to successor abstract nodes
        std::map<std::pair<AsapNode*,int>,std::map<AbstractNode*,double>> abstract_transition_prob_map;
        for(auto& [_, node] : (*search_stats.layerMap)[depth]) {
            for(auto& [action, children] : *node->getChildren()) {
                node->abstract_q_nodes.erase(action);
                for(auto child : children) {
                    double transition_prob = empirical_model? (child.second->visits_per_parent.at({node,action}) / (double) node->getActionVisits(action)) : child.second->parents_transition_probs.at({node,action});
                    abstract_transition_prob_map[{node,action}][child.second->abstract_node] += transition_prob;
                }
            }
        }

        //Construct abstraction
        if(framework == "asap" || framework == "val")
            constructAsapActionAbstraction(search_stats, abstract_q_nodes, depth, abstract_transition_prob_map);
        constructNodeAbstraction(search_stats, abstract_nodes, abstract_q_nodes, depth, abstract_transition_prob_map);
    }
}


void AsapAgent::cleanupTree(AsapNode* root){
    //cleanup tree
    std::vector<AsapNode*> clear_stack = {root};
    std::set<AsapNode*> to_clear_nodes = {root};
    while(!clear_stack.empty()){
        auto next = clear_stack.back();
        clear_stack.pop_back();
        for (auto& q_state : std::views::values(*next->getChildren())){
            for (const auto& child_state_node : std::views::values(q_state)){
                if(!to_clear_nodes.contains(child_state_node)){
                    clear_stack.push_back(child_state_node);
                    to_clear_nodes.insert(child_state_node);
                }
            }
        }
    }

    delete root->getState();
    for(auto* node : to_clear_nodes){
        for (auto& q_state : std::views::values(*node->getChildren())){
            for (const auto& gamestate : std::views::keys(q_state))
                delete gamestate;
        }
        delete node;
    }
}

void AsapAgent::cleanupAbstractions(std::map<int,std::vector<AbstractQNode*>> &abstract_q_nodes, std::map<int,std::vector<AbstractNode*>>& abstract_nodes) {
    for(auto& [_, absQs] : abstract_q_nodes) {
        for(auto absQ : absQs)
            delete absQ;
    }
    for(auto& [_, absNs] : abstract_nodes) {
        for(auto absN : absNs)
            delete absN;
    }
    abstract_q_nodes.clear();
    abstract_nodes.clear();
}

int AsapAgent::getAction(ABS::Model* model, ABS::Gamestate* state, std::mt19937& rng){
    assert (model->getNumPlayers() == 1 && (model->hasTransitionProbs() || empirical_model));

    //Init statistics
    auto init_state = model->copyState(state);
    auto* root = new AsapNode(model, init_state,0,  rng);
    std::map<int,gsToNodeMap<AsapNode*>> layerMap = {};
    layerMap = {{0,{{init_state,root}}}};
    AsapSearchStats search_stats = {budget,std::chrono::high_resolution_clock::now(), 0, 0, 0, &layerMap,0.0, 0.0,0,l};
    const int total_forward_calls_before = model->getForwardCalls();
    std::map<int,std::vector<AbstractQNode*>> abstract_q_nodes; //map from layer to abstract q nodes of that layer
    std::map<int,std::vector<AbstractNode*>> abstract_nodes;

    //Tree search
    while (progress(search_stats) < 1.0 - 1e-6){
        if(progress(search_stats) > (l - search_stats.remaining_l + 1)/(double) (l+1)) {
            cleanupAbstractions(abstract_q_nodes,abstract_nodes);
            constructAbstraction(search_stats,abstract_q_nodes, abstract_nodes, rng);
            //model->printState(state);
            //printAbstraction(model, abstract_q_nodes,abstract_nodes);
            search_stats.remaining_l--;
        }

        auto leaf_path = treePolicy(model, root, rng, search_stats);
        const auto reward = rollout(model, std::get<0>(leaf_path.back()), rng);
        backup(reward,leaf_path, search_stats);
        search_stats.completed_iterations++;
        search_stats.total_forward_calls = model->getForwardCalls() - total_forward_calls_before;
    }

    //Greedy action selection
    const int best_action = selectAction(root, true, rng, search_stats);

    //Free pointers
    cleanupAbstractions(abstract_q_nodes,abstract_nodes);
    cleanupTree(root);

    return best_action;
}

std::vector<std::tuple<AsapNode*,int,double>> AsapAgent::treePolicy(ABS::Model* model, AsapNode* node, std::mt19937& rng, AsapSearchStats& search_stats){
    bool reached_leaf = false;
    std::vector<std::tuple<AsapNode*,int,double>>  state_action_reward_path;
    auto old_node = node;
    while (!node->isTerminal() && !reached_leaf){
        int chosen_action;
        double reward;
        node = selectNode(model, node, reached_leaf, chosen_action, reward, rng, search_stats);
        state_action_reward_path.emplace_back(old_node,chosen_action, reward);
        old_node = node;
        if (node->getDepth() > search_stats.max_depth)
            search_stats.max_depth = node->getDepth();
    }
    state_action_reward_path.push_back({node,-1,{}});
    return state_action_reward_path;
}

AsapNode* AsapAgent::selectNode(ABS::Model* model, AsapNode* node, bool& reached_leaf, int& chosen_action, double& reward,  std::mt19937& rng, AsapSearchStats& search_stats)
{
    reached_leaf = false;
    chosen_action = node->isFullyExpanded()? selectAction(node, false, rng, search_stats) : node->popUntriedAction();
    auto sample_state = node->getStateCopy();

    // Sample successor of state-action-pair
    auto [rewards_tmp, probability] = model->applyAction(sample_state, chosen_action, rng, nullptr);
    assert (!node->immediate_action_rewards.contains(chosen_action) || node->immediate_action_rewards[chosen_action] == rewards_tmp[0]);
    reward = rewards_tmp[0];

    const auto* successors = &node->getChildren()->at(chosen_action);
    AsapNode* leaf;
    if (!node->getChildren()->at(chosen_action).contains(sample_state))
    {
        // New successor sampled
        if((*search_stats.layerMap)[node->getDepth()+1].contains(sample_state)){
            leaf = (*search_stats.layerMap)[node->getDepth()+1][sample_state];
            (*node->getChildren())[chosen_action][sample_state] = leaf;
        }else{
            leaf = new AsapNode(model, sample_state, node->getDepth() + 1, rng);
            reached_leaf = true;
            (*search_stats.layerMap)[node->getDepth()+1][sample_state] = leaf;
            (*node->getChildren())[chosen_action][sample_state] = leaf;
        }
    }else{
        leaf = successors->at(sample_state);
        assert (leaf->parents_transition_probs.at({node,chosen_action}) == probability);
        delete sample_state;
    }

    node->immediate_action_rewards[chosen_action] = rewards_tmp[0];
    leaf->parents_transition_probs[{node,chosen_action}] = probability;
    leaf->visits_per_parent[{node,chosen_action}]++;

    return leaf;
}

int AsapAgent::selectAction(AsapNode* node, bool greedy, std::mt19937& rng, AsapSearchStats& search_stats)
{
    // UCT Formula: w/n + c * sqrt(ln(N)/n)
    assert(!node->getTriedActions()->empty());

    std::uniform_real_distribution<double> dist(-1.0, 1.0);
    double best_value = -std::numeric_limits<double>::infinity();
    int best_action = -42;

    //For local std calculation
    double dynamic_exp_factor = 1;
    if(dynamic_exploration_factor){
        const double var = std::max(0.0,search_stats.total_squared_v / search_stats.global_num_vs - (search_stats.total_v / search_stats.global_num_vs) *  (search_stats.total_v / search_stats.global_num_vs));
        dynamic_exp_factor = sqrt(var);
    }

    //Get visit sum
    int total_visits = 0;
    for(const int action : *node->getTriedActions())
        total_visits += node->isAbstracted(action)? node->getAbstractActionVisits(action) : node->getActionVisits(action);

    //Test abstraction consistency
    for(const int action : *node->getTriedActions()){
        if(node->isAbstracted(action)){
            auto absQ = node->abstract_q_nodes[action];
            int absvisits = 0;
            double absval = 0;
            for(auto& [child, child_action] : absQ->ground_q_nodes){
                absvisits += child->getActionVisits(child_action);
                absval += child->getActionValue(child_action);
            }
            assert(absvisits == absQ->visits);
            assert(std::fabs(absval - absQ->value) < 1e-6);
        }
    }

    for (const int action : *node->getTriedActions()){
        const double action_value = node->isAbstracted(action)? node->getAbstractActionValue(action) : node->getActionValue(action);
        const int action_visits = node->isAbstracted(action)? node->getAbstractActionVisits(action) : node->getActionVisits(action);
        const double exploration_term = sqrt(log(total_visits) / (double)action_visits);
        const double q_value = action_value / (double)action_visits;

        double exploration_param = node->getDepth() >= static_cast<int>(exploration_parameters.size()) ? exploration_parameters.back() : exploration_parameters[node->getDepth()];
        double score;
        if(exploration_param == -1 && !greedy) //greedy overwrites uniform
            score = exploration_term;
        else
            score = q_value + (greedy? 0 : exploration_param * exploration_term * dynamic_exp_factor);
        score += TIEBREAKER_NOISE * dist(rng); //trick to efficiently break ties
        if (score > best_value){
            best_value = score;
            best_action = action;
        }
    }

    return best_action;
}

double AsapAgent::rollout(ABS::Model* model, AsapNode* node, std::mt19937& rng) const
{
    double reward_sum = 0.0;
    if (node->isTerminal())
        return reward_sum;

    for(int i = 0; i < num_rollouts; i++) {
        double total_discount = 1;
        auto* rollout_state = node->getStateCopy();
        int episode_steps = 0;
        while (!rollout_state->terminal && (rollout_length == -1 || episode_steps < rollout_length))
        {
            // Sample action
            auto available_actions = model->getActions(rollout_state);
            std::uniform_int_distribution<int> dist(0, static_cast<int>(available_actions.size()) - 1);
            const int action = available_actions[dist(rng)];

            // Apply action and get rewards
            auto [reward, outcome_and_probability] = model->applyAction(rollout_state, action,rng, nullptr);
            reward_sum += reward[0] * total_discount * (1 / (double) num_rollouts); //asserting single player
            total_discount *= discount;
            episode_steps++;
        }
        delete rollout_state;
    }

    return reward_sum;
}

void AsapAgent::backup(double value, std::vector<std::tuple<AsapNode*,int,double>>& path, AsapSearchStats& search_stats) const
{
    for (size_t i = path.size()-1; i >= 1; i--)
    {
        auto parent = std::get<0>(path[i-1]);
        auto parent_action = std::get<1>(path[i-1]);
        auto reward = std::get<2>(path[i-1]);

        value = value * discount + reward;

        parent->addVisit();
        parent->addActionVisit(parent_action);
        parent->addActionValue(parent_action, value);

        //Bookkeeping for abstractions
        if(parent->isAbstracted(parent_action)) {
            auto absQ = parent->abstract_q_nodes[parent_action];
            absQ->value += value;
            absQ->visits++;
        }

        //Bookkeeping for exploration factor
        if(parent->getActionVisits(parent_action) == 1)
            search_stats.global_num_vs++;
        if(parent->getActionVisits(parent_action) > 1) { //only remove value if it was present before
            double old_q = (parent->getActionValue(parent_action)-value) / ((double) parent->getActionVisits(parent_action)-1);
            search_stats.total_v -= old_q;
            search_stats.total_squared_v -= old_q * old_q;
        }
        double q = parent->getActionValue(parent_action) / (double) parent->getActionVisits(parent_action);
        search_stats.total_v += q;
        search_stats.total_squared_v += q*q;

    }
}
