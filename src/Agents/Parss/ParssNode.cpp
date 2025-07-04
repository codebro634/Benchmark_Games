#include "../../../include/Agents/Parss/ParssNode.h"

#include <algorithm>
#include <cassert>
#include <set>
#include <map>
#include <cmath>
#include <queue>
#include "../../../include/Games/MDPs/Saving.h"

using namespace PARSS;

ParssNode::ParssNode(ParssNode* parent, const gnMap<double>& ground_states, ABS::Model* model, ParssArgs* args) {
    this->ground_states = ground_states;
    this->parent = parent;
    this->expanded = false;
    this->args = args;

    //depth
    if(parent != nullptr) {
        ss_depth = parent->ss_depth + 1;
        remaining_episode_steps = parent->remaining_episode_steps - 1;
    }
    else {
        assert (ground_states.size() == 1);
        ss_depth = 0;
        remaining_episode_steps =  static_cast<int>(dynamic_cast<FINITEH::Gamestate *>(ground_states.begin()->first->state)->remaining_steps);
    }

    //Bounds (important that this is done after setting remaining_game_steps and updating model)
    lb = getVBound(model,true,false);
    ub = getVBound(model, false,false);

    //Calculate reward and actions
    update(model);

}

ParssNode::ParssNode(ParssNode* tosplit, ParssNode* parent, const  gnSet& ground_states_to_keep, ABS::Model* model, std::mt19937& rng) {

    //Copy attributes
    this->args = tosplit->args;
    this->parent = parent;
    this->expanded = tosplit->expanded;
    this->lb = tosplit->lb; //all bound values technically not necessary as they are recalculated in updateTree after split
    this->ub = tosplit->ub;
    this->actions_ub = tosplit->actions_ub;
    this->actions_lb = tosplit->actions_lb;
    this->ss_depth = tosplit->ss_depth;
    this->remaining_episode_steps = tosplit->remaining_episode_steps;
    this->freed_gamestates = tosplit->freed_gamestates;

    //Filter ground states
    this->ground_states = {};
    for(auto gs : ground_states_to_keep)
        ground_states.insert({gs,0});

    //Recalculate reward, actions, terminality and probabilities
    update(model);

    this->successors = {};
    this->terminal_succ = {};
    if(!isSSTreeLeaf()) {

        //add successors (some of the keys may not be in the current action set, that happens for intersection action space - we do not access them though)
        for(auto & pair : tosplit->successors) {
            assert (!pair.second.empty());
            for(auto succ : tosplit->successors[pair.first]) {
                auto to_keep_succ_gs = gnSet();
                for(auto gs : succ->ground_states) {
                    if(ground_states_to_keep.contains(gs.first->parent)) {
                        to_keep_succ_gs.insert(gs.first);
                    }
                }
                if(!to_keep_succ_gs.empty()) {
                    auto new_succ = new ParssNode(succ,this,to_keep_succ_gs,model,rng);
                    this->successors[pair.first].emplace_back(new_succ);
                    if(new_succ->terminal && args->separate_terminals)
                        this->terminal_succ[pair.first] = new_succ;
                }
            }
        }

        //actions that are not in split, will be automatically expanded in enforceSparseSamplingProperty
    }

}

double ParssNode::getHeuristicalVForLeaf(ABS::Model *model) const {
    if(terminal)
        return 0;

    double reward = 0;
    if(!args->zero_at_leaf){
        for(auto gs : ground_states)
            reward += gs.second * model->heuristicsValue(gs.first->state)[0];
    }

    double lb = getVBound(model, true,false);
    double ub = getVBound(model, false,false);
    if(reward < lb) {
        reward = lb;
        assert (!args->zero_at_leaf); //this violates the assumption that 0 is a bounded heuristic
    }
    else if(reward > ub) {
        reward = ub;
        assert (!args->zero_at_leaf);
    }
    return reward;
}

