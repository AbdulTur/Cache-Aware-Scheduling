# Cache-Aware Scheduling for Real-Time Systems

This project simulates periodic real-time workloads and studies how cache interference changes execution-time predictability. The simulator compares shared-cache scheduling against cache partitioning and cache coloring, measures cache-related preemption delay (CRPD), and exports repeatable results for the course report and presentation.

## Project Goals

- Implement periodic real-time scheduling with `RMS` and `EDF`.
- Model cache interference caused by task preemption.
- Measure CRPD, response time, deadline misses, and cache behavior.
- Compare baseline shared-cache execution with cache-aware isolation policies.
- Generate repeatable traces, CSV summaries, and plots for evaluation.

## Repository Layout

```text
.
├── CMakeLists.txt
├── Makefile
├── inc/
├── src/
├── scripts/
├── results/
└── docs/
```

## Requirements

- C compiler with C11 support
- CMake 3.16+
- Python 3.10+ with `numpy`, `pandas`, `matplotlib`

The project was designed to work on WSL and native Windows. The simulator is standard C and does not depend on Linux-only APIs.

## Build Instructions

### WSL / Linux

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Windows

From PowerShell with Visual Studio Build Tools or MinGW installed:

```powershell
cmake -S . -B build
cmake --build build --config Release
```

## Run Instructions

List built-in scenarios:

```bash
./build/cache_aware_scheduler --list-scenarios
```

Run the baseline shared-cache simulation:

```bash
./build/cache_aware_scheduler --scenario demo --scheduler rms --policy shared
```

Run a cache-colored comparison and save the event trace:

```bash
./build/cache_aware_scheduler --scenario demo --scheduler rms --policy colored --trace-csv results/demo_trace.csv
```

Export one-run summary CSV:

```bash
./build/cache_aware_scheduler --scenario stress --scheduler edf --policy partitioned --summary-csv results/stress_partitioned.csv
```

## Built-In Scenarios

- `demo`: three periodic tasks with intentional cache-set collisions
- `harmonic`: four harmonic tasks for RMS versus EDF comparison
- `stress`: higher-interference task set that amplifies CRPD

## Metrics Reported

- Jobs released and completed
- Deadline misses
- Preemptions
- Cache hits and misses
- Cross-task evictions
- Observed CRPD cycles
- Analytic CRPD bound
- Average and worst-case response time

## Experiment Workflow

Run the full sweep:

```bash
python3 scripts/run_experiments.py
```

Generate plots:

```bash
python3 scripts/plot_results.py
```

Expected outputs:

- `results/experiment_results.csv`
- `results/crpd_cycles.png`
- `results/deadline_misses.png`
- `results/cross_task_evictions.png`

## Model Notes

- The simulator is discrete-time and deterministic.
- Each task job is represented as a sequence of memory accesses.
- Each access costs one cycle on a cache hit and `1 + miss_penalty` cycles on a miss.
- On preemption, the simulator snapshots the preempted task's resident cache lines.
- Misses to lines that were resident before preemption are counted as CRPD when the task resumes.
- `partitioned` assigns disjoint cache-set regions to tasks.
- `colored` assigns cache colors so tasks map into separate color classes.

## Validation

Run the built-in self-test:

```bash
./build/cache_aware_scheduler --self-test
```

The self-test checks that the shared-cache baseline produces more interference than the cache-isolated configurations on the `demo` scenario and that observed CRPD stays under the simulator's analytic bound for the colored case.

## Known Limitations

- The simulator models one outstanding job per task and drops overrunning jobs when a new release arrives.
- Cache coloring is modeled through deterministic set-color mapping rather than a full MMU/page allocator.
- The analytic CRPD bound is conservative and intended for comparison, not formal proof.

## References and Credits

- Project concept based on the CE8400 / ENGI9875 course brief.
- If AI assistance is used in the final submission, include a short disclosure statement in the report and README as required by the course policy.
