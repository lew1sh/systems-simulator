#include "terminator.h"
#include <iostream>
#include <fstream>
#include <algorithm>

Terminator::Terminator(sc_module_name name) : sc_module(name) {
    SC_METHOD(check_conditions);
    sensitive << dout << stat << start << din << com_reg;
    for (int i = 0; i < 5; ++i)
        sensitive << hash[i];
    dont_initialize();
}

void Terminator::load_conditions(const string& filename) {
    ifstream file(filename);
    string line;
    string func_name;
    vector<string> args;
    string expr;

    while (getline(file, line)) {
        if (line.find("def") == 0) {
            if (!func_name.empty()) {
                funcs.push_back({func_name, args, expr});
                expr.clear(); args.clear();
            }
            smatch m;
            if (regex_match(line, m, regex(R"(def\s+(\w+)\s*\(\s*(.*?)\s*\))"))) {
                func_name = m[1];
                string args_str = m[2];
                // Исправленный парсинг аргументов
                regex arg_regex(R"(\w+)");
                sregex_iterator iter(args_str.begin(), args_str.end(), arg_regex);
                sregex_iterator end;
                for (; iter != end; ++iter) {
                    args.push_back(iter->str());
                }
            }
        } else if (!line.empty() && line.find_first_not_of(" \t") != string::npos) {
            expr += line + " ";
        }
    }
    if (!func_name.empty())
        funcs.push_back({func_name, args, expr});
    
    cout << "[Terminator] Loaded " << funcs.size() << " functions" << endl;
    for (const auto& func : funcs) {
        cout << "  Function: " << func.name << " with " << func.args.size() << " args" << endl;
    }
}

void Terminator::check_conditions() {
    unordered_map<string, int> context = {
        {"dout", dout.read()},
        {"stat", stat.read()},
        {"start", start.read()},
        {"din", din.read()},
        {"com_reg", com_reg.read()},
        {"hash_0", hash[0].read()},
        {"hash_1", hash[1].read()},
        {"hash_2", hash[2].read()},
        {"hash_3", hash[3].read()},
        {"hash_4", hash[4].read()}
    };

    cout << "[Terminator] Checking at " << sc_time_stamp() 
         << " - hash: [" << context["hash_0"] << ", " << context["hash_1"] 
         << ", " << context["hash_2"] << ", " << context["hash_3"] 
         << ", " << context["hash_4"] << "]" << endl;

    for (const auto& func : funcs) {
        if (evaluate_expr(func.expr, context)) {
            cout << "[Terminator] Function " << func.name
                 << " returned TRUE at " << sc_time_stamp() << endl;
            triggered = true;
            trigger_time = sc_time_stamp();
            sc_stop();
            return;
        }
    }
}

bool Terminator::evaluate_expr(const string& expr, const unordered_map<string, int>& ctx) {
    string clean_expr = expr;
    clean_expr.erase(remove_if(clean_expr.begin(), clean_expr.end(), ::isspace), clean_expr.end());
    
    cout << "[Terminator] Evaluating: " << clean_expr << endl;
    
    vector<string> and_terms;
    size_t pos = 0;
    size_t prev_pos = 0;
    
    while ((pos = clean_expr.find('&', prev_pos)) != string::npos) {
        and_terms.push_back(clean_expr.substr(prev_pos, pos - prev_pos));
        prev_pos = pos + 1;
    }
    and_terms.push_back(clean_expr.substr(prev_pos));
    
    for (const string& term : and_terms) {
        if (!evaluate_single_condition(term, ctx)) {
            return false;
        }
    }
    
    return true;
}

bool Terminator::evaluate_single_condition(const string& condition, const unordered_map<string, int>& ctx) {
    smatch m;
    if (regex_match(condition, m, regex(R"(\((\w+)\s*(==|!=|[<>]=?)\s*(0x[\da-fA-F]+|\d+)\))"))) {
        string var_name = m[1];
        string op = m[2];
        string val_str = m[3];
        
        if (!ctx.count(var_name)) {
            cout << "[Terminator] Variable " << var_name << " not found in context" << endl;
            return false;
        }
        
        int var_val = ctx.at(var_name);
        int compare_val;
        
        if (val_str.find("0x") == 0) {
            compare_val = stoi(val_str.substr(2), nullptr, 16);
        } else {
            compare_val = stoi(val_str);
        }
        
        bool result = false;
        if (op == "==") result = (var_val == compare_val);
        else if (op == "!=") result = (var_val != compare_val);
        else if (op == ">") result = (var_val > compare_val);
        else if (op == "<") result = (var_val < compare_val);
        else if (op == ">=") result = (var_val >= compare_val);
        else if (op == "<=") result = (var_val <= compare_val);
        
        cout << "[Terminator] " << var_name << " " << op << " " << compare_val 
             << " (" << var_val << ") = " << (result ? "TRUE" : "FALSE") << endl;
        
        return result;
    }
    
    cout << "[Terminator] Could not parse condition: " << condition << endl;
    return false;
} 