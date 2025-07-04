#ifndef LOOKUPAGENT_H
#define LOOKUPAGENT_H
#include <unordered_map>
#include "../Utils/ValueIteration.h"
#include "Agent.h"
#endif


namespace LookupAgent {

    class LookupTableAgent: public Agent {

    public:
        explicit LookupTableAgent(ABS::Model* model, std::string filename);
        explicit LookupTableAgent(std::unordered_map<std::pair<FINITEH::Gamestate*,int>, double, VALUE_IT::QMapHash, VALUE_IT::QMapCompare>* Q_map);
        ~LookupTableAgent() override;
        int getAction(ABS::Model* model, ABS::Gamestate* gamestate, std::mt19937& rng) override;


    private:
        bool loaded_from_file = false;
        std::unordered_map<std::pair<FINITEH::Gamestate*,int>, double, VALUE_IT::QMapHash, VALUE_IT::QMapCompare>* Q_map;

    };

}


