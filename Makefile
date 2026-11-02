CXX := clang++
CXXFLAGS := -std=c++17 -Wall -Wextra -O3 -pthread -Iinclude
SRC_DIR := src
TEST_DIR := tests
BENCH_DIR := benchmarks
BUILD_DIR := build
BIN_DIR := bin

SRCS := $(wildcard $(SRC_DIR)/*.cpp)
OBJS := $(patsubst $(SRC_DIR)/%.cpp, $(BUILD_DIR)/%.o, $(SRCS))

TEST_SRCS := $(wildcard $(TEST_DIR)/*.cpp)
TEST_OBJS := $(patsubst $(TEST_DIR)/%.cpp, $(BUILD_DIR)/%.o, $(TEST_SRCS))

.PHONY: all test benchmark clean run

all: $(BIN_DIR)/rtp_engine $(BIN_DIR)/test_runner $(BIN_DIR)/benchmark_throughput

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(TEST_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BIN_DIR)/rtp_engine: main.cpp $(OBJS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) main.cpp $(OBJS) -o $@

$(BIN_DIR)/test_runner: $(TEST_OBJS) $(OBJS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(TEST_OBJS) $(OBJS) -o $@

$(BIN_DIR)/benchmark_throughput: $(BENCH_DIR)/benchmark_throughput.cpp $(OBJS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(BENCH_DIR)/benchmark_throughput.cpp $(OBJS) -o $@

test: $(BIN_DIR)/test_runner
	./$(BIN_DIR)/test_runner

benchmark: $(BIN_DIR)/benchmark_throughput
	./$(BIN_DIR)/benchmark_throughput

run: $(BIN_DIR)/rtp_engine
	./$(BIN_DIR)/rtp_engine

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)
