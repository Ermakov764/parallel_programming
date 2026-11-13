CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -pthread

.PHONY: all demo clean

all: filter

filter: main.cpp
	$(CXX) $(CXXFLAGS) -o filter main.cpp

demo: filter
	mkdir -p samples
	./filter --demo

clean:
	rm -f filter
