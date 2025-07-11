
SYSTEMC_HOME ?= $(shell brew --prefix systemc)
SYSTEMC_LIB ?= $(SYSTEMC_HOME)/lib

CXX = g++
CXXFLAGS = -std=c++17 -I$(SYSTEMC_HOME)/include
LDFLAGS = -L$(SYSTEMC_LIB) -Wl,-rpath,$(SYSTEMC_LIB)
LIBS = -lsystemc -lm

SRCS = $(wildcard main3newfail.cpp)
OBJS = $(SRCS:.cpp=.o)
TARGET = sim

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) -o $@ $(OBJS) $(LDFLAGS) $(LIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

run: $(TARGET)
	DYLD_LIBRARY_PATH=$(SYSTEMC_LIB) ./$(TARGET)

.PHONY: all clean run
