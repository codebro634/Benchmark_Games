#pragma once

#ifndef PARSSAGENT_H
#define PARSSAGENT_H
#include "../Agent.h"
#include "ParssNode.h"
#endif

namespace PARSS {

    class ParssAgent final : public Agent
    {

    public:
        explicit ParssAgent(const ParssArgs& args) : args(args) {};
        int getAction(ABS::Model* model, ABS::Gamestate* state, std::mt19937& rng) override;

        static void runTests();
        static void testSplitNum(int C, int d, bool ignore_vmax_cond, bool update_tree, int num_splits);
        static void testTreeSize(int C, int d, double init_grp_ratio, int size);
        static void testRun();

    private:
        int AFSSS(ParssNode* node, ABS::Model* model, std::mt19937& rng);
        std::pair<bool,int> visit(ParssNode* node, ABS::Model* model, std::mt19937& rng);
        void backup(ABS::Model* model, ParssNode* node, int action);
        void backup(ParssNode* node);
        ParssNode* nextRefine(ABS::Model* model, ParssNode* root, std::mt19937& rng) const;
        void updateTree(ParssNode* node, std::tuple<int,int,int>& action_and_split_indices, ABS::Model* model, std::mt19937& rng);
        void enforceSparseSamplingProperty(ParssNode* node, ABS::Model* model, std::mt19937& rng);
        void deleteStatesOfClosedNodes(ParssNode* node);

        ParssArgs args;

        constexpr static double TIEBREAKER_NOISE = 1e-6;
    };
}
