#include <systemc>
#include <iostream>
using namespace sc_core;
using namespace sc_dt;
using namespace std;

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

    // Пошаговая симуляция: 1 такт → Enter → следующий
    for (int i = 0; i < 10; ++i) {
        cout << "\n>>> Step " << i + 1 << ": Press Enter to continue...";
        cin.get();  // ожидание нажатия клавиши

        sc_start(1, SC_NS);  // один такт симуляции
    }

    sc_close_vcd_trace_file(tf);
    return 0;
}
