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
                cout << "Signal " << m[1] << " not found at time " << time << endl;
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

// Главная функция
int sc_main(int, char*[]) {
    sc_signal<int> din, start, com_reg, data;
    sc_clock clk("clk", 1, SC_NS);
    
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
    
    sc_trace_file* tf = sc_create_vcd_trace_file("wave");
    sc_trace(tf, clk, "clk");
    sc_trace(tf, din, "din");
    sc_trace(tf, start, "start");
    sc_trace(tf, com_reg, "com_reg");
    
    unordered_map<string, int> sigs = {{"din",0},{"start",0},{"com_reg",0},{"data",0}};
    
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