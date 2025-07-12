#ifndef TERMINATOR_H
#define TERMINATOR_H

#include <systemc>
#include <vector>
#include <string>
#include <unordered_map>
#include <regex>

using namespace sc_core;
using namespace std;

class Terminator : public sc_module {
public:
    sc_in<int> dout, stat;
    sc_in<int> hash[5];
    sc_in<int> start, din, com_reg; 
    
    // Флаг для отслеживания срабатывания
    bool triggered = false;
    sc_time trigger_time;

    Terminator(sc_module_name name);
    void load_conditions(const string& filename);

private:
    struct FuncCond {
        string name;
        vector<string> args;
        string expr;
    };

    vector<FuncCond> funcs;

    void check_conditions();
    bool evaluate_expr(const string& expr, const unordered_map<string, int>& ctx);
    bool evaluate_single_condition(const string& condition, const unordered_map<string, int>& ctx);
};

#endif // TERMINATOR_H 