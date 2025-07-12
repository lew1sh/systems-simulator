#include <systemc>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <unordered_map>
#include <functional>
#include <variant>
#include <regex>
#include <algorithm>
#include <climits>

using namespace sc_core;
using namespace sc_dt;
using namespace std;

class SimpleTestVerifier {
public:
    struct TestCondition {
        int time_start;
        int time_end;
        string condition;
    };

    void parse(const string& filename) {
        ifstream file(filename);
        string line;
        smatch matches;

        regex re(R"(\[\s*(\d+)(?:\.\.(\d+)?)*\s*\]\s*[@#]\s*\((.+)\))");

        while (getline(file, line)) {
            line = trim(line);
            if (line.empty()) continue;

            if (regex_match(line, matches, re)) {
                TestCondition cond;
                cond.condition = matches[3];

                if (matches[2].matched && !matches[2].str().empty()) {
                    cond.time_start = stoi(matches[1]);
                    cond.time_end = stoi(matches[2]);
                } else if (!matches[2].matched) {
                    cond.time_start = stoi(matches[1]);
                    cond.time_end = cond.time_start;
                } else {
                    cond.time_start = stoi(matches[1]);
                    cond.time_end = INT_MAX;
                }

                conditions.push_back(cond);
            }
        }
    }

    bool check_conditions(const unordered_map<string, int>& signals, int current_time) {
        for (const auto& cond : conditions) {
            if (current_time >= cond.time_start && current_time <= cond.time_end) {
                if (!evaluate_condition(cond.condition, signals)) {
                    cout << "Condition failed at time " << current_time 
                         << ": " << cond.condition << endl;
                    return false;
                }
            }
        }
        return true;
    }

private:
    vector<TestCondition> conditions;

    bool evaluate_condition(const string& cond, const unordered_map<string, int>& signals) {
        smatch matches;
        regex pattern(R"((\w+)\s*(==|!=|>|<|>=|<=)\s*(\d+|0x[0-9a-fA-F]+))");
        if (regex_match(cond, matches, pattern)) {
            string signal = matches[1];
            string op = matches[2];
            int value = (matches[3].str().find("0x") == 0) ? 
                        stoi(matches[3].str(), nullptr, 16) : stoi(matches[3]);

            if (signals.find(signal) == signals.end()) return false;
            int signal_value = signals.at(signal);

            if (op == "==") return signal_value == value;
            if (op == "!=") return signal_value != value;
            if (op == ">") return signal_value > value;
            if (op == "<") return signal_value < value;
            if (op == ">=") return signal_value >= value;
            if (op == "<=") return signal_value <= value;
        }
        return false;
    }

    static string trim(const string& s) {
        size_t start = s.find_first_not_of(" \t");
        size_t end = s.find_last_not_of(" \t");
        return (start == string::npos) ? "" : s.substr(start, end - start + 1);
    }
};

SC_MODULE(Sink) {
    sc_in<int> in;
    sc_in<bool> clk;

    void receive() {
        cout << "Sink received: " << in.read() << " at " << sc_time_stamp() << endl;
    }

    SC_CTOR(Sink) {
        SC_METHOD(receive);
        sensitive << clk.pos();
        dont_initialize();
    }
};

class ScenarioLoader {
    struct ScenarioStep {
        sc_time time;
        string signal_name;
        int value;
    };
    vector<ScenarioStep> steps;
    size_t current_step = 0;
    
public:
    void load(const string& filename) {
        ifstream file(filename);
        if (!file.is_open()) {
            cerr << "Error: Could not open scenario file: " << filename << endl;
            return;
        }

        string line;
        int line_num = 0;
        while (getline(file, line)) {
            line_num++;
            line = trim(line);
            if (line.empty() || line[0] == '#') continue;

            istringstream iss(line);
            ScenarioStep step;
            double time_ns;
            string value_str;

            if (!(iss >> time_ns >> step.signal_name >> value_str)) {
                cerr << "Error parsing scenario line " << line_num << ": " << line << endl;
                continue;
            }

            if (!is_valid_signal_name(step.signal_name)) {
                cerr << "Invalid signal name '" << step.signal_name << "' at line " << line_num << endl;
                continue;
            }

            try {
                if (value_str.substr(0, 2) == "0x") {
                    step.value = stoi(value_str.substr(2), nullptr, 16);
                } else {
                    step.value = stoi(value_str);
                }
            } catch (...) {
                cerr << "Invalid value at line " << line_num << endl;
                continue;
            }

            if (time_ns < 0) {
                cerr << "Negative time " << time_ns << " at line " << line_num << endl;
                continue;
            }

            step.time = sc_time(time_ns, SC_NS);
            steps.push_back(step);
        }

        sort(steps.begin(), steps.end(), [](const auto& a, const auto& b) {
            return a.time < b.time;
        });
    }

    bool get_next_step(sc_time current_time, string& signal_name, int& value) {
        while (current_step < steps.size() && steps[current_step].time <= current_time) {
            signal_name = steps[current_step].signal_name;
            value = steps[current_step].value;
            current_step++;
            return true;
        }
        return false;
    }

    bool is_complete() const {
        return current_step >= steps.size();
    }

    void reset() {
        current_step = 0;
    }

private:
    static string trim(const string& str) {
        size_t first = str.find_first_not_of(" \t");
        if (string::npos == first) return "";
        size_t last = str.find_last_not_of(" \t");
        return str.substr(first, (last - first + 1));
    }

