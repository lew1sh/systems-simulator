SYSTEMC_HOME = /usr/local/systemc

CXX = g++
CXXFLAGS = -std=c++17 -I$(SYSTEMC_HOME)/include
LDFLAGS = -L$(SYSTEMC_HOME)/lib -Wl,-rpath,$(SYSTEMC_HOME)/lib -lsystemc -lm

# Исходные файлы
SOURCES = main.cpp scenario_loader.cpp source.cpp terminator.cpp
OBJECTS = $(SOURCES:.cpp=.o)

# Основные цели
all: simulator

# Основная программа
simulator: $(OBJECTS)
	$(CXX) -o $@ $(OBJECTS) $(LDFLAGS)

# Компиляция объектных файлов
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

test_terminator: test_terminator.cpp
	$(CXX) $(CXXFLAGS) -o test_terminator test_terminator.cpp $(LDFLAGS)

# Запуск тестов
test: test_terminator
	./test_terminator


run: simulator
	./simulator

# Очистка
clean:
	rm -f *.o simulator test_terminator *.vcd

.PHONY: all test clean run
