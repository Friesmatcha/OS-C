CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -Isrc -Itests

CORE_SOURCES :=
TEST_SOURCES := tests/FileSystemTests.cpp $(CORE_SOURCES)
CLI_SOURCES := src/cli/main.cpp src/cli/CommandParser.cpp $(CORE_SOURCES)

.PHONY: all test clean

all: build/osc_fs build/osc_fs_tests

build:
	mkdir -p build

build/osc_fs_tests: $(TEST_SOURCES) | build
	$(CXX) $(CXXFLAGS) $(TEST_SOURCES) -o $@

build/osc_fs: $(CLI_SOURCES) | build
	$(CXX) $(CXXFLAGS) $(CLI_SOURCES) -o $@

test: build/osc_fs_tests
	./build/osc_fs_tests

clean:
	rm -rf build

