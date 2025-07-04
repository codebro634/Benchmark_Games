#include "../../include/Agents/FFHindsightAgent.h"

using namespace FFHindsight;

FFHindsightAgent::FFHindsightAgent(const FFHindsightArgs& args) {
    mcts_agent = new Mcts::MctsAgent(args.mcts_args);
    num_futures = args.num_futures;
    variance_reduction_trick = args.variance_reduction_trick;
    average_averages = args.average_averages;
}

FFHindsightAgent::~FFHindsightAgent() {
    delete mcts_agent;
}

int FFHindsightAgent::getAction(ABS::Model* model, ABS::Gamestate* gamestate, std::mt19937& rng) {

    std::map<int,std::pair<double,int>> action_values = {};

    //get Q values for num_futures determinized envs
    for(int i = 0; i < num_futures; i++) {
        Mcts::MctsSearchStats search_stats;
        auto root = mcts_agent->buildTree(model,gamestate, search_stats, rng,true,variance_reduction_trick);
        for(int action : *root->getTriedActions()){
            action_values[action].first += (*root->getActionValues(action))[gamestate->turn] / (double)(average_averages? root->getActionVisits(action) : 1);
            action_values[action].second += average_averages? 1 : root->getActionVisits(action);
        }
        mcts_agent->cleanupTree(root);
    }

    //choose action with highest average Q value
    int best_action = -42;
    double best_value = std::numeric_limits<double>::lowest();
    for(auto& [action, value] : action_values){
        double avg_value = value.first / (double)value.second;
        if(avg_value > best_value){
            best_value = avg_value;
            best_action = action;
        }
    }

    return best_action;
}