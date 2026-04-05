#ifndef SCENARIO_H
#define SCENARIO_H

#include "config.h"

typedef struct {
    int id;
    const char *name;
    int period;
    int relative_deadline;
    int wcet_accesses;
    int phase;
    int working_set_size;
    int line_stride;
    int base_line;
    int partition_group;
    int color_group;
} TaskSpec;

typedef struct {
    const char *name;
    const char *description;
    int duration;
    CacheConfig cache;
    int task_count;
    TaskSpec tasks[MAX_TASKS];
} Scenario;

const Scenario *find_scenario(const char *name);
void print_scenarios(void);
double scenario_nominal_utilization(const Scenario *scenario);

#endif
