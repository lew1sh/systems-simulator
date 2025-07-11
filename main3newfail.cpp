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

// Модуль Sink (приемник)
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

// Модуль Monitor (монитор)
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

// Класс для загрузки сценария
class ScenarioLoader {
    struct ScenarioStep {
        sc_time time;
        int signal_value;
    };
    vector<ScenarioStep> steps;
    size_t current_step = 0;
    
public:
    void load(const string& filename) {
        ifstream file(filename);
        string line;
        while (getline(file, line)) {
            line = trim(line);
            if (line.empty() || line[0] == '#') continue;
            
            istringstream iss(line);
            double time_ns;
            int value;
            
            if (!(iss >> time_ns >> value)) {
                cerr << "Error parsing scenario line: " << line << endl;
                continue;
            }
            
            steps.push_back({sc_time(time_ns, SC_NS), value});
        }
        
        // Сортируем по времени
        sort(steps.begin(), steps.end(), 
            [](const ScenarioStep& a, const ScenarioStep& b) {
                return a.time < b.time;
            });
    }
    
  bool get_next_value(sc_time current_time, int& value) {
    if (current_step >= steps.size()) {
        cout << "No more steps in scenario" << endl;
        return false;
    }
    
    cout << "Checking step " << current_step << ": " 
         << steps[current_step].time << " <= " << current_time << endl;
         
    if (steps[current_step].time <= current_time) {
        value = steps[current_step].signal_value;
        current_step++;
        cout << "Applied step: value = " << value << endl;
        return true;
    }
    return false;
}
    
private:
    static string trim(const string& str) {
        size_t first = str.find_first_not_of(" \t");
        if (string::npos == first) return "";
        size_t last = str.find_last_not_of(" \t");
        return str.substr(first, (last - first + 1));
    }
};

// Класс для загрузки условий терминации
class TerminatorLoader {
    struct Condition {
        string signal;
        string op;
        int value;
    };
    vector<Condition> conditions;
    
public:
    void load(const string& filename) {
        ifstream file(filename);
        if (!file.is_open()) {
            cerr << "Error: Could not open scenario file: " << filename << endl;
            return;
        }
        string line;
        while (getline(file, line)) {
            line = trim(line);
            if (line.empty() || line[0] == '#') continue;
            
            istringstream iss(line);
            Condition cond;
            if (!(iss >> cond.signal >> cond.op >> cond.value)) {
                cerr << "Error parsing condition line: " << line << endl;
                continue;
            }
            conditions.push_back(cond);
        }
    }
    
    bool check_conditions(int signal_value) {
        for (const auto& cond : conditions) {
            bool result = false;
            
            if (cond.op == ">") result = (signal_value > cond.value);
            else if (cond.op == ">=") result = (signal_value >= cond.value);
            else if (cond.op == "<") result = (signal_value < cond.value);
            else if (cond.op == "<=") result = (signal_value <= cond.value);
            else if (cond.op == "==") result = (signal_value == cond.value);
            else if (cond.op == "!=") result = (signal_value != cond.value);
            
            if (result) {
                cout << "Termination condition met: " << cond.signal 
                     << " " << cond.op << " " << cond.value 
                     << " (actual: " << signal_value << ")" << endl;
                return true;
            }
        }
        return false;
    }
    
private:
    static string trim(const string& str) {
        size_t first = str.find_first_not_of(" \t");
        if (string::npos == first) return "";
        size_t last = str.find_last_not_of(" \t");
        return str.substr(first, (last - first + 1));
    }
};

// Управляемый источник данных
SC_MODULE(ControlledSource) {
    sc_out<int> out;
    sc_in<bool> clk;
    ScenarioLoader* scenario;
    
    void generate() {
    if (!scenario) return;  // Добавляем проверку
    
    int value;
    if (scenario->get_next_value(sc_time_stamp(), value)) {
        out.write(value);
        cout << "Source generated: " << value << " at " << sc_time_stamp() << endl;
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
    // Создание и настройка системы
    sc_signal<int> data_sig;
    sc_clock clk("clk", 1, SC_NS);  // Такт 1 нс
    
    // Создание модулей
    ControlledSource src("Source");
    Sink snk("Sink");
    Monitor mon("Monitor");
    
    // Подключение модулей
    src.out(data_sig);
    src.clk(clk);
    
    snk.in(data_sig);
    snk.clk(clk);
    
    mon.signal(data_sig);
    mon.clk(clk);
    
    // Загрузка сценария и условий
    ScenarioLoader scenario;
    scenario.load("inputs/scenario.txt");
    src.set_scenario(&scenario);
    
    TerminatorLoader terminator;
    terminator.load("inputs/conditions.txt");
    
    // Настройка трассировки
    sc_trace_file* tf = sc_create_vcd_trace_file("waveform");
    sc_trace(tf, clk, "clk");
    sc_trace(tf, data_sig, "data");
    
    // Логирование в файл
    ofstream logfile("simulation_log.csv");
    logfile << "time_ns,signal_value\n";
    
    // Основной цикл симуляции
    try {
        while (true) {
            sc_start(1, SC_NS);  // Выполняем по 1 такту
            
            // Логирование текущего состояния
            logfile << sc_time_stamp().to_default_time_units() << ","
                   << data_sig.read() << "\n";
            
            // Проверка условий завершения
            if (terminator.check_conditions(data_sig.read())) {
                cout << "Simulation terminated by condition at " << sc_time_stamp() << endl;
                break;
            }
            
            // Проверка завершения сценария
            if (sc_time_stamp() >= sc_time(100, SC_NS)) {  // Максимальное время симуляции
                cout << "Simulation finished (timeout) at " << sc_time_stamp() << endl;
                break;
            }
        }
    } catch (const sc_report& e) {
        cerr << "Simulation error: " << e.what() << endl;
    }
    
    // Завершение
    logfile.close();
    sc_close_vcd_trace_file(tf);
    
    return 0;
}