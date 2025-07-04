#include "../../../include/Agents/Parss/ParssAgent.h"

#include <cassert>
#include <chrono>
#include <set>
#include <unordered_set>

#include "../../../include/Agents/Parss/ParssNode.h"

using namespace PARSS;

int ParssAgent::getAction(ABS::Model* model, ABS::Gamestate* state, std::mt19937& rng) {
    assert (model->getNumPlayers() == 1); //PARSS only supports single player games;
    assert (!args.prune_closed_nodes || args.select_strategy == "bfs");
    assert (model->hasTransitionProbs());
    assert (dynamic_cast<FINITEH::Model*>(model) && dynamic_cast<FINITEH::Gamestate*>(state));

    //Statistics
    int refines = 0;
    const auto start = std::chrono::high_resolution_clock::now();
    const int total_forward_calls_before = model->getForwardCalls();

    //Visit and refine until convergence
    auto* root_ground = new GroundNode(args.abstract_action_space, model, model->copyState(state),nullptr,0,1);
    auto* root = new ParssNode(nullptr,{{root_ground,1}},model,&args);
    while( (args.budget.quantity == "refines" && refines < args.budget.amount)
        || (args.budget.quantity == "forward_calls" && model->getForwardCalls() - total_forward_calls_before < args.budget.amount)
        ||  (args.budget.quantity == "milliseconds" && std::chrono::duration<double, std::milli>(
            std::chrono::high_resolution_clock::now() - start).count() < args.budget.amount)) {

        //Run AFSSS till convergence
        AFSSS(root, model, rng);

        //Refine tree
        auto next_refine = nextRefine(model, root, rng);
        if(next_refine == nullptr) {
            break;
        }
        auto action_and_split_indices = next_refine->split(model, rng);
        updateTree( next_refine->getParent(), action_and_split_indices, model, rng);
        delete next_refine;
        refines++;

        if(args.prune_closed_nodes)
            deleteStatesOfClosedNodes(root);

        if(args.test_mode) {
            root->test_empty_successors();
            root->test_bounds(model);
            root->test_succ_numbers();
        }
    }
    int action = AFSSS(root, model, rng); //Run AFSSS till convergence after final refinement

    //Cleanup pointers
    root->delete_gamestates(); //deletes all gamestates first as they may be referenced by multiple nodes
    delete root_ground; //delete the ground search tree
    delete root; //delete the abstract search tree

    return action;
}

int ParssAgent::AFSSS(ParssNode* node,ABS::Model* model, std::mt19937& rng) {
    auto res = visit(node, model, rng);
    while(!res.first) {
        res = visit(node, model, rng);
    }
    return res.second;
}

std::pair<bool,int> ParssAgent::visit(ParssNode* node,ABS::Model* model, std::mt19937& rng) {
    if (node->isSSTreeLeaf()) {
        double val = node->getHeuristicalVForLeaf(model);
        node->setLowerBound(val);
        node->setUpperBound(val);
        return {true,-1};
    }else {
        bool converged = false;
        if(!node->isExpanded())
            node->expand(model, rng);

        //Choose action
        std::uniform_real_distribution<double> dist(-1.0, 1.0);
        int action=-42, best_action_lb = -42;
        double noisy_max_ub = std::numeric_limits<double>::lowest(), max_lb = std::numeric_limits<double>::lowest(), max_lb_up = std::numeric_limits<double>::lowest();
        for (int i : node->actions) {
            double noisy_up = node->actions_ub[i] + TIEBREAKER_NOISE * dist(rng);
            if (noisy_up > noisy_max_ub) {
                noisy_max_ub = noisy_up;
                action = i;
            }
            double lb = node->actions_lb[i];
            if (lb > max_lb || (lb == max_lb and noisy_up > max_lb_up)) {
                max_lb = lb;
                max_lb_up = noisy_up;
                best_action_lb = i;
            }
        }
        assert (action != -42);

        //Test convergence
        double max = std::numeric_limits<double>::lowest();
        for (int i : node->actions) {
            if (node->actions_ub[i] > max && i != best_action_lb)
                max = node->actions_ub[i];
        }
        if (node->actions_lb[best_action_lb] >= max) {
            converged = true;
        }

        //Choose highest uncertainty successor
        ParssNode* next = nullptr;
        double max_uncertainty = std::numeric_limits<double>::lowest();
        for (auto& succ : node->successors[action]) {
            double uncertainty = succ->getUpperBound() - succ->getLowerBound() + TIEBREAKER_NOISE * dist(rng);
            if (uncertainty > max_uncertainty) {
                max_uncertainty = uncertainty;
                next = succ;
            }
        }

        //Visit chosen succesor and update search tree
        visit(next, model, rng);
        backup(model, node,action);
        backup(node);
        return {converged,best_action_lb}; //not converged
    }
}


