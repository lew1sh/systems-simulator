#include <systemc>
#include <iostream>
#include <fstream>
#include <thread>
#include <unistd.h>
#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Chart.H>
#include <FL/Fl_Button.H>

using namespace sc_core;
using namespace sc_dt;
using namespace std;

// Класс для загрузки сценария
class ScenarioLoader {
    vector<pair<sc_time, int>> steps;
    size_t current_step = 0;
public:
    void load(const string& filename) {
        ifstream file(filename);
        double time_ns;
        int value;
        while (file >> time_ns >> value) {
            steps.emplace_back(sc_time(time_ns, SC_NS), value);
        }
        sort(steps.begin(), steps.end());
    }

    bool get_next_value(sc_time current_time, int& value) {
        if (current_step >= steps.size()) return false;
        if (steps[current_step].first <= current_time) {
            value = steps[current_step].second;
            current_step++;
            return true;
        }
        return false;
    }
};

// Класс для загрузки условий терминации
class TerminatorLoader {
    vector<tuple<string, string, int>> conditions;
public:
    void load(const string& filename) {
        ifstream file(filename);
        string signal, op;
        int value;
        while (file >> signal >> op >> value) {
            conditions.emplace_back(signal, op, value);
        }
    }

    bool check_conditions(int signal_value) {
        for (const auto& [signal, op, value] : conditions) {
            if ((op == ">" && signal_value > value) ||
                (op == ">=" && signal_value >= value) ||
                (op == "<" && signal_value < value) ||
                (op == "<=" && signal_value <= value) ||
                (op == "==" && signal_value == value) ||
                (op == "!=" && signal_value != value)) {
                return true;
            }
        }
        return false;
    }
};


class FLgraph {
    Fl_Window* window;
    Fl_Chart* chart;
    Fl_Value_Output* value_display;
    vector<double> time_values;
    vector<double> signal_values;
    int max_points = 100;
    bool keep_open = true;
    
public:
    FLgraph() {
        window = new Fl_Window(800, 600, "SystemC Signal Visualizer");
        chart = new Fl_Chart(20, 20, 760, 500, "Signal Value");
        chart->type(FL_LINE_CHART);
        chart->bounds(-10, 10);
        
        value_display = new Fl_Value_Output(350, 530, 100, 30, "Current Value");
        window->end();
        window->show();
    }
    
    void update(double time, double value) {
        time_values.push_back(time);
        signal_values.push_back(value);
        
        if (time_values.size() > max_points) {
            time_values.erase(time_values.begin());
            signal_values.erase(signal_values.begin());
        }
        
        chart->clear();
        for (size_t i = 0; i < time_values.size(); ++i) {
            chart->add(signal_values[i], "", FL_RED);
        }
        
        value_display->value(value);
        
        // Добавляем небольшую задержку для визуализации
        usleep(50000); // 50ms задержка
        Fl::check();
    }
    
    void keep_open_after_simulation() {
        while (keep_open) {
            Fl::wait(0.11); // Медленное обновление после завершения
        }
    }
    
    ~FLgraph() {
        keep_open = false;
        delete window;
    }
};


// Модуль источника с поддержкой сценария
SC_MODULE(ControlledSource) {
    sc_out<int> out;
    sc_in<bool> clk;
    ScenarioLoader* scenario;
    
    void generate() {
        int value;
        while (true) {
            if (scenario->get_next_value(sc_time_stamp(), value)) {
                out.write(value);
            }
            wait();
        }
    }

    SC_CTOR(ControlledSource) : scenario(nullptr) {
        SC_THREAD(generate);
        sensitive << clk.pos();
    }
    
    void set_scenario(ScenarioLoader* scen) { scenario = scen; }
};

// Основная функция
int sc_main(int argc, char* argv[]) {
    // Загрузка сценария и условий
    ScenarioLoader scenario;
    scenario.load("inputs/scenario.txt");
    
    TerminatorLoader terminator;
    terminator.load("inputs/conditions.txt");

    // Создание системы
    sc_signal<int> data_sig;
    sc_clock clk("clk", 10, SC_NS);
    
    ControlledSource src("Source");
    src.out(data_sig);
    src.clk(clk);
    src.set_scenario(&scenario);

    // Графический интерфейс
    FLgraph visualizer;
    auto fltk_run = [&]() {
        while (visualizer.should_run()) {
            usleep(50000);
            Fl::check();
        }
    };
    thread fltk_thread(fltk_run);

    // Главный цикл симуляции
    try {
        sc_start(1, SC_NS); // Инициализация
        
        while (visualizer.should_run()) {
            visualizer.update(data_sig.read());
            
            if (terminator.check_conditions(data_sig.read())) {
                cout << "Termination condition met!" << endl;
                break;
            }
            
            sc_start(10, SC_NS);
        }
    } catch (...) {
        cerr << "Simulation error" << endl;
    }

    fltk_thread.join();
    return 0;
}