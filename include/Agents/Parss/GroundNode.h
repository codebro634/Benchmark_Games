

#ifndef GROUNDNODE_H
#define GROUNDNODE_H

#include <map>
#include <unordered_set>
#include <set>
#include <unordered_map>
#include "../../Arena.h"

#endif //GROUNDNODE_H


namespace PARSS {


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

    template<class T>
    using gsToNodeMap = std::unordered_map<ABS::Gamestate*, T, GSHash, GSCompare>;

    inline long global_id = 0;

    class GroundNode {

    public:
        GroundNode(const std::string& abstract_action_space, ABS::Model* model, ABS::Gamestate* state, GroundNode* parent_gs, double reward, double trans_prob);
        ~GroundNode();

        std::tuple<ABS::Gamestate*,double,double> applyAbstractAction(ABS::Model* model, int abstract_action, std::mt19937& rng) const;

        double trans_prob;
        double reward;
        ABS::Gamestate* state;
        GroundNode* parent;
        std::set<int> avail_actions;
        std::string abstract_action_space;

        //dynamic attributes
        std::map<int,double> q_vals = {};
        double val = 0;
        int num_sampled = 1;

        std::map<int,gsToNodeMap<GroundNode*>> sample_indices = {};
        std::map<int,double> cumul_sample_probs = {};

        long id = global_id++;
    };


    //Custom set and map for GroundNodes pointers as we have to iterate over these data structures and want to ensure a deterministic order for reproducibility
    class GroundNode;

    struct GNCompare {
        bool operator()(const GroundNode* lhs, const GroundNode* rhs) const {
            return lhs->id < rhs->id; // Sort by id in ascending order
        }
    };

    template <class U>
    using gnMap = std::map<GroundNode*, U, GNCompare>;
    using gnSet = std::set<GroundNode*, GNCompare>;



}
