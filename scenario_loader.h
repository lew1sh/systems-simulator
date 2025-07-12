#ifndef SCENARIO_LOADER_H
#define SCENARIO_LOADER_H

#include <systemc>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <algorithm>

using namespace sc_core;
using namespace std;

struct ScenarioStep {
    sc_time t;
    string name;
    int val;
};

//  сценарий
class ScenarioLoader {
    vector<ScenarioStep> steps;
    size_t idx = 0;
public:
    void load(const string& filename);
    bool get_next(sc_time t, string& name, int& val);
    bool done() const { return idx >= steps.size(); }
};

#endif