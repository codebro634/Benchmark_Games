#include "../../include/Utils/AgentMaker.h"
#include "../../include/Agents/Mcts/MctsAgent.h"
#include "../../include/Agents/FFHindsightAgent.h"
#include "../../include/Games/TwoPlayerGames/Pusher.h"
#include "../../include/Agents/LookupTableAgent.h"
#include "../../include/Agents/RandomAgent.h"
#include "../../include/Agents/Asap/AsapAgent.h"
#include "../../include/Agents/HumanAgent.h"
#include "../../include/Agents/Parss/ParssAgent.h"
#include "../../include/Agents/SparseSamplingAgent.h"

#include <map>
#include <set>
#include <sstream>


Agent* getDefaultAgent(bool strong){
    if (strong)
        return new Mcts::MctsAgent({{500, "iterations"}, {4}, 1.0, 1, -1, true, true});
    else
       return new RandomAgent();
}

std::string extraArgs(std::map<std::string, std::string>& given_args, const std::set<std::string>& acceptable_args){
    for (auto& [key, val] : given_args) {
        if(!acceptable_args.contains(key))
            return key;
    }
    return "";
}

Agent* getAgent(const std::string& agent_type, const std::vector<std::string>& a_args)
{
    //Parse named args
    std::map<std::string, std::string> agent_args;
    for(auto &arg   : a_args) {
        //split at '='
        auto pos = arg.find('=');
        if (pos == std::string::npos) {
            std::cout << "Invalid agent argument: " << arg << ". It must be of the form arg_name=arg_val" << std::endl;
            return nullptr;
        }
        agent_args[arg.substr(0, pos)] = arg.substr(pos + 1);
    }
    std::set<std::string> acceptable_args;

    Agent* agent;
    if (agent_type == "random") {
        acceptable_args = {};
        agent =  new RandomAgent();
    }
    else if(agent_type == "mcts")
    {
        assert (agent_args.contains("iterations"));
        if(agent_args.contains("wirsa"))
            assert (agent_args.contains("a") && agent_args.contains("b"));
        acceptable_args = {"iterations", "rollout_length", "discount", "num_rollouts", "dag", "dynamic_exp_factor", "expfacs", "wirsa", "a", "b"};

        int iterations = std::stoi(agent_args["iterations"]);
        int rollout_length = agent_args.find("rollout_length") == agent_args.end() ? -1 : std::stoi(agent_args["rollout_length"]);
        double discount = agent_args.find("discount") == agent_args.end() ? 1.0 : std::stod(agent_args["discount"]);
        int num_rollouts = agent_args.find("num_rollouts") == agent_args.end() ? 1: std::stoi(agent_args["num_rollouts"]);
        bool dag = agent_args.find("dag") == agent_args.end() ? false : std::stoi(agent_args["dag"]);
        bool dynamic_exp_factor = agent_args.find("dynamic_exp_factor") == agent_args.end() ? false : std::stoi(agent_args["dynamic_exp_factor"]);
        bool wirsa = agent_args.find("wirsa") == agent_args.end() ? false : std::stoi(agent_args["wirsa"]);
        double a = agent_args.find("a") == agent_args.end() ? 0.0 : std::stod(agent_args["a"]);
        double b = agent_args.find("b") == agent_args.end() ? 0.0 : std::stod(agent_args["b"]);
        std::string exp_facs = agent_args.find("expfacs") == agent_args.end() ? "1" : agent_args["expfacs"];
        std::vector<double> expfac;
        std::stringstream ss(exp_facs);
        double i;
        while (ss >> i){
            expfac.push_back(i);
            if (ss.peek() == ';')
                ss.ignore();
        }
        auto args = Mcts::MctsArgs{.budget = {iterations, "iterations"}, .exploration_parameters = expfac, .discount = discount,
            .num_rollouts = num_rollouts,
            .rollout_length = rollout_length,
            .dag=dag,
            .dynamic_exploration_factor=dynamic_exp_factor,
            .wirsa = wirsa,
            .a=a,.b=b};
        agent =  new Mcts::MctsAgent(args);
    }
     else if(agent_type == "parss") {
        assert (agent_args.contains("C") && agent_args.contains("d") && agent_args.contains("refines") && agent_args.contains("vmin") && agent_args.contains("vmax"));
        acceptable_args = {"C", "d", "refines", "vmin", "vmax", "discount"};

        int refines = std::stoi(agent_args["refines"]);
        int d = std::stoi(agent_args["d"]);
        int C = std::stoi(agent_args["C"]);
        double discount = agent_args.find("discount") == agent_args.end() ? 1.0 : std::stod(agent_args["discount"]);
        auto args = PARSS::ParssArgs{ .budget = {refines, "refines" }, .C = C, .depth = d, .discount = discount}  ;
        agent =  new PARSS::ParssAgent(args);

    } else if(agent_type == "asap") {
        assert (agent_args.contains("iterations"));
        acceptable_args = {"min_group_visits","top_n_matches","iterations", "expfacs","l", "discount", "num_rollouts", "rollout_length", "dynamic_exp_fac", "group_partially_expanded_states","eps_a", "eps_t", "empirical_model", "framework"};
        int iterations = std::stoi(agent_args["iterations"]);
        std::string exp_facs = agent_args.find("expfacs") == agent_args.end() ? "4" : agent_args["expfacs"];
        std::vector<double> expfac;
        std::stringstream ss(exp_facs);
        double i;
        while (ss >> i){
            expfac.push_back(i);
            if (ss.peek() == ';')
                ss.ignore();
        }
        double discount = agent_args.find("discount") == agent_args.end() ? 1.0 : std::stod(agent_args["discount"]);
        int rollout_length = agent_args.find("rollout_length") == agent_args.end() ? -1 : std::stoi(agent_args["rollout_length"]);
        int num_rollouts = agent_args.find("num_rollouts") == agent_args.end() ? 1: std::stoi(agent_args["num_rollouts"]);
        bool dynamic_exp_fac = agent_args.find("dynamic_exp_fac") == agent_args.end() ? true: std::stoi(agent_args["dynamic_exp_fac"]);
        bool group_partially_explored_states = agent_args.find("group_partially_expanded_states") == agent_args.end() ? true : std::stoi(agent_args["group_partially_expanded_states"]);
        double epsilon_a = agent_args.find("eps_a") == agent_args.end() ? 0.0 : std::stod(agent_args["eps_a"]);
        double epsilon_t = agent_args.find("eps_t") == agent_args.end() ? 0.0 : std::stod(agent_args["eps_t"]);
        int l = agent_args.find("l") == agent_args.end() ? 1 : std::stoi(agent_args["l"]);
        bool empirical_model = agent_args.find("empirical_model") == agent_args.end() ? false : std::stoi(agent_args["empirical_model"]);
        int top_n_matches = agent_args.find("top_n_matches") == agent_args.end() ? 1 : std::stoi(agent_args["top_n_matches"]);
        int min_group_visits = agent_args.find("min_group_visits") == agent_args.end() ? 0 : std::stoi(agent_args["min_group_visits"]);
        std::string framework = agent_args.find("framework") == agent_args.end() ? "mcts" : agent_args["framework"];
        auto args = Asap::AsapArgs{.budget = {iterations, "iterations"}, .exploration_parameters = expfac, .discount = discount,
            .num_rollouts = num_rollouts,
            .rollout_length = rollout_length,
            .dynamic_exploration_factor = dynamic_exp_fac,
            .group_partially_expanded_states = group_partially_explored_states,
            .epsilon_a = epsilon_a,
            .epsilon_t = epsilon_t,
            .l = l,
            .empirical_model = empirical_model,
            .framework = framework,
            .min_group_visits = min_group_visits,
            .top_n_matches = top_n_matches};
        agent =  new Asap::AsapAgent(args);
    }else{
        throw std::runtime_error("Invalid agent");
    }

    if (agent != nullptr) {
        if (!extraArgs(agent_args, acceptable_args).empty()) {
            std::string err_string = "Invalid agent argument: " + extraArgs(agent_args, acceptable_args);
            std::cout << err_string << std::endl;
            throw std::runtime_error(err_string);
        }
        return agent;
    }else {
        std::cout << "Invalid agent" << std::endl;
        throw std::runtime_error("Invalid agent");
    }
}