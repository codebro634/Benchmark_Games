#include "../../../include/Agents/Parss/GroundNode.h"

#include <cassert>

namespace PARSS {

    GroundNode::GroundNode(const std::string& abstract_action_space, ABS::Model* model, ABS::Gamestate* state, GroundNode* parent, double reward, double trans_prob) :
    trans_prob(trans_prob),
    reward(reward),
    state(state),
    parent(parent),
    abstract_action_space(abstract_action_space)
    {
        if(abstract_action_space == "union") {
            if(state->terminal) {
                avail_actions = std::set<int>();
            }else {
                auto actions = model->getActions(state);
                avail_actions = std::set<int>(actions.begin(), actions.end());
            }
        }
    }

    GroundNode::~GroundNode() {
        for(auto& pair : sample_indices) {
            for(auto& pair2 : pair.second) {
                delete pair2.second;
            }
        }
    }

    std::tuple<ABS::Gamestate*,double,double> GroundNode::applyAbstractAction(ABS::Model* model, int abstract_action, std::mt19937& rng) const {
        auto copy = model->copyState(state);

        if(state->terminal) {
            return {copy,0,1};
        }

        //determine action
        int action;
        if(abstract_action_space == "intersection") {
            action = abstract_action; //passthrough
        }else if(abstract_action_space == "union") {
            if(avail_actions.contains(abstract_action)) {
                action = abstract_action;
            }else {
                //sample random action
                assert (!avail_actions.empty());
                std::uniform_int_distribution<int> dist(0,avail_actions.size()-1);
                auto it = avail_actions.begin();
                std::advance(it, dist(rng));
                action = *it;
            }
        }else {
           throw std::runtime_error("Unknown abstract action space: " + abstract_action_space);
        }

        auto result = model->applyAction(copy, action, rng, nullptr);
        double reward = result.first[0]; //assuming single player hence we query index 0 of the reward. Also we assume that an action on a terminal state has no effect
        double sample_prob = result.second;
        return {copy,reward,sample_prob};
    }

} //namespace PARSS