double ParssNode::getReward(int action) const {
    assert (!terminal);

    if(args->empirical_sampling) {
        double rsum = 0;
        int n = 0;
        for(auto gs : ground_states) {
            for(auto gs_succ : gs.first->sample_indices[action]) {
                rsum += gs_succ.second->reward * gs_succ.second->num_sampled;
                n += gs_succ.second->num_sampled;
            }
        }
        return rsum/(double)n;

    }else {
        double reward = 0;
        for(auto gs : ground_states) {
            double psum = 0;
            double rsum = 0;
            for(auto gs_succ : gs.first->sample_indices[action]) {
                rsum += gs_succ.second->reward * gs_succ.second->trans_prob;
                psum += gs_succ.second->trans_prob;
            }
            reward += gs.second * (rsum / psum);
        }

        return reward;
    }
}

double ParssNode::getVBound(ABS::Model* model, bool lower, bool after_action)  const{
    if(terminal) {
        assert (!after_action);
        return 0;
    }else {
        double upper_val = model->getMaxV(remaining_episode_steps);
        double lower_val = model->getMinV(remaining_episode_steps);
        assert (upper_val >= lower_val &&  (args->abstract_terminality != "intersection" || (lower_val <= 0 && upper_val >= 0)));
        return lower? lower_val : upper_val;
    }
}

void ParssNode::update(ABS::Model* model) {

    //terminality
    if(args->abstract_terminality == "union"){
        terminal = false;
        for (const auto& gs : ground_states) {
            if (gs.first->state->terminal) {
                terminal = true;
                break;
            }
        }
    }
    else if(args->abstract_terminality == "intersection"){
        terminal = true;
        for (const auto& gs : ground_states) {
            if (!gs.first->state->terminal) {
                terminal = false;
                break;
            }
        }
    }
    else {
        throw std::runtime_error("Unknown abstract terminality");
    }

    //Actions (intersection of all available ground state actions)
    if(terminal) {
        actions = {};
    }else if(args->abstract_action_space == "intersection") {
        std::vector<std::vector<int>> action_lists = {};
        for (const auto& gs : ground_states) {
            if(!gs.first->state->terminal)
                action_lists.push_back(model->getActions(gs.first->state));
            else
                action_lists.emplace_back();
        }
        std::map<int, int> frequencyMap;
        for (const auto& sublist : action_lists) {
            for (int num : sublist)
                frequencyMap[num]++;
        }
        actions = {};
        for (const auto& pair : frequencyMap) {
            if (pair.second == static_cast<int>(action_lists.size())) {
                actions.push_back(pair.first);
            }
        }
    }else if (args->abstract_action_space == "union"){
        std::set<int> action_set;
        for (const auto& gs : ground_states) {
            if(gs.first->state->terminal)
                continue;
            for (int a : model->getActions(gs.first->state)) {
                action_set.insert(a);
            }
        }
        actions = std::vector<int>(action_set.begin(), action_set.end());
    }else {
        throw std::runtime_error("Unknown abstract action space");
    }

    //Update ground node probabilities
    if(args->empirical_sampling) {
        int total = 0;
        for(auto& gs : ground_states)
            total += gs.first->num_sampled;
        for(auto& gs : ground_states) {
            gs.second = gs.first->num_sampled / (double) total;
        }
    }else {
        double psum = 0;
        for(auto& gs : ground_states) {
            double p_path = 1;
            auto tmp = gs.first;
            while(tmp != nullptr) {
                p_path *= tmp->trans_prob;
                tmp = tmp->parent;
            }
            psum += p_path;
            ground_states[gs.first] = p_path;
        }
        for(auto& gs : ground_states)
            ground_states[gs.first] /= psum;
    }

}

std::string ParssNode::toString(int spaces) {
    std::string prefix;
    for(int i = 0; i < spaces; i++)
        prefix += "   ";
    std::string str = prefix+"Node: " + std::to_string(expanded) + " " + std::to_string(lb) + " " + std::to_string(ub)  + std::to_string(terminal) + "\n";
    for(auto& pair : successors) {
        str += prefix+"Action: " + std::to_string(pair.first) + "\n";
        for(auto succ : pair.second) {
            str += succ->toString(spaces+1);
        }
    }
    return str;
}

