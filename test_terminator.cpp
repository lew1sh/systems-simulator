#include <systemc>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <unordered_map>
#include <regex>

using namespace sc_core;
using namespace sc_dt;
using namespace std;

class Terminator : public sc_module {
public:
    sc_in<int> dout, stat;
    sc_in<int> hash[5];
    
    // Флаг для отслеживания срабатывания
    bool triggered = false;
    sc_time trigger_time;

    Terminator(sc_module_name name) : sc_module(name) {
        SC_METHOD(check_conditions);
        sensitive << dout << stat;
        for (int i = 0; i < 5; ++i)
            sensitive << hash[i];
        dont_initialize();
    }

    void load_conditions(const string& filename) {
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

private:
    struct FuncCond {
        string name;
        vector<string> args;
        string expr;
    };

    vector<FuncCond> funcs;

    void check_conditions() {
        unordered_map<string, int> context = {
            {"dout", dout.read()},
            {"stat", stat.read()},
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

    bool evaluate_expr(const string& expr, const unordered_map<string, int>& ctx) {
        // Удаляем лишние пробелы и приводим к нижнему регистру
        string clean_expr = expr;
        clean_expr.erase(remove_if(clean_expr.begin(), clean_expr.end(), ::isspace), clean_expr.end());
        
        cout << "[Terminator] Evaluating: " << clean_expr << endl;
        
        // Парсим выражение по операторам & (AND)
        vector<string> and_terms;
        size_t pos = 0;
        size_t prev_pos = 0;
        
        while ((pos = clean_expr.find('&', prev_pos)) != string::npos) {
            and_terms.push_back(clean_expr.substr(prev_pos, pos - prev_pos));
            prev_pos = pos + 1;
        }
        and_terms.push_back(clean_expr.substr(prev_pos));
        
        // Проверяем каждое условие
        for (const string& term : and_terms) {
            if (!evaluate_single_condition(term, ctx)) {
                return false;
            }
        }
        
        return true;
    }
    
    bool evaluate_single_condition(const string& condition, const unordered_map<string, int>& ctx) {
        // Парсим условие вида: (hash_0==0x80)
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
};

// Простой модуль для тестирования
SC_MODULE(HashGenerator) {
    sc_out<int> hash[5];
    sc_in<bool> clk;
    
    int counter = 0;
    
    void generate() {
        // Генерируем тестовые значения
        if (counter == 5) {
            // Устанавливаем ожидаемые значения
            hash[0].write(0x80);
            hash[1].write(0x95);
            hash[2].write(0x41);
            hash[3].write(0x3C);
            hash[4].write(0xFF);
            cout << "[HashGenerator] Set expected hash values at " << sc_time_stamp() << endl;
        } else {
            hash[0].write(counter);
            hash[1].write(counter + 1);
            hash[2].write(counter + 2);
            hash[3].write(counter + 3);
            hash[4].write(counter + 4);
        }
        counter++;
    }
    
    SC_CTOR(HashGenerator) {
        SC_METHOD(generate);
        sensitive << clk.pos();
        dont_initialize();
    }
};

int sc_main(int, char*[]) {
    sc_signal<int> dout, stat;
    sc_signal<int> hash[5];
    sc_clock clk("clk", 1, SC_NS);

    // Terminator
    Terminator term("term");
    term.dout(dout);
    term.stat(stat);
    for(int i=0; i<5; ++i)
        term.hash[i](hash[i]);
    term.load_conditions("inputs/terminator.txt");

    // HashGenerator для тестирования
    HashGenerator gen("gen");
    for(int i=0; i<5; ++i)
        gen.hash[i](hash[i]);
    gen.clk(clk);

    // Инициализация сигналов
    dout.write(0);
    stat.write(0);

    // Tracing
    sc_trace_file* tf = sc_create_vcd_trace_file("terminator_test");
    sc_trace(tf, clk, "clk");
    sc_trace(tf, dout, "dout");
    sc_trace(tf, stat, "stat");
    for(int i=0; i<5; ++i)
        sc_trace(tf, hash[i], "hash_" + to_string(i));

    cout << "Starting Terminator test..." << endl;
    sc_start(10, SC_NS);

    sc_close_vcd_trace_file(tf);
    
    // Проверяем, сработал ли Terminator
    if (term.triggered) {
        cout << "Terminator triggered at " << term.trigger_time << endl;
    } else {
        cout << "Terminator did not trigger" << endl;
    }
    
    cout << "Test completed" << endl;
    return 0;
} 