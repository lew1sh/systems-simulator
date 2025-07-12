SYSTEMC_HOME = /usr/local/systemc

CXX = g++
CXXFLAGS = -std=c++17 -I$(SYSTEMC_HOME)/include
LDFLAGS = -L$(SYSTEMC_HOME)/lib -Wl,-rpath,$(SYSTEMC_HOME)/lib -lsystemc -lm

SOURCES = main.cpp scenario_loader.cpp source.cpp terminator.cpp
OBJECTS = $(SOURCES:.cpp=.o)


all: simulator


simulator: $(OBJECTS)
	$(CXX) -o $@ $(OBJECTS) $(LDFLAGS)


%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@


run: simulator
	./simulator

# Очистка
clean:
	rm -f *.o simulator  *.vcd

.PHONY: all  clean run