int ParssNode::getNumGroundSuccessors(int action){
    int num = 0;
    for(auto succ : successors[action])
        num += succ->ground_states.size();
    return num;
}

int ParssNode::size()
{
    int size = 0;
    for(auto& pair : successors) {
        for(auto succ : pair.second) {
            size += succ->size();
        }
    }
    return size + 1;
}

void ParssNode::delete_gamestates(bool recursively) {
    if(!freed_gamestates) {
        for(auto gs : ground_states) {
            delete gs.first->state;
        }
    }else {
        assert (args->prune_closed_nodes);
    }
    freed_gamestates = true;

    if(recursively) {
        for(auto& pair : successors) {
            for(auto succ : pair.second) {
                succ->delete_gamestates(recursively);
            }
        }
    }
}

ParssNode::~ParssNode() {
    for (auto& pair : successors) {
        for (auto succ : pair.second) {
            delete succ;
        }
    }
}

void ParssNode::sample(ABS::Model* model, int abstract_action, bool override_emp_sampling, std::mt19937& rng) {

    assert (!freed_gamestates);

    //Create abstract (empty) successor(s) if action is sampled for the first time
    if(successors[abstract_action].empty() || (args->separate_terminals && successors[abstract_action].size() == 1 && terminal_succ.contains(abstract_action))){
        int to_be_sampled_gs = ground_states.size() * std::ceil(args->C / (double) this->ground_states.size());
        int num_abs_succ = (int) (args->init_group_ratio * (to_be_sampled_gs-1) + 1);
        for(int i = 0; i < num_abs_succ; i++)
            successors[abstract_action].push_back(new ParssNode(this,{},model,args));
    }

    //random permutation to distribute samples across abstract successors evenly
    int n = successors[abstract_action].size();
    assert (n == 1 || args->init_group_ratio > 0.0 || args->separate_terminals); //since we are doing bfs, it should not happen that a successor is split before its parent
    int num_sample = 0;
    std::vector<int> perm(n);
    std::iota(perm.begin(), perm.end(), 0);
    std::shuffle(perm.begin(), perm.end(), rng);

    //determine which ground states to sample from
    std::vector<GroundNode*> to_sample_gs = {};
    bool empirical_sampling = args->empirical_sampling && !override_emp_sampling;
    if(empirical_sampling) { //sample C ground states
         std::vector<GroundNode*> gs_list = {};
         std::vector<double> prob_list = {};
            double psum = 0;
         for(auto& gs : ground_states) {
             gs_list.push_back(gs.first);
             prob_list.push_back(gs.second);
             psum += gs.second;
         }
        assert (std::fabs(1 - psum) < 1e-6);
         std::discrete_distribution<> dist(prob_list.begin(), prob_list.end());
        for(int i = 0; i < args->C; i++)
            to_sample_gs.push_back(gs_list[dist(rng)]);

    }else {
        for(auto gs : ground_states)
            to_sample_gs.push_back(gs.first);
    }

    //Do actual sampling from ground states
    for(auto gs : to_sample_gs){
        if(gs->sample_indices.find(abstract_action) == gs->sample_indices.end()) {
            gs->sample_indices[abstract_action] = {};
            gs->cumul_sample_probs[abstract_action] = 0;
        }
        auto& sampled_succ_states = gs->sample_indices[abstract_action];

        //sample one time or until every ground state satisfies the sparse sampling property
        const double tol = 1e-6;
        int samples = 0;
        while( (empirical_sampling && samples < 1) || ( !empirical_sampling  &&
                                                            std::fabs(gs->cumul_sample_probs[abstract_action] - 1) > tol &&
                                                            sampled_succ_states.size() < std::ceil(args->C / (double) this->ground_states.size()))) {

            auto state_rew_prob = gs->applyAbstractAction(model, abstract_action, rng);
            if(!sampled_succ_states.contains(std::get<0>(state_rew_prob))) {
                gs->cumul_sample_probs[abstract_action] += std::get<2>(state_rew_prob);
                auto new_sampled_gs = new GroundNode(args->abstract_action_space, model, std::get<0>(state_rew_prob), gs, std::get<1>(state_rew_prob),std::get<2>(state_rew_prob));
                if(new_sampled_gs->state->terminal && args->separate_terminals) {
                    if(!terminal_succ.contains(abstract_action)) {
                        terminal_succ[abstract_action] =new ParssNode(this,{},model,args);
                        successors[abstract_action].push_back(terminal_succ[abstract_action]);
                    }
                    terminal_succ[abstract_action]->ground_states.insert({new_sampled_gs,0});
                }
                else {
                    if(args->separate_terminals && terminal_succ.contains(abstract_action) &&
                        terminal_succ[abstract_action] == successors[abstract_action][perm[num_sample%n]]) //skip terminal successor if necessary
                        num_sample++;
                    auto succ = successors[abstract_action][perm[num_sample % n]];
                    if( (++num_sample)%n == 0)
                        std::shuffle(perm.begin(), perm.end(), rng);
                    succ->ground_states.insert({new_sampled_gs,0});
                }
                sampled_succ_states.insert(std::pair(std::get<0>(state_rew_prob),new_sampled_gs));
            }else {
                sampled_succ_states[std::get<0>(state_rew_prob)]->num_sampled++;
            }
            samples++;
        }
    }

    //Update reward, action space and terminality
    bool all_in_terminal = false;
    for(auto as : successors[abstract_action]) {
        as->update(model);
        if(as->getNumGroundStates() == 0) {
            assert (args->separate_terminals && successors[abstract_action].size() ==2 && terminal_succ.contains(abstract_action));
            all_in_terminal = true;
            delete as;
        }
    }
    if(all_in_terminal) { //handle case where all successors are empty (only happens when first time sample is called and all succ land in terminal)
        successors[abstract_action] = {terminal_succ[abstract_action]};
    }

}

