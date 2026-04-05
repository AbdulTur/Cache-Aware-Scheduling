#ifndef SIMULATOR_H
#define SIMULATOR_H

#include <stdbool.h>
#include <stdio.h>

#include "cache.h"

typedef struct {
    const char *name;
    uint64_t jobs_released;
    uint64_t jobs_completed;
    uint64_t deadline_misses;
    uint64_t preemptions;
    uint64_t cache_hits;
    uint64_t cache_misses;
    uint64_t crpd_cycles;
    uint64_t total_response_time;
    int max_response_time;
} TaskSummary;

typedef struct {
    const char *scenario_name;
    const char *scheduler_name;
    const char *policy_name;
    const char *description;
    int duration;
    double nominal_utilization;
    uint64_t jobs_released;
    uint64_t jobs_completed;
    uint64_t deadline_misses;
    uint64_t preemptions;
    uint64_t cache_hits;
    uint64_t cache_misses;
    uint64_t cross_task_evictions;
    uint64_t crpd_cycles;
    uint64_t analytic_crpd_bound;
    uint64_t busy_cycles;
    uint64_t idle_cycles;
    double average_response_time;
    int max_response_time;
    int task_count;
    TaskSummary task_summaries[MAX_TASKS];
} SimulationSummary;

typedef struct {
    const Scenario *scenario;
    SchedulerKind scheduler;
    CachePolicyKind policy;
    const char *trace_path;
} SimulationOptions;

bool run_simulation(
    const SimulationOptions *options,
    SimulationSummary *summary
);

void print_summary(const SimulationSummary *summary, FILE *stream);
bool write_summary_csv(
    const SimulationSummary *summary,
    const char *path
);
bool run_self_test(void);

#endif