    bool is_valid_signal_name(const string& name) {
        static const regex valid_name_regex("^[a-zA-Z_][a-zA-Z0-9_]*$");
        return regex_match(name, valid_name_regex);
    }
};

SC_MODULE(MultiSignalSource) {
    sc_out<int> din;
    sc_out<int> start;
    sc_out<int> com_reg;
    sc_in<bool> clk;
    ScenarioLoader* scenario;

    void generate() {
        if (!scenario) return;
        string signal_name;
        int value;
        while (scenario->get_next_step(sc_time_stamp(), signal_name, value)) {
            if (signal_name == "din") din.write(value);
            else if (signal_name == "start") start.write(value);
            else if (signal_name == "com_reg") com_reg.write(value);
            else cerr << "Unknown signal: " << signal_name << endl;
        }
    }

    SC_CTOR(MultiSignalSource) : scenario(nullptr) {
        SC_METHOD(generate);
        sensitive << clk.pos();
        dont_initialize();
    }

    void set_scenario(ScenarioLoader* scen) {
        scenario = scen;
        din.write(0);
        start.write(0);
        com_reg.write(0);
    }
};

SC_MODULE(Monitor) {
    sc_in<int> signal;
    sc_in<bool> clk;

    void watch() {
        cout << "[Monitor] Signal = " << signal.read() << " at " << sc_time_stamp() << endl;
    }

    SC_CTOR(Monitor) {
        SC_METHOD(watch);
        sensitive << clk.pos();
        dont_initialize();
    }
};

class TerminatorLoader {
    struct Condition {
        string var;
        int mask = 0xFFFFFFFF;
        int expected;
    };
    vector<Condition> conditions;

public:
    void load(const string& filename) {
        ifstream file(filename);
        string line;

        while (getline(file, line)) {
            line = trim(line);
            if (line.empty() || line[0] == '#') continue;

            Condition cond;
            size_t bracket_pos = line.find('[');

            if (bracket_pos != string::npos) {
                cond.var = trim(line.substr(0, bracket_pos));
                size_t colon_pos = line.find(':', bracket_pos);
                int start = stoi(line.substr(bracket_pos + 1, colon_pos - bracket_pos - 1));
                int end = stoi(line.substr(colon_pos + 1, line.find(']') - colon_pos - 1));
                cond.mask = ((1 << (end - start + 1)) - 1) << start;
                size_t eq_pos = line.find("==");
                cond.expected = stoi(line.substr(eq_pos + 2), nullptr, 16) << start;
            } else {
                size_t eq_pos = line.find("==");
                cond.var = trim(line.substr(0, eq_pos));
                cond.expected = stoi(line.substr(eq_pos + 2), nullptr, 16);
            }

            conditions.push_back(cond);
        }
    }

    bool check(const unordered_map<string, int>& signals) {
        for (const auto& cond : conditions) {
            if (!signals.count(cond.var) || 
                (signals.at(cond.var) & cond.mask) != cond.expected) {
                return false;
            }
        }
        return true;
    }

private:
    static string trim(const string& s) {
        size_t start = s.find_first_not_of(" \t");
        size_t end = s.find_last_not_of(" \t");
        return (start == string::npos) ? "" : s.substr(start, end - start + 1);
    }
};

int sc_main(int argc, char* argv[]) {
    sc_signal<int> din_sig("din");
    sc_signal<int> start_sig("start");
    sc_signal<int> com_reg_sig("com_reg");
    sc_signal<int> data_sig("data_sig");
    sc_clock clk("clk", 1, SC_NS);

    MultiSignalSource src("Source");
    Sink snk("Sink");
    Monitor mon("Monitor");

    src.din(din_sig);
    src.start(start_sig);
    src.com_reg(com_reg_sig);
    src.clk(clk);

    snk.in(din_sig);
    snk.clk(clk);

    mon.signal(din_sig);
    mon.clk(clk);

    ScenarioLoader scenario;
    scenario.load("inputs/scenario.txt");
    src.set_scenario(&scenario);

    TerminatorLoader terminator;
    terminator.load("inputs/terminator.txt");

    SimpleTestVerifier test_verifier;
    test_verifier.parse("inputs/tests.txt");

    sc_trace_file* tf = sc_create_vcd_trace_file("waveform");
    sc_trace(tf, clk, "clk");
    sc_trace(tf, din_sig, "din");
    sc_trace(tf, start_sig, "start");
    sc_trace(tf, com_reg_sig, "com_reg");

    ofstream logfile("simulation_log.csv");
    logfile << "time_ns,din,start,com_reg\n";

    unordered_map<string, int> signals = {
        {"din", 0}, {"start", 0}, {"com_reg", 0}
    };

    while (true) {
        sc_start(1, SC_NS);
        int t = sc_time_stamp().to_default_time_units();

        signals["din"] = din_sig.read();
        signals["start"] = start_sig.read();
        signals["com_reg"] = com_reg_sig.read();

        logfile << t << ","
                << signals["din"] << ","
                << signals["start"] << ","
                << signals["com_reg"] << "\n";

        if (!test_verifier.check_conditions(signals, t)) break;
        if (terminator.check(signals)) break;
        if (scenario.is_complete()) break;
        if (t >= 100) break;
    }

    logfile.close();
    sc_close_vcd_trace_file(tf);
    return 0;
}
