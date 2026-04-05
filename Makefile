BUILD_DIR ?= build
CONFIG ?= Release

.PHONY: all configure build run test clean demo experiments

all: build

configure:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(CONFIG)

build: configure
	cmake --build $(BUILD_DIR)

run: build
	./$(BUILD_DIR)/cache_aware_scheduler --scenario demo --scheduler rms --policy shared

test: build
	ctest --test-dir $(BUILD_DIR) --output-on-failure

demo: build
	./$(BUILD_DIR)/cache_aware_scheduler --scenario demo --scheduler rms --policy colored --trace-csv results/demo_trace.csv

experiments: build
	python3 scripts/run_experiments.py

clean:
	rm -rf $(BUILD_DIR)
