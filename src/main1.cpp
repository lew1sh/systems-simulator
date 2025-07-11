#include <systemc>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <unordered_map>
#include <functional>
using namespace sc_core;
using namespace sc_dt;
using namespace std;

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
// Класс для чтения и парсинга файла сценария
class ScenarioLoader {
    struct ScenarioStep {
        int clock_cycle;
        int signal_value;
    };
    vector<ScenarioStep> steps;
    size_t current_step = 0;
public:
    void load(const string& filename) {
        ifstream file(filename);
        string line;
        
        while (getline(file, line)) {
            if (line.empty() || line[0] == '#') continue;
            
            istringstream iss(line);
            ScenarioStep step;
            if (!(iss >> step.clock_cycle >> step.signal_value)) {
                cerr << "Error parsing scenario line: " << line << endl;
                continue;
            }
            steps.push_back(step);
        }
    }
    
    bool get_next_step(int& cycle, int& value) {
        if (current_step >= steps.size()) return false;
        
        cycle = steps[current_step].clock_cycle;
        value = steps[current_step].signal_value;
        current_step++;
        return true;
    }
};

// Класс для чтения условий терминатора
class TerminatorLoader {
    struct TerminationCondition {
        string signal_name;
        string operation;
        int value;
    };
    vector<TerminationCondition> conditions;
public:
    void load(const string& filename) {
        ifstream file(filename);
        string line;
        
        while (getline(file, line)) {
            if (line.empty() || line[0] == '#') continue;
            
            istringstream iss(line);
            TerminationCondition cond;
            if (!(iss >> cond.signal_name >> cond.operation >> cond.value)) {
                cerr << "Error parsing condition line: " << line << endl;
                continue;
            }
            conditions.push_back(cond);
        }
    }
    
    bool check_conditions(const unordered_map<string, int>& current_values) {
        for (const auto& cond : conditions) {
            if (!current_values.count(cond.signal_name)) continue;
            
            int signal_value = current_values.at(cond.signal_name);
            bool result = false;
            
            if (cond.operation == ">") result = (signal_value > cond.value);
            else if (cond.operation == ">=") result = (signal_value >= cond.value);
            else if (cond.operation == "<") result = (signal_value < cond.value);
            else if (cond.operation == "<=") result = (signal_value <= cond.value);
            else if (cond.operation == "==") result = (signal_value == cond.value);
            else if (cond.operation == "!=") result = (signal_value != cond.value);
            
            if (result) {
                cout << "Termination condition met: " << cond.signal_name 
                     << " " << cond.operation << " " << cond.value 
                     << " (actual: " << signal_value << ")" << endl;
                return true;
            }
        }
        return false;
    }
};

// Модифицированные модули SystemC
SC_MODULE(ControlledSource) {
    sc_out<int> out;
    sc_in<bool> clk;
    ScenarioLoader* scenario;
    
    void generate() {
        int cycle, value;
        if (scenario->get_next_step(cycle, value)) {
            out.write(value);
            cout << "Scenario step: cycle=" << cycle << ", value=" << value << endl;
        }
    }

    SC_CTOR(ControlledSource) : scenario(nullptr) {
        SC_METHOD(generate);
        sensitive << clk.pos();
        dont_initialize();
    }
    
    void set_scenario(ScenarioLoader* scen) { scenario = scen; }
};

int sc_main(int argc, char* argv[]) {
    // Загрузка файлов
    ScenarioLoader scenario;
    scenario.load("inputs/scenario.txt");
    
    TerminatorLoader terminator;
    terminator.load("inputs/conditions.txt");
    
    // Создание системы
    sc_signal<int> data_sig;
    sc_clock clk("clk", 1, SC_NS);
    
    ControlledSource src("Source");
    Sink snk("Sink");
    Monitor mon("Monitor");
    
    src.out(data_sig);
    src.clk(clk);
    src.set_scenario(&scenario);
    
    snk.in(data_sig);
    snk.clk(clk);
    
    mon.signal(data_sig);
    mon.clk(clk);
    
    // Настройка трассировки
    sc_trace_file* tf = sc_create_vcd_trace_file("outputs/waveform");
    sc_trace(tf, clk, "clk");
    sc_trace(tf, data_sig, "data");
    
    // Открытие лог-файла
    ofstream logfile("outputs/log.csv");
    logfile << "time_ns,signal_value\n";
    
    // Главный цикл симуляции
    while (true) {
        sc_start(1, SC_NS);
        
        // Логирование
        logfile << sc_time_stamp().to_default_time_units() << "," 
               << data_sig.read() << "\n";
        
        // Проверка условий терминатора
        unordered_map<string, int> current_values = {
            {"data", data_sig.read()}
        };
        
        if (terminator.check_conditions(current_values)) {
            cout << "Simulation terminated by condition at " << sc_time_stamp() << endl;
            break;
        }
        
        // Проверка завершения сценария
        if (sc_core::sc_get_status() & sc_core::SC_STOPPED) {
            cout << "Simulation finished normally at " << sc_time_stamp() << endl;
            break;
        }
    }
    
    // Завершение
    logfile.close();
    sc_close_vcd_trace_file(tf);
    return 0;
}