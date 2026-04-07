#include "scenario.h"

#include <stdio.h>
#include <string.h>

static const Scenario SCENARIOS[] = {
    {
        .name = "demo",
        .description = "Three periodic tasks with partial color overlap and isolated partitions.",
        .duration = 240,
        .cache = { .set_count = 12, .ways = 2, .miss_penalty = 4, .color_count = 2 },
        .task_count = 3,
        .tasks = {
            { .id = 0, .name = "T_ctrl",   .period = 12, .relative_deadline = 12, .wcet_accesses = 4, .phase = 0, .working_set_size = 8, .line_stride = 1, .base_line = 0,  .partition_group = 0, .color_group = 0 },
            { .id = 1, .name = "T_filter", .period = 18, .relative_deadline = 18, .wcet_accesses = 5, .phase = 0, .working_set_size = 4, .line_stride = 2, .base_line = 12, .partition_group = 1, .color_group = 1 },
            { .id = 2, .name = "T_fusion", .period = 36, .relative_deadline = 36, .wcet_accesses = 7, .phase = 0, .working_set_size = 8, .line_stride = 3, .base_line = 24, .partition_group = 2, .color_group = 0 }
        }
    },
    {
        .name = "harmonic",
        .description = "Four harmonic tasks used for repeatable scheduler and cache-policy comparisons.",
        .duration = 320,
        .cache = { .set_count = 16, .ways = 2, .miss_penalty = 3, .color_count = 2 },
        .task_count = 4,
        .tasks = {
            { .id = 0, .name = "T_sense",  .period = 10, .relative_deadline = 10, .wcet_accesses = 3, .phase = 0, .working_set_size = 6, .line_stride = 1, .base_line = 0,  .partition_group = 0, .color_group = 0 },
            { .id = 1, .name = "T_state",  .period = 20, .relative_deadline = 20, .wcet_accesses = 5, .phase = 0, .working_set_size = 6, .line_stride = 3, .base_line = 16, .partition_group = 1, .color_group = 1 },
            { .id = 2, .name = "T_plan",   .period = 40, .relative_deadline = 40, .wcet_accesses = 7, .phase = 0, .working_set_size = 6, .line_stride = 1, .base_line = 32, .partition_group = 2, .color_group = 0 },
            { .id = 3, .name = "T_logger", .period = 80, .relative_deadline = 80, .wcet_accesses = 10, .phase = 0, .working_set_size = 6, .line_stride = 3, .base_line = 48, .partition_group = 3, .color_group = 0 }
        }
    },
    {
        .name = "stress",
        .description = "High-interference workload that amplifies cache-related preemption delay.",
        .duration = 352,
        .cache = { .set_count = 16, .ways = 2, .miss_penalty = 5, .color_count = 2 },
        .task_count = 4,
        .tasks = {
            { .id = 0, .name = "T_irq",    .period = 14, .relative_deadline = 14, .wcet_accesses = 4, .phase = 0, .working_set_size = 6, .line_stride = 1, .base_line = 0,  .partition_group = 0, .color_group = 0 },
            { .id = 1, .name = "T_ctrl",   .period = 22, .relative_deadline = 22, .wcet_accesses = 6, .phase = 0, .working_set_size = 6, .line_stride = 3, .base_line = 16, .partition_group = 1, .color_group = 1 },
            { .id = 2, .name = "T_map",    .period = 44, .relative_deadline = 44, .wcet_accesses = 8, .phase = 0, .working_set_size = 6, .line_stride = 1, .base_line = 32, .partition_group = 2, .color_group = 0 },
            { .id = 3, .name = "T_stream", .period = 88, .relative_deadline = 88, .wcet_accesses = 11, .phase = 0, .working_set_size = 6, .line_stride = 3, .base_line = 48, .partition_group = 3, .color_group = 0 }
        }
    },
    {
        .name = "sched_compare",
        .description = "Non-harmonic scheduler-comparison workload with isolated schedulability margin.",
        .duration = 420,
        .cache = { .set_count = 18, .ways = 2, .miss_penalty = 2, .color_count = 2 },
        .task_count = 4,
        .tasks = {
            { .id = 0, .name = "T_irq",    .period = 15, .relative_deadline = 15, .wcet_accesses = 1, .phase = 0, .working_set_size = 4, .line_stride = 1, .base_line = 0,  .partition_group = 0, .color_group = 0 },
            { .id = 1, .name = "T_ctrl",   .period = 24, .relative_deadline = 20, .wcet_accesses = 2, .phase = 0, .working_set_size = 5, .line_stride = 2, .base_line = 18, .partition_group = 1, .color_group = 1 },
            { .id = 2, .name = "T_vision", .period = 37, .relative_deadline = 19, .wcet_accesses = 3, .phase = 0, .working_set_size = 6, .line_stride = 3, .base_line = 36, .partition_group = 2, .color_group = 0 },
            { .id = 3, .name = "T_map",    .period = 58, .relative_deadline = 58, .wcet_accesses = 4, .phase = 0, .working_set_size = 7, .line_stride = 4, .base_line = 54, .partition_group = 3, .color_group = 0 }
        }
    },
    {
        .name = "crpd_peak",
        .description = "Overloaded non-harmonic task set that exposes worst-case CRPD and scheduler stress.",
        .duration = 420,
        .cache = { .set_count = 18, .ways = 2, .miss_penalty = 6, .color_count = 2 },
        .task_count = 4,
        .tasks = {
            { .id = 0, .name = "T_irq",    .period = 12, .relative_deadline = 12, .wcet_accesses = 4, .phase = 0, .working_set_size = 6, .line_stride = 1, .base_line = 0,  .partition_group = 0, .color_group = 0 },
            { .id = 1, .name = "T_ctrl",   .period = 20, .relative_deadline = 20, .wcet_accesses = 5, .phase = 0, .working_set_size = 7, .line_stride = 2, .base_line = 18, .partition_group = 1, .color_group = 1 },
            { .id = 2, .name = "T_vision", .period = 30, .relative_deadline = 14, .wcet_accesses = 8, .phase = 0, .working_set_size = 8, .line_stride = 3, .base_line = 36, .partition_group = 2, .color_group = 0 },
            { .id = 3, .name = "T_map",    .period = 60, .relative_deadline = 36, .wcet_accesses = 10, .phase = 0, .working_set_size = 8, .line_stride = 4, .base_line = 54, .partition_group = 3, .color_group = 0 }
        }
    }
};

