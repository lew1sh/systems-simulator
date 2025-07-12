#include "scenario_loader.h"

void ScenarioLoader::load(const string& filename) {
    ifstream file(filename);
    for(string line; getline(file, line);) {
        istringstream iss(line);
        double t; string name; int val;
        if(iss >> t >> name >> val) 
            steps.push_back({sc_time(t,SC_NS), name, val});
    }
    sort(steps.begin(), steps.end(), [](auto& a, auto& b){ return a.t < b.t; });
}

bool ScenarioLoader::get_next(sc_time t, string& name, int& val) {
    if(idx >= steps.size()) return false;
    if(steps[idx].t <= t) {
        name = steps[idx].name;
        val = steps[idx].val;
        idx++;
        return true;
    }
    return false;
} 