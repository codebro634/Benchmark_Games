#ifndef FFHSAGENT_H
#define FFHSAGENT_H
#include "Agent.h"
#include "Mcts/MctsAgent.h"
#endif


namespace FFHindsight {

    struct FFHindsightArgs{
        Mcts::MctsArgs mcts_args;
        int num_futures;
        bool variance_reduction_trick;
        bool average_averages;
    };

    class FFHindsightAgent: public Agent {

    public:
        explicit FFHindsightAgent(const FFHindsightArgs& args);
        ~FFHindsightAgent();
        int getAction(ABS::Model* model, ABS::Gamestate* gamestate, std::mt19937& rng) override;

    private:
        Mcts::MctsAgent* mcts_agent;
        int num_futures;
        bool variance_reduction_trick;
        bool average_averages;

    };

}