void ParssAgent::backup(ABS::Model* model, ParssNode* node, int action){

    //Update abstract bounds
    double sum_lower = 0, sum_upper = 0;
    int n = 0;
    for (auto& succ : node->successors[action]) {
        n += succ->getNumGroundStates();
        sum_lower += succ->getLowerBound() * succ->getNumGroundStates();
        sum_upper += succ->getUpperBound() * succ->getNumGroundStates();
    }
    node->actions_lb[action] = node->getReward(action) + args.discount * sum_lower / (double)n;
    node->actions_ub[action] = node->getReward(action) + args.discount * sum_upper / (double)n;
    if(args.clip_bounds) {
        node->actions_lb[action] = std::max(node->actions_lb[action],node->getVBound(model, true,true));
        node->actions_ub[action] = std::min(node->actions_ub[action],node->getVBound(model, false,true));
    }

    //Abstract ground q-values
    for(auto& gs : node->ground_states) {
        if(!gs.first->sample_indices.contains(action))
            continue;
        double val_succ = 0;
        double r_immediate = 0;
        double psum = 0;
        for(auto &gs_succ : gs.first->sample_indices[action]) {
            val_succ += gs_succ.second->val * gs_succ.second->trans_prob;
            r_immediate += gs_succ.second->reward * gs_succ.second->trans_prob;
            psum += gs_succ.second->trans_prob;
        }
        val_succ /= psum;
        r_immediate /= psum;
        gs.first->q_vals[action] = r_immediate + args.discount * val_succ;
    }
}

void ParssAgent::backup(ParssNode* node) {
    assert (node->isExpanded());

    //Update abstract values
    double max = std::numeric_limits<double>::lowest();
    for (int i : node->actions) {
        if (node->actions_lb[i] > max)
            max = node->actions_lb[i];
    }
    node->setLowerBound(max);
    max = std::numeric_limits<double>::lowest();
    for (int i : node->actions) {
        if (node->actions_ub[i] > max)
            max = node->actions_ub[i];
    }
    node->setUpperBound(max);

    //Update ground values
    for(auto& gs : node->ground_states) {
        max = std::numeric_limits<double>::lowest();
        if(!gs.first->q_vals.empty())
            assert (!gs.first->state->terminal);
        for(auto& pair : gs.first->q_vals) {
            if(pair.second > max)
                max = pair.second;
        }
        gs.first->val = max;
    }
}

void ParssAgent::enforceSparseSamplingProperty(ParssNode* node, ABS::Model* model, std::mt19937& rng) {

    if(node->isExpanded()){
        for(int a : node->actions) {
            if(!node->successors.contains(a)) //this may occur after a split, when an already expanded new gets new available actions
                node->expand(a, model,true, rng);
            else
                node->sample(model, a, true, rng);
            for(auto succ : node->successors[a])
                enforceSparseSamplingProperty(succ, model, rng);
            backup(model, node,a);
        }
        backup(node);
    }else{
        if(args.upsample_unexpanded_policy == "leaf") {
            double val = node->getHeuristicalVForLeaf(model);
            node->setLowerBound(val);
            node->setUpperBound(val);
        }else if(args.upsample_unexpanded_policy == "keep") {
            //do nothing
        }else {
            throw std::runtime_error("Unknown upsample leaf policy.");
        }
    }

}

void ParssAgent::updateTree(ParssNode *node, std::tuple<int,int,int>& action_and_split_indices, ABS::Model* model, std::mt19937& rng) {

    //Sample missing states down-tree
    enforceSparseSamplingProperty(node->successors[std::get<0>(action_and_split_indices)][std::get<1>(action_and_split_indices)], model, rng);
    enforceSparseSamplingProperty(node->successors[std::get<0>(action_and_split_indices)][std::get<2>(action_and_split_indices)], model, rng);
    //Propagate new bounds up-tree
    while(node != nullptr) {
        for(int a : node->actions) {
            backup(model, node,a);
        }
        backup(node);
        node = node->getParent();
    }

}


ParssNode* ParssAgent::nextRefine(ABS::Model* model, ParssNode* root, std::mt19937& rng) const {

    std::vector<std::pair<ParssNode*,int>> condition_nodes_actions = {};

    std::set<int> untried_actions = {};
    for (int a : root->actions)
        untried_actions.insert(a);

    //Randomly sample root action from which to descent the tree
    while(!untried_actions.empty() && condition_nodes_actions.empty()) {
        std::uniform_int_distribution<int> dist(0, untried_actions.size() - 1);
        auto it = untried_actions.begin();
        std::advance(it, dist(rng));
        int root_action = *it;
        untried_actions.erase(root_action);

        //Do breadth first search until we find a non-fully refined node with ub < args.vmax
        std::vector<std::pair<ParssNode*,int>> q = {{root,root_action}};
        std::vector<std::pair<ParssNode*,int>> next_q = {};

        while(!q.empty()) {
            for(auto [node,action] : q) {
                if(!args.allow_refine_with_optimal_parent && node->actions_ub[action] >= node->getVBound(model, false,true)) {
                    //no increase can be gained from this node or any of its successors
                    continue;
                }
                bool permissible = node->actions_ub[action] < node->getVBound(model, false,true) && static_cast<int>(node->successors[action].size()) < node->getNumGroundSuccessors(action);
                if(permissible) {
                    condition_nodes_actions.emplace_back(node,action);
                }
                if(!permissible || args.select_strategy != "bfs" ) {
                    for(auto succ : node->successors[action]) {
                        if(!succ->isExpanded())
                            continue;
                        for(int succ_a : succ->actions)
                            next_q.emplace_back(succ,succ_a);
                    }
                }
            }
            q = next_q;
            next_q = {};
        }

    }

    ParssNode* to_refine = nullptr;

    if(!condition_nodes_actions.empty()) {

        if(args.select_strategy == "bfs") {
            //randomly sample from condition nodes
            std::uniform_int_distribution<int> dist_cond(0, condition_nodes_actions.size() - 1);
            auto [node,action] = condition_nodes_actions[dist_cond(rng)];

            //select biggest successor
            std::uniform_real_distribution<double> dist(-1.0, 1.0);
            double max_size = 0;
            int biggest_idx = -1;
            for(size_t i = 0; i < node->successors[action].size(); i++) {
                double size = node->successors[action][i]->ground_states.size() + 0.1 * dist(rng); //noise to ensure random choice in case of equal sizes
                if(size > max_size) {
                    max_size = size;
                    biggest_idx = i;
                }
            }
            assert (biggest_idx != -1);
            to_refine = node->successors[action][biggest_idx];
        }else if(args.select_strategy == "variance") {

            //gather all condition nodes
            auto condition_nodes = std::vector<ParssNode*>();
            for(auto [node,action] : condition_nodes_actions) {
                for(auto succ : node->successors[action]) {
                    condition_nodes.push_back(succ);
                }
            }

            //select node with highest variance of q-values
            std::uniform_real_distribution<double> dist(-1.0, 1.0);
            double highest_var = std::numeric_limits<double>::lowest();
            for(auto node : condition_nodes) {
                double avg_var = 0;
                for(int action : node->actions) {
                    double avg_q = 0;
                    for(auto& gs : node->ground_states)
                        avg_q += gs.second * gs.first->q_vals[action];
                    double var_a = 0;
                    for(auto& gs : node->ground_states)
                        var_a += gs.second * (gs.first->q_vals[action] - avg_q) * (gs.first->q_vals[action] - avg_q);
                    avg_var += var_a;
                }
                avg_var /= node->ground_states.size();
                avg_var += TIEBREAKER_NOISE * dist(rng);
                if(avg_var > highest_var) {
                    highest_var = avg_var;
                    to_refine = node;
                }
            }

        }else {
            throw std::runtime_error("Unknown select strategy:" + args.select_strategy);
        }
    }

    return to_refine;

}

void ParssAgent::deleteStatesOfClosedNodes(ParssNode* node) {

    //A node is closed iff node is expanded or an SS-tree leaf and all predecessors are closed too
    if( (node->isExpanded() || node->isSSTreeLeaf()) && node->getNumGroundStates() == 1) {
        node->delete_gamestates(false);
        for(auto& pair : node->successors) {
            for(auto succ : pair.second) {
                deleteStatesOfClosedNodes(succ);
            }
        }
    }

}