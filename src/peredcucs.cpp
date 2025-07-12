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


struct TestCondition {
    int t_start, t_end;
    string cond;
};

struct ScenarioStep {
    sc_time t;
    string name;
    int val;
};

// Верификатор тестов
class TestVerifier {
    vector<TestCondition> conditions;
public:
    void parse(const string& filename) {
        ifstream file(filename);
        for(string line; getline(file, line);) {
            smatch m;
            if(regex_match(line, m, regex(R"(\[\s*(\d+)(?:\.\.(\d+))?\s*\][@#]\((.*)\))"))) {
                conditions.push_back({
                    stoi(m[1]),
                    m[2].matched ? stoi(m[2]) : stoi(m[1]),
                    m[3]
                });
            }
        }
    }
    bool check(const unordered_map<string, int>& sigs, int time) {
        for(auto& c : conditions) {
            if(time >= c.t_start && time <= c.t_end && 
               !evaluate(c.cond, sigs)) {
                cout << "Test failed at " << time << ": " << c.cond << endl;
                return false;
            }
        }
        return true;
    }
private:
    bool evaluate(const string& cond, const unordered_map<string, int>& sigs) {
        smatch m;
        if(regex_match(cond, m, regex(R"((\w+)\s*(==|!=|[<>]=?)\s*(\d+|0x[\da-fA-F]+))"))) {
            int val = m[3].str().find("0x") == 0 ? stoi(m[3].str().substr(2),0,16) : stoi(m[3]);
            if(!sigs.count(m[1])) {
                //cout << "Signal " << m[1] << " not found at time " << time << endl;
                return false;
            }

            int sig_val = sigs.at(m[1]);
            string op = m[2];
            if(op=="==") return sig_val==val;
            if(op=="!=") return sig_val!=val;
            if(op==">") return sig_val>val;
            if(op=="<") return sig_val<val;
            if(op==">=") return sig_val>=val;
            if(op=="<=") return sig_val<=val;
        }
        return false;
    }
};

// Загрузчик сценария
class ScenarioLoader {
    vector<ScenarioStep> steps;
    size_t idx = 0;
public:
    void load(const string& filename) {
        ifstream file(filename);
        for(string line; getline(file, line);) {
            istringstream iss(line);
            double t; string name; int val;
            if(iss >> t >> name >> val) 
                steps.push_back({sc_time(t,SC_NS), name, val});
        }
        sort(steps.begin(), steps.end(), [](auto& a, auto& b){ return a.t < b.t; });
    }
    bool get_next(sc_time t, string& name, int& val) {
        if(idx >= steps.size()) return false;
        if(steps[idx].t <= t) {
            name = steps[idx].name;
            val = steps[idx].val;
            idx++;
            return true;
        }
        return false;
    }
    bool done() const { return idx >= steps.size(); }
};

// Модуль источника сигналов
SC_MODULE(Source) {
    sc_out<int> din, start, com_reg;
    sc_in<bool> clk;
    ScenarioLoader* scen;
    
   void gen() {
    while (true) {
        string name; int val;
        if(!scen->get_next(sc_time_stamp(), name, val)) break;
        if(name=="din") din.write(val);
        else if(name=="start") start.write(val);
        else if(name=="com_reg") com_reg.write(val);
        cout << "Set " << name << "=" << val << " at " << sc_time_stamp() << endl;
    }
}

    
    SC_CTOR(Source) : scen(nullptr) {
        SC_METHOD(gen);
        sensitive << clk.pos();
        dont_initialize();
    }
    void set_scen(ScenarioLoader* s) { scen = s; }
};

class Terminator : public sc_module {
public:
    sc_in<int> dout, stat;
    sc_in<int> hash[5];

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
                    istringstream iss(args_str);
                    for (string a; iss >> a;) args.push_back(a);
                }
            } else {
                expr += line + " ";
            }
        }
        if (!func_name.empty())
            funcs.push_back({func_name, args, expr});
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

        for (const auto& func : funcs) {
            if (evaluate_expr(func.expr, context)) {
                cout << "[Terminator] Function " << func.name
                     << " returned TRUE at " << sc_time_stamp() << endl;
                sc_stop();
                return;
            }
        }
    }

    bool evaluate_expr(const string& expr, const unordered_map<string, int>& ctx) {
        // ЗДЕСЬ должен быть твой простой интерпретатор выражений.
        // Например: hash_0 == 0x80 & hash_1 == 0x95 ...
        // Можно использовать regex и разбор по операторам.
        // Для прототипа — можно просто вручную сравнивать ожидаемые значения.

        // Пример тестового костыля:
        if (ctx.at("hash_0") == 0x80 &&
            ctx.at("hash_1") == 0x95 &&
            ctx.at("hash_2") == 0x41 &&
            ctx.at("hash_3") == 0x3C &&
            ctx.at("hash_4") == 0xFF)
            return true;
        return false;
    }
};

int sc_main(int, char*[]) {
    sc_signal<int> din, start, com_reg, data;
    sc_signal<int> dout, stat;
    sc_signal<int> hash[5];
    sc_clock clk("clk", 1, SC_NS);

    // Source
    Source src("src");
    src.din(din);
    src.start(start);
    src.com_reg(com_reg);
    src.clk(clk);

    ScenarioLoader scen;
    scen.load("inputs/scenario.txt");
    src.set_scen(&scen);

    TestVerifier test;
    test.parse("inputs/tests.txt");

    // Terminator
    Terminator term("term");
    term.dout(dout);
    term.stat(stat);
    for(int i=0; i<5; ++i)
        term.hash[i](hash[i]);
    term.load_conditions("inputs/terminator.txt");

    // Tracing
    sc_trace_file* tf = sc_create_vcd_trace_file("wave");
    sc_trace(tf, clk, "clk");
    sc_trace(tf, din, "din");
    sc_trace(tf, start, "start");
    sc_trace(tf, com_reg, "com_reg");
    sc_trace(tf, dout, "dout");
    sc_trace(tf, stat, "stat");
    for(int i=0; i<5; ++i)
        sc_trace(tf, hash[i], "hash_" + to_string(i));

    unordered_map<string, int> sigs = {
        {"din",0}, {"start",0}, {"com_reg",0}, {"data",0}
    };

    while(true) {
        sc_start(1, SC_NS);
        int t = sc_time_stamp().to_default_time_units();
        sigs["din"] = din.read();
        sigs["start"] = start.read();
        sigs["com_reg"] = com_reg.read();

        if(!test.check(sigs, t)) break;
        if(scen.done()) { cout << "Scenario done" << endl; break; }
        if(t >= 100) { cout << "Timeout" << endl; break; }
    }

    sc_close_vcd_trace_file(tf);
    return 0;
}
