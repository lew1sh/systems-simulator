#include "source.h"
#include <iostream>

void Source::gen() {
    while (true) {
        string name; int val;
        if(!scen->get_next(sc_time_stamp(), name, val)) break;
        if(name=="din") din.write(val);
        else if(name=="start") start.write(val);
        else if(name=="com_reg") com_reg.write(val);
        cout << "Set " << name << "=" << val << " at " << sc_time_stamp() << endl;
    }
} 