static const size_t SCENARIO_COUNT = sizeof(SCENARIOS) / sizeof(SCENARIOS[0]);

const char *scheduler_name(SchedulerKind scheduler) {
    switch (scheduler) {
        case SCHEDULER_RMS:
            return "rms";
        case SCHEDULER_EDF:
            return "edf";
        default:
            return "unknown";
    }
}

const char *policy_name(CachePolicyKind policy) {
    switch (policy) {
        case CACHE_POLICY_SHARED:
            return "shared";
        case CACHE_POLICY_PARTITIONED:
            return "partitioned";
        case CACHE_POLICY_COLORED:
            return "colored";
        default:
            return "unknown";
    }
}

bool parse_scheduler(const char *value, SchedulerKind *scheduler) {
    if (strcmp(value, "rms") == 0) {
        *scheduler = SCHEDULER_RMS;
        return true;
    }
    if (strcmp(value, "edf") == 0) {
        *scheduler = SCHEDULER_EDF;
        return true;
    }
    return false;
}

bool parse_policy(const char *value, CachePolicyKind *policy) {
    if (strcmp(value, "shared") == 0) {
        *policy = CACHE_POLICY_SHARED;
        return true;
    }
    if (strcmp(value, "partitioned") == 0) {
        *policy = CACHE_POLICY_PARTITIONED;
        return true;
    }
    if (strcmp(value, "colored") == 0) {
        *policy = CACHE_POLICY_COLORED;
        return true;
    }
    return false;
}

const Scenario *find_scenario(const char *name) {
    size_t i;

    for (i = 0; i < SCENARIO_COUNT; ++i) {
        if (strcmp(name, SCENARIOS[i].name) == 0) {
            return &SCENARIOS[i];
        }
    }

    return NULL;
}

void print_scenarios(void) {
    size_t i;

    for (i = 0; i < SCENARIO_COUNT; ++i) {
        const Scenario *scenario = &SCENARIOS[i];
        printf("%s: %s\n", scenario->name, scenario->description);
    }
}

static int local_line_for_job(
    const TaskSpec *task,
    int access_index
) {
    if (task->working_set_size <= 0) {
        return 0;
    }

    return (access_index * task->line_stride) % task->working_set_size;
}

static int isolated_cold_wcet_cycles(
    const Scenario *scenario,
    const TaskSpec *task
) {
    bool touched[MAX_WORKING_SET] = {false};
    int unique_lines = 0;
    int access_index;

    for (access_index = 0; access_index < task->wcet_accesses; ++access_index) {
        int local_line = local_line_for_job(task, access_index);

        if (!touched[local_line]) {
            touched[local_line] = true;
            unique_lines += 1;
        }
    }

    return task->wcet_accesses + unique_lines * scenario->cache.miss_penalty;
}

double scenario_base_access_utilization(const Scenario *scenario) {
    double utilization = 0.0;
    int i;

    for (i = 0; i < scenario->task_count; ++i) {
        const TaskSpec *task = &scenario->tasks[i];
        utilization += (double) task->wcet_accesses / (double) task->period;
    }

    return utilization;
}

double scenario_isolated_cold_wcet_utilization(const Scenario *scenario) {
    double utilization = 0.0;
    int i;

    for (i = 0; i < scenario->task_count; ++i) {
        const TaskSpec *task = &scenario->tasks[i];
        utilization +=
            (double) isolated_cold_wcet_cycles(scenario, task) /
            (double) task->period;
    }

    return utilization;
}
