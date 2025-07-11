#include <systemc>
#include <iostream>
#include <fstream>
using namespace sc_core;
using namespace sc_dt;
using namespace std;

// Класс сценария для управления симуляцией
class Scenario {
public:
    virtual void setup() = 0;
    virtual void teardown() = 0;
    virtual ~Scenario() {}
};

// Класс терминатора для определения условий завершения симуляции
class Terminator {
public:
    virtual bool is_done() = 0;
    virtual ~Terminator() {}
};

// Наш конкретный сценарий
class SimpleScenario : public Scenario {
    ofstream logfile;
public:
    SimpleScenario() : logfile("simulation_log.txt") {
        if (!logfile.is_open()) {
            cerr << "Error opening log file!" << endl;
        }
    }
    
    void setup() override {
        logfile << "Simulation started at " << sc_time_stamp() << endl;
    }
    
    void teardown() override {
        logfile << "Simulation ended at " << sc_time_stamp() << endl;
        logfile.close();
    }
    
    void log_step(int step) {
        logfile << "Step " << step << " completed at " << sc_time_stamp() << endl;
    }
};

// Наш конкретный терминатор
class StepTerminator : public Terminator {
    int max_steps;
    int current_step;
public:
    StepTerminator(int steps) : max_steps(steps), current_step(0) {}
    
    bool is_done() override {
        return ++current_step > max_steps;
    }
};

SC_MODULE(Source) {
    sc_out<int> out;
    sc_in<bool> clk;
    int value = 0;

    void generate() {
        out.write(value++);
    }

    SC_CTOR(Source) {
        SC_METHOD(generate);
        sensitive << clk.pos();
        dont_initialize();
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

int sc_main(int argc, char* argv[]) {
    sc_signal<int> data_sig;
    sc_clock clk("clk", 1, SC_NS);  // период 1 наносекунда

    // Создаём модули
    Source src("Source");
    Sink snk("Sink");
    Monitor mon("Monitor");

    // Подключаем сигналы
    src.out(data_sig);
    src.clk(clk);

    snk.in(data_sig);
    snk.clk(clk);

    mon.signal(data_sig);
    mon.clk(clk);

    // Трассировка в файл
    sc_trace_file *tf = sc_create_vcd_trace_file("waveform");
    sc_trace(tf, clk, "clk");
    sc_trace(tf, data_sig, "data_sig");

    // Создаем сценарий и терминатор
    SimpleScenario scenario;
    StepTerminator terminator(10); // 10 шагов симуляции
    
    scenario.setup();
    
    // Пошаговая симуляция с использованием терминатора
    int step = 1;
    while (!terminator.is_done()) {
        cout << "\n>>> Step " << step << ": Press Enter to continue...";
        cin.get();  // ожидание нажатия клавиши

        sc_start(1, SC_NS);  // один такт симуляции
        scenario.log_step(step);
        step++;
    }
    
    scenario.teardown();
    sc_close_vcd_trace_file(tf);
    return 0;
}