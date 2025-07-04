#include <utility>
#include <random>
#include <algorithm>
#include <ranges>
#include "../../../include/Agents/Asap/AsapNode.h"

using namespace Asap;

AsapNode::AsapNode(
    ABS::Model* model,
    ABS::Gamestate* state,
    int depth,
    std::mt19937& rng
): model(model), state(state), depth(depth)
{
    visits = 0;
    untried_actions = state->terminal? std::vector<int>() : model->getActions(state);
    std::ranges::shuffle(untried_actions.begin(), untried_actions.end(), rng);
}

int AsapNode::popUntriedAction(){
    int a = untried_actions.back();
    untried_actions.pop_back();
    tried_actions.push_back(a);
    action_values[a] = 0;
    action_visits[a] = 0;
    children[a] = {};
    return a;
}

void AsapNode::addVisit()
{
    visits++;
}

bool AsapNode::isAbstracted(int action) {
    return abstract_q_nodes.contains(action);
}

double AsapNode::getAbstractActionValue(int action) {
    return abstract_q_nodes.at(action)->value;
}

int AsapNode::getAbstractActionVisits(int action) {
    return abstract_q_nodes.at(action)->visits;
}


void AsapNode::addActionVisit(const int action)
{
    action_visits[action]++;
}

void AsapNode::addActionValue(const int action, const double value){
        action_values[action] += value;
}

bool AsapNode::isFullyExpanded() const
{
    return untried_actions.empty();
}

int AsapNode::getDepth() const
{
    return depth;
}

ABS::Model* AsapNode::getModel() const
{
    return model;
}

double AsapNode::getActionValue(const int action)
{
    return action_values[action];
}

int AsapNode::getActionVisits(const int action)
{
    return action_visits[action];
}

std::vector<int>* AsapNode::getTriedActions()
{
    return &tried_actions;
}

std::map<int, gsToNodeMap<AsapNode*>>* AsapNode::getChildren()
{
    return &children;
}

int AsapNode::getVisits() const
{
    return visits;
}

bool AsapNode::isTerminal() const
{
    return state->terminal;
}

ABS::Gamestate* AsapNode::getStateCopy() const
{
    return model->copyState(state);
}

int AsapNode::getPlayer() const
{
    return state->turn;
}

const ABS::Gamestate* AsapNode::getState() const {
    return state;
}
