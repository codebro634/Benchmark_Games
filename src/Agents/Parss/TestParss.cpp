#include "../../../include/Utils/UnitTest.h"
#include "../../../include/Agents/Parss/ParssAgent.h"
#include "../../../include/Games/MDPs/Saving.h"
#include "../../../include/Games/MDPs/FixedActionSpaceEnv.h"

namespace PARSS {

    void ParssNode::test_split() {
        if(args->allow_refine_with_optimal_parent)
            return;
        //check if there is no node below the current one that has already been split
        for(int action : actions) {
            if(!successors.contains(action))
                continue;
            assert (!isExpanded() || successors[action].size() == 1);
            for(auto succ : successors[action]) {
                succ->test_split();
            }
        }
    }

    void ParssNode::test_bounds(ABS::Model *model) {
        //recursively check if any action or node bounds are outside vmin,vmax range
        assert (lb >= getVBound(model,true,false) && ub <= getVBound(model,false,false));
        for(auto& pair : successors) {
            assert (actions_lb[pair.first] >= getVBound(model,true,true) && actions_ub[pair.first] <= getVBound(model,false,true));
            for(auto succ : pair.second) {
                succ->test_bounds(model);
            }
        }
    }

    void ParssNode::test_succ_numbers(){
        //test if successor numbers match for each action
        for(auto& pair : successors) {
            int num_succ = 0;
            for(auto succ : pair.second) {
                num_succ += succ->ground_states.size();
                succ->test_succ_numbers();
            }
            int m = 0;
            for(auto& gs : ground_states) {
                m += gs.first->sample_indices[pair.first].size();
            }
            if(num_succ != m)
                std::cout << "Num succ: " << num_succ << " m: " << m << std::endl;
            assert(num_succ == m);
        }
        if(!isExpanded() || args->empirical_sampling)
            return;
        for(int a : actions) {
            int num = -1;
            for(auto& gs : ground_states) {
                assert (gs.first->sample_indices.contains(a));
                if(num == -1)
                    num = gs.first->sample_indices.at(a).size();
                assert (num == static_cast<int>(gs.first->sample_indices.at(a).size()));
            }
        }
    }

    void ParssNode::test_empty_successors() {
        //test if there are no empty successors
        for(auto &pair : successors) {
            assert (pair.second.size() > 0);
            for(auto succ : pair.second) {
                succ->test_empty_successors();
            }
        }
    }

    void ParssAgent::testRun() {
        const int seed = 42;
        std::mt19937 rng(static_cast<unsigned int>(seed));
        auto model= FINITEH::Model(new SAVING::Model(),100,true);
        auto state = model.getInitialState(rng);
        auto agent = PARSS::ParssAgent({{1000, "refines"},5,5,0.0,
             "intersection", "union", "bfs", true, true, 1.0, true});
        for(int i = 0; i < 5; i++) {
            int action = agent.getAction(&model,state,rng);
            model.applyAction(state,action,rng, nullptr);
        }
        delete state;

        ASSERT_TRUE(true); //success if we reach this point
    }

    void ParssAgent::testTreeSize(int C, int d, double init_grp_ratio, int size) {
        const int seed = 42;
        std::mt19937 rng(static_cast<unsigned int>(seed));
        auto model= FINITEH::Model(new FASP::Model(3),100,true);
        auto state = model.getInitialState(rng);
        auto agent = PARSS::ParssAgent({{1000, "milliseconds"},C,d,init_grp_ratio,
             "intersection", "union", "bfs", true, true, 1.0});
        auto* root = new ParssNode(nullptr,{{new GroundNode{agent.args.abstract_action_space, &model, model.copyState(state),nullptr,0,1},1}},&model,&agent.args);
        agent.visit(root, &model, rng);
        //std::cout << root->toString() << std::endl;
        delete state;

        ASSERT_EQUALS(size, root->size());
    }

    //Helper for testSplitNum
    void addToActionBounds(ParssNode* node, int value) {
        if(!node->isExpanded())
            return;
        for(int a : node->actions) {
            node->actions_lb[a] -= value;
            node->actions_ub[a] += value;
            for(auto succ : node->successors[a]) {
                addToActionBounds(succ, value);
            }
        }
    }

    void ParssAgent::testSplitNum(int C, int d, bool ignore_vmax_cond, bool update_tree, int num_splits) {
        const int seed = 42;
        std::mt19937 rng(static_cast<unsigned int>(seed));
        auto model= FINITEH::Model(new FASP::Model(), 100, true);
        auto state = model.getInitialState(rng);
        auto agent = PARSS::ParssAgent({{1000, "milliseconds"},C,d, 0.0,
             "intersection", "union", "bfs", true, true, 1.0});
        auto* root = new ParssNode(nullptr,{{new GroundNode{agent.args.abstract_action_space, &model, model.copyState(state),nullptr,0,1},1}},&model,&agent.args);
        agent.visit(root, &model, rng);
        int refines = 0;
        int sub_val = ignore_vmax_cond? 100 : 0;
        addToActionBounds(root,-sub_val);
        auto next = agent.nextRefine(&model, root, rng);
        addToActionBounds(root,sub_val);
        while(next != nullptr) {
            auto indices = next->split(&model, rng);
            if(update_tree)
                agent.updateTree(next->getParent(), indices, &model, rng);
           addToActionBounds(root,-sub_val);
            next = agent.nextRefine(&model, root, rng);
            addToActionBounds(root,sub_val);
            refines++;
        }
        delete state;

        ASSERT_EQUALS(num_splits, refines);
    }


    void ParssAgent::runTests() {
        std::cout << "Running tests for ParssAgent" << std::endl;

        //Refine tests
        testSplitNum(9,2,false,false, 8);
        testSplitNum(9,7,false,false, 8);
        testSplitNum(2,2,false,false, 1);
        testSplitNum(3,10,false,false, 2);
        testSplitNum(2,2,true, true, 9);
        testSplitNum(2,3,true,true, 21);
        testSplitNum(3,2,true,true,24);
        testSplitNum(5,2,true,true,72);

        //Test tree sizes of non-abstracted tree (i.e. init_group_ratio = 1.0) after 1 call of visit
        testTreeSize(2,2,1.0,13);
        testTreeSize(2,3,1.0,19);
        testTreeSize(7,11,1.0,232);

        //Test tree sizes of fully abstracted tree (i.e. init_group_ratio = 0.0) after 1 call of visit
        testTreeSize(2,2,0.0,7);
        testTreeSize(10,2,0.0,7);
        testTreeSize(5,10,0.0,31);

        //Run a big instance of PARSS in test mode to check for runtime errors or assertions fails
        //testRun();


        std::cout << "Finished tests for ParssAgent" << std::endl;

    }

}