void ParssNode::expand(int action, ABS::Model* model, bool override_emp_sampling, std::mt19937& rng) {
    actions_lb[action] = getVBound(model, true,true);
    actions_ub[action] = getVBound(model, false,true);
    sample(model, action, override_emp_sampling, rng);
}

void ParssNode::expand(ABS::Model* model, std::mt19937& rng) {
    for(int a : actions) {
        expand(a, model, false, rng);
    }
    expanded = true;
}

std::tuple<int,int,int> ParssNode::split(ABS::Model* model, std::mt19937& rng) {

    if(args->test_mode)
        test_split();

    //split into two groups of equal size
    std::uniform_real_distribution<double> dist(-1.0, 1.0);

    std::map<double,GroundNode*> gs_sample_vals = {};

    for (auto gs : ground_states)
        gs_sample_vals[dist(rng)] = gs.first;

    auto first_gs = gnSet();
    auto second_gs = gnSet();
    size_t size1 = ground_states.size() / 2;
    [[maybe_unused]] size_t size2 = ground_states.size() - size1;
    int idx = 0;
    for(auto& pair : gs_sample_vals) {
        if(idx < static_cast<int>(size1))
            first_gs.insert(pair.second);
        else
            second_gs.insert(pair.second);
        idx++;
    }
    assert (first_gs.size() == size1 && second_gs.size() == size2);

    //find action and idx that led to this node
    assert (getParent() != nullptr);
    std::pair<int,int> parent_action_succ;
    bool found = false;
    for(int a : getParent()->actions) {
        for(size_t i = 0; i < getParent()->successors[a].size(); i++) {
            if(getParent()->successors[a][i] == this) {
                parent_action_succ = {a,i};
                found=true;
                break;
            }
        }
        if(found)
            break;
    }

    //propagate split down the tree in bfs manner
    auto *split1 = new ParssNode(this,this->getParent(),first_gs,model, rng);
    auto *split2 = new ParssNode(this,this->getParent(),second_gs,model,rng);
    getParent()->successors[parent_action_succ.first][parent_action_succ.second] = split1;
    getParent()->successors[parent_action_succ.first].push_back(split2);


    return {parent_action_succ.first,parent_action_succ.second,getParent()->successors[parent_action_succ.first].size()-1}; //action + idx of split node + idx of second split node
}