CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -pthread
CPPFLAGS = -Iinclude
SOURCES = main.cpp $(wildcard src/*.cpp)

.PHONY: all demo clean
all: filter
filter: $(SOURCES) $(wildcard include/*.hpp)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -o $@ $(SOURCES)
demo: filter
	mkdir -p samples
	./filter --demo
clean:
	rm -f filter filter.exe
