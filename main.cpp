#include <systemc>
#include <iostream>
#include "scenario_loader.h"
#include "source.h"
#include "terminator.h"

using namespace sc_core;
using namespace sc_dt;
using namespace std;

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

    // Terminator
    Terminator term("term");
    term.dout(dout);
    term.stat(stat);
    term.start(start);
    term.din(din);
    term.com_reg(com_reg);
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

    cout << "Starting simulation..." << endl;
    
    // Простая симуляция без верификатора
    while(true) {
        sc_start(1, SC_NS);
        
        if(scen.done()) { 
            cout << "Scenario done" << endl; 
            break; 
        }
        
        // Проверяем таймаут
        if(sc_time_stamp().to_default_time_units() >= 100) { 
            cout << "Timeout" << endl; 
            break; 
        }
    }

    // Проверяем причину остановки симуляции
    if (sc_end_of_simulation_invoked()) {
        cout << "Simulation was stopped by Terminator or sc_stop()" << endl;
    } else {
        cout << "Simulation ended normally" << endl;
    }
    
    // Проверяем, сработал ли Terminator
    if (term.triggered) {
        cout << "Terminator triggered at " << term.trigger_time << endl;
    } else {
        cout << "Terminator did not trigger" << endl;
    }

    sc_close_vcd_trace_file(tf);
    return 0;
} 