#include <systemc>
#include <iostream>
#include <fstream>
#include <vector>

using namespace sc_core;
using namespace sc_dt;
using namespace std;

// SCENARIO: считывает значения из файла и подаёт по одному за такт
SC_MODULE(Scenario) {
    sc_out<int> out;
    sc_in<bool> clk;

    vector<int> values;
    int index = 0;

    void load_values(const string& filename) {
        ifstream infile(filename);
        int val;
        while (infile >> val) {
            values.push_back(val);
        }
    }

    void generate() {
        if (index < values.size()) {
            out.write(values[index++]);
        } else {
            out.write(0); // ничего нового, можно нули
        }
    }

    SC_CTOR(Scenario) {
        SC_METHOD(generate);
        sensitive << clk.pos();
        dont_initialize();

        load_values("input.txt");  // загрузка при конструировании
    }
};

// TERMINATOR: завершает симуляцию после N тактов
SC_MODULE(Terminator) {
    sc_in<bool> clk;
    int ticks = 0;
    int max_ticks;

    void check() {
        ticks++;
        if (ticks >= max_ticks) {
            cout << "[Terminator] Simulation stopping at " << sc_time_stamp() << endl;
            sc_stop();
        }
    }

    SC_CTOR(Terminator) : max_ticks(10) {
        SC_METHOD(check);
        sensitive << clk.pos();
        dont_initialize();
    }
};

// Приёмник (без изменений)
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

// Монитор (без изменений)
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
    sc_clock clk("clk", 1, SC_NS);

    Scenario scen("Scenario");
    Sink snk("Sink");
    Monitor mon("Monitor");
    Terminator term("Terminator");

    scen.out(data_sig);
    scen.clk(clk);

    snk.in(data_sig);
    snk.clk(clk);

    mon.signal(data_sig);
    mon.clk(clk);

    term.clk(clk);

    // Трассировка
    sc_trace_file *tf = sc_create_vcd_trace_file("waveform");
    sc_trace(tf, clk, "clk");
    sc_trace(tf, data_sig, "data_sig");

    // Автоматическая симуляция (без ожидания нажатий)
    sc_start();  // sc_stop будет вызван терминатором

    sc_close_vcd_trace_file(tf);
    return 0;
}
