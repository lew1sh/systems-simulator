#ifndef SOURCE_H
#define SOURCE_H

#include <systemc>
#include <string>
#include "scenario_loader.h"

using namespace sc_core;
using namespace std;

// Модуль источника сигналов
SC_MODULE(Source) {
    sc_out<int> din, start, com_reg;
    sc_in<bool> clk;
    ScenarioLoader* scen;
    
    void gen();
    
    SC_CTOR(Source) : scen(nullptr) {
        SC_METHOD(gen);
        sensitive << clk.pos();
        dont_initialize();
    }
    void set_scen(ScenarioLoader* s) { scen = s; }
};

#endif // SOURCE_H 