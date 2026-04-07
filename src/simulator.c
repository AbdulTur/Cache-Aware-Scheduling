#include "simulator.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    bool active;
    bool started;
    bool was_preempted;
    int next_release_time;
    int absolute_deadline;
    int release_time;
    int job_number;
    int remaining_accesses;
    int remaining_access_cost;
    int resume_penalty_remaining;
    int next_access_index;
    bool pending_reload[MAX_WORKING_SET];
    int pending_reload_source_task[MAX_WORKING_SET];
    TaskSummary summary;
} TaskRuntime;

static int local_line_for_access(const TaskSpec *task, int access_index, int job_number) {
    if (task->working_set_size <= 0) {
        return 0;
    }

    return (job_number + access_index * task->line_stride) % task->working_set_size;
}

static bool task_ready(const TaskRuntime *runtime, int time_now) {
    return runtime->active && runtime->release_time <= time_now;
}

static bool candidate_is_higher_priority(
    const TaskSpec *candidate,
    const TaskRuntime *candidate_runtime,
    const TaskSpec *current,
    const TaskRuntime *current_runtime,
    SchedulerKind scheduler
) {
    if (scheduler == SCHEDULER_RMS) {
        if (candidate->period != current->period) {
            return candidate->period < current->period;
        }
    } else {
        if (candidate_runtime->absolute_deadline != current_runtime->absolute_deadline) {
            return candidate_runtime->absolute_deadline < current_runtime->absolute_deadline;
        }
    }

    return candidate->id < current->id;
}

static int choose_task(
    const Scenario *scenario,
    const TaskRuntime runtimes[MAX_TASKS],
    SchedulerKind scheduler,
    int time_now
) {
    int selected = -1;
    int i;

    for (i = 0; i < scenario->task_count; ++i) {
        if (!task_ready(&runtimes[i], time_now)) {
            continue;
        }

        if (selected < 0 ||
            candidate_is_higher_priority(
                &scenario->tasks[i],
                &runtimes[i],
                &scenario->tasks[selected],
                &runtimes[selected],
                scheduler)) {
            selected = i;
        }
    }

    return selected;
}

static void reset_pending_reload(TaskRuntime *runtime) {
    int line;

    for (line = 0; line < MAX_WORKING_SET; ++line) {
        runtime->pending_reload[line] = false;
        runtime->pending_reload_source_task[line] = -1;
    }
}

static int local_line_for_physical(
    const TaskSpec *task,
    int physical_line
) {
    int local_line = physical_line - task->base_line;

    if (local_line < 0 || local_line >= task->working_set_size) {
        return -1;
    }

    return local_line;
}

static void record_preempted_eviction(
    const Scenario *scenario,
    TaskRuntime runtimes[MAX_TASKS],
    int source_task_id,
    const CacheAccessResult *access
) {
    int victim_local_line;
    TaskRuntime *victim_runtime;
    const TaskSpec *victim_task;

    if (access == NULL || access->evicted_owner < 0 || access->evicted_physical_line < 0) {
        return;
    }

    victim_runtime = &runtimes[access->evicted_owner];
    victim_task = &scenario->tasks[access->evicted_owner];
    victim_local_line = local_line_for_physical(victim_task, access->evicted_physical_line);

    if (victim_local_line < 0 || !victim_runtime->was_preempted) {
        return;
    }
    if (!victim_runtime->pending_reload[victim_local_line]) {
        return;
    }
    if (victim_runtime->pending_reload_source_task[victim_local_line] >= 0) {
        return;
    }

    victim_runtime->pending_reload_source_task[victim_local_line] = source_task_id;
    victim_runtime->summary.evicted_while_preempted_by_task[source_task_id] += 1U;
}

static void start_new_job(const TaskSpec *task, TaskRuntime *runtime, int time_now) {
    runtime->active = true;
    runtime->started = false;
    runtime->was_preempted = false;
    runtime->release_time = time_now;
    runtime->absolute_deadline = time_now + task->relative_deadline;
    runtime->remaining_accesses = task->wcet_accesses;
    runtime->remaining_access_cost = 0;
    runtime->resume_penalty_remaining = 0;
    runtime->next_access_index = 0;
    reset_pending_reload(runtime);
    runtime->summary.jobs_released += 1U;
}

static void trace_event(
    FILE *trace,
    int time_now,
    const char *event,
    const char *task_name,
    const char *detail
) {
    if (trace == NULL) {
        return;
    }

    fprintf(trace, "%d,%s,%s,%s\n", time_now, event, task_name, detail);
}

static bool open_trace(const char *path, FILE **stream) {
    if (path == NULL) {
        *stream = NULL;
        return true;
    }

    *stream = fopen(path, "w");
    if (*stream == NULL) {
        return false;
    }

    fprintf(*stream, "time,event,task,detail\n");
    return true;
}

static void release_jobs(
    const Scenario *scenario,
    TaskRuntime runtimes[MAX_TASKS],
    int time_now,
    FILE *trace
) {
    int i;

    for (i = 0; i < scenario->task_count; ++i) {
        const TaskSpec *task = &scenario->tasks[i];
        TaskRuntime *runtime = &runtimes[i];

        if (time_now != runtime->next_release_time) {
            continue;
        }

        if (runtime->active) {
            runtime->summary.deadline_misses += 1U;
            runtime->active = false;
            trace_event(trace, time_now, "drop", task->name, "new_release_overran_previous_job");
        }

        runtime->job_number += 1;
        start_new_job(task, runtime, time_now);
        runtime->next_release_time += task->period;
        trace_event(trace, time_now, "release", task->name, "job_ready");
    }
}

static void capture_preemption_snapshot(
    const TaskSpec *task,
    TaskRuntime *runtime,
    const CacheState *cache
) {
    int local_line;

    if (!runtime->active) {
        return;
    }

    runtime->was_preempted = true;
    runtime->started = false;
    runtime->summary.preemptions += 1U;

    for (local_line = 0; local_line < task->working_set_size; ++local_line) {
        int physical_line = cache_physical_line(task, local_line);
        runtime->pending_reload[local_line] =
            cache_is_resident(cache, task->id, physical_line);
    }
}

static void apply_resume_penalty(
    const Scenario *scenario,
    const TaskSpec *task,
    TaskRuntime *runtime,
    CacheState *cache,
    TaskRuntime runtimes[MAX_TASKS]
) {
    int local_line;
    int lost_lines = 0;

    if (!runtime->was_preempted) {
        return;
    }

    for (local_line = 0; local_line < task->working_set_size; ++local_line) {
        int physical_line = cache_physical_line(task, local_line);

        if (runtime->pending_reload[local_line] &&
            !cache_is_resident(cache, task->id, physical_line)) {
            CacheAccessResult reload;
            int source_task_id;

            ++lost_lines;
            source_task_id = runtime->pending_reload_source_task[local_line];
            if (source_task_id >= 0) {
                runtime->summary.crpd_from_task[source_task_id] +=
                    (uint64_t) cache->config.miss_penalty;
            }
            reload = cache_reload_line(cache, task, physical_line);
            record_preempted_eviction(
                scenario,
                runtimes,
                task->id,
                &reload
            );
        }
        runtime->pending_reload[local_line] = false;
        runtime->pending_reload_source_task[local_line] = -1;
    }

    runtime->resume_penalty_remaining = lost_lines * cache->config.miss_penalty;
    runtime->summary.crpd_cycles += (uint64_t) runtime->resume_penalty_remaining;
    runtime->was_preempted = false;
}

static void start_access(
    const Scenario *scenario,
    const TaskSpec *task,
    TaskRuntime *runtime,
    CacheState *cache,
    TaskRuntime runtimes[MAX_TASKS]
) {
    int local_line = local_line_for_access(
        task,
        runtime->next_access_index,
        runtime->job_number
    );
    int physical_line = cache_physical_line(task, local_line);
    CacheAccessResult access = cache_access(cache, task, physical_line);

    record_preempted_eviction(
        scenario,
        runtimes,
        task->id,
        &access
    );

    if (access.hit) {
        runtime->remaining_access_cost = 1;
        runtime->summary.cache_hits += 1U;
    } else {
        runtime->remaining_access_cost = 1 + cache->config.miss_penalty;
        runtime->summary.cache_misses += 1U;
    }
}

static uint64_t popcount_mask(uint64_t value) {
    uint64_t count = 0U;

    while (value != 0U) {
        value &= (value - 1U);
        ++count;
    }

    return count;
}

static uint64_t estimate_crpd_bound(
    const Scenario *scenario,
    const CacheState *cache,
    SchedulerKind scheduler
) {
    uint64_t bound = 0U;
    int i;
    int j;

    for (i = 0; i < scenario->task_count; ++i) {
        const TaskSpec *task = &scenario->tasks[i];
        uint64_t jobs = (uint64_t) (scenario->duration / task->period + 1);

        for (j = 0; j < scenario->task_count; ++j) {
            const TaskSpec *higher = &scenario->tasks[j];
            uint64_t overlap_sets;
            uint64_t overlap_lines;
            uint64_t releases_per_job;
            bool higher_priority = false;

            if (i == j) {
                continue;
            }

            if (scheduler == SCHEDULER_RMS) {
                higher_priority = higher->period < task->period;
            } else {
                higher_priority = higher->relative_deadline < task->relative_deadline;
            }

            if (!higher_priority) {
                continue;
            }

            overlap_sets = popcount_mask(cache_overlap_mask(cache, i, j));
            overlap_lines = overlap_sets * (uint64_t) cache->config.ways;
            if ((uint64_t) task->working_set_size < overlap_lines) {
                overlap_lines = (uint64_t) task->working_set_size;
            }
            if ((uint64_t) higher->working_set_size < overlap_lines) {
                overlap_lines = (uint64_t) higher->working_set_size;
            }

            releases_per_job =
                (uint64_t) ((task->period + higher->period - 1) / higher->period);
            bound += jobs * releases_per_job * overlap_lines *
                (uint64_t) cache->config.miss_penalty;
        }
    }

    return bound;
}

bool run_simulation(
    const SimulationOptions *options,
    SimulationSummary *summary
) {
    CacheState cache;
    TaskRuntime runtimes[MAX_TASKS];
    FILE *trace = NULL;
    int running_task = -1;
    int time_now;
    int i;

    if (options == NULL || options->scenario == NULL || summary == NULL) {
        return false;
    }

    if (!cache_init(&cache, options->scenario, options->policy)) {
        return false;
    }

    memset(summary, 0, sizeof(*summary));
    memset(runtimes, 0, sizeof(runtimes));

    summary->scenario_name = options->scenario->name;
    summary->scheduler_name = scheduler_name(options->scheduler);
    summary->policy_name = policy_name(options->policy);
    summary->description = options->scenario->description;
    summary->duration = options->scenario->duration;
    summary->base_access_utilization =
        scenario_base_access_utilization(options->scenario);
    summary->isolated_cold_wcet_utilization =
        scenario_isolated_cold_wcet_utilization(options->scenario);
    summary->task_count = options->scenario->task_count;
    summary->analytic_crpd_bound = estimate_crpd_bound(
        options->scenario,
        &cache,
        options->scheduler
    );

    for (i = 0; i < options->scenario->task_count; ++i) {
        runtimes[i].next_release_time = options->scenario->tasks[i].phase;
        runtimes[i].summary.name = options->scenario->tasks[i].name;
    }

    if (!open_trace(options->trace_path, &trace)) {
        return false;
    }

    for (time_now = 0; time_now < options->scenario->duration; ++time_now) {
        int selected_task;

        release_jobs(options->scenario, runtimes, time_now, trace);

        selected_task = choose_task(
            options->scenario,
            runtimes,
            options->scheduler,
            time_now
        );

        if (running_task >= 0 &&
            selected_task >= 0 &&
            running_task != selected_task &&
            runtimes[running_task].active) {
            capture_preemption_snapshot(
                &options->scenario->tasks[running_task],
                &runtimes[running_task],
                &cache
            );
            trace_event(
                trace,
                time_now,
                "preempt",
                options->scenario->tasks[running_task].name,
                options->scenario->tasks[selected_task].name
            );
        }

        running_task = selected_task;

        if (running_task < 0) {
            summary->idle_cycles += 1U;
            continue;
        }

        {
            const TaskSpec *task = &options->scenario->tasks[running_task];
            TaskRuntime *runtime = &runtimes[running_task];

            if (!runtime->started) {
                if (runtime->was_preempted) {
                    apply_resume_penalty(
                        options->scenario,
                        task,
                        runtime,
                        &cache,
                        runtimes
                    );
                }
                runtime->started = true;
                trace_event(trace, time_now, "dispatch", task->name, "start_or_resume");
            }

            if (runtime->resume_penalty_remaining > 0) {
                runtime->resume_penalty_remaining -= 1;
                summary->busy_cycles += 1U;
                continue;
            }

            if (runtime->remaining_access_cost == 0) {
                start_access(
                    options->scenario,
                    task,
                    runtime,
                    &cache,
                    runtimes
                );
            }

            runtime->remaining_access_cost -= 1;
            summary->busy_cycles += 1U;

            if (runtime->remaining_access_cost == 0) {
                runtime->remaining_accesses -= 1;
                runtime->next_access_index += 1;

                if (runtime->remaining_accesses == 0) {
                    int completion_time = time_now + 1;
                    int response_time = completion_time - runtime->release_time;

                    runtime->summary.jobs_completed += 1U;
                    runtime->summary.total_response_time += (uint64_t) response_time;
                    if (response_time > runtime->summary.max_response_time) {
                        runtime->summary.max_response_time = response_time;
                    }
                    if (completion_time > runtime->absolute_deadline) {
                        runtime->summary.deadline_misses += 1U;
                    }

                    runtime->active = false;
                    runtime->started = false;
                    runtime->was_preempted = false;
                    runtime->resume_penalty_remaining = 0;
                    trace_event(trace, completion_time, "complete", task->name, "job_complete");
                    running_task = -1;
                }
            }
        }
    }

    if (trace != NULL) {
        fclose(trace);
    }

    for (i = 0; i < options->scenario->task_count; ++i) {
        TaskSummary *task_summary = &summary->task_summaries[i];
        *task_summary = runtimes[i].summary;
        summary->jobs_released += task_summary->jobs_released;
        summary->jobs_completed += task_summary->jobs_completed;
        summary->deadline_misses += task_summary->deadline_misses;
        summary->preemptions += task_summary->preemptions;
        summary->cache_hits += task_summary->cache_hits;
        summary->cache_misses += task_summary->cache_misses;
        summary->crpd_cycles += task_summary->crpd_cycles;
        summary->average_response_time +=
            (double) task_summary->total_response_time;
        if (task_summary->max_response_time > summary->max_response_time) {
            summary->max_response_time = task_summary->max_response_time;
        }
    }

    summary->cross_task_evictions = cache.cross_task_evictions;

    if (summary->jobs_completed > 0U) {
        summary->average_response_time /=
            (double) summary->jobs_completed;
    }

    return true;
}

void print_summary(const SimulationSummary *summary, FILE *stream) {
    int i;

    fprintf(stream, "Scenario: %s\n", summary->scenario_name);
    fprintf(stream, "Scheduler: %s\n", summary->scheduler_name);
    fprintf(stream, "Policy: %s\n", summary->policy_name);
    fprintf(stream, "Description: %s\n", summary->description);
    fprintf(stream, "Duration: %d cycles\n", summary->duration);
    fprintf(stream, "Base access utilization: %.3f\n",
        summary->base_access_utilization);
    fprintf(stream, "Isolated cold WCET utilization: %.3f\n",
        summary->isolated_cold_wcet_utilization);
    fprintf(stream, "Jobs released/completed: %llu/%llu\n",
        (unsigned long long) summary->jobs_released,
        (unsigned long long) summary->jobs_completed);
    fprintf(stream, "Deadline misses: %llu\n",
        (unsigned long long) summary->deadline_misses);
    fprintf(stream, "Preemptions: %llu\n",
        (unsigned long long) summary->preemptions);
    fprintf(stream, "Cache hits/misses: %llu/%llu\n",
        (unsigned long long) summary->cache_hits,
        (unsigned long long) summary->cache_misses);
    fprintf(stream, "Cross-task evictions: %llu\n",
        (unsigned long long) summary->cross_task_evictions);
    fprintf(stream, "Observed CRPD cycles: %llu\n",
        (unsigned long long) summary->crpd_cycles);
    fprintf(stream, "Analytic CRPD bound: %llu\n",
        (unsigned long long) summary->analytic_crpd_bound);
    fprintf(stream, "Average response time: %.2f cycles\n",
        summary->average_response_time);
    fprintf(stream, "Max response time: %d cycles\n",
        summary->max_response_time);
    fprintf(stream, "\nPer-task summary:\n");

    for (i = 0; i < summary->task_count; ++i) {
        const TaskSummary *task = &summary->task_summaries[i];
        fprintf(stream,
            "  %s: jobs=%llu completed=%llu misses=%llu preemptions=%llu cache_misses=%llu crpd=%llu\n",
            task->name,
            (unsigned long long) task->jobs_released,
            (unsigned long long) task->jobs_completed,
            (unsigned long long) task->deadline_misses,
            (unsigned long long) task->preemptions,
            (unsigned long long) task->cache_misses,
            (unsigned long long) task->crpd_cycles);
    }
}

bool write_summary_csv(const SimulationSummary *summary, const char *path) {
    FILE *stream;

    if (path == NULL) {
        return true;
    }

    if (strcmp(path, "-") == 0) {
        stream = stdout;
    } else {
        stream = fopen(path, "w");
        if (stream == NULL) {
            return false;
        }
    }

    fprintf(stream,
        "scenario,scheduler,policy,duration,base_access_utilization,isolated_cold_wcet_utilization,jobs_released,jobs_completed,deadline_misses,preemptions,cache_hits,cache_misses,cross_task_evictions,crpd_cycles,analytic_crpd_bound,busy_cycles,idle_cycles,average_response_time,max_response_time\n");
    fprintf(stream,
        "%s,%s,%s,%d,%.6f,%.6f,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%.6f,%d\n",
        summary->scenario_name,
        summary->scheduler_name,
        summary->policy_name,
        summary->duration,
        summary->base_access_utilization,
        summary->isolated_cold_wcet_utilization,
        (unsigned long long) summary->jobs_released,
        (unsigned long long) summary->jobs_completed,
        (unsigned long long) summary->deadline_misses,
        (unsigned long long) summary->preemptions,
        (unsigned long long) summary->cache_hits,
        (unsigned long long) summary->cache_misses,
        (unsigned long long) summary->cross_task_evictions,
        (unsigned long long) summary->crpd_cycles,
        (unsigned long long) summary->analytic_crpd_bound,
        (unsigned long long) summary->busy_cycles,
        (unsigned long long) summary->idle_cycles,
        summary->average_response_time,
        summary->max_response_time);

    if (stream != stdout) {
        fclose(stream);
    }

    return true;
}

bool write_pairwise_csv(const SimulationSummary *summary, const char *path) {
    FILE *stream;
    int victim;
    int source;

    if (path == NULL) {
        return true;
    }

    if (strcmp(path, "-") == 0) {
        stream = stdout;
    } else {
        stream = fopen(path, "w");
        if (stream == NULL) {
            return false;
        }
    }

    fprintf(stream,
        "scenario,scheduler,policy,victim_task,source_task,evictions_while_preempted,attributed_crpd_cycles\n");

    for (victim = 0; victim < summary->task_count; ++victim) {
        const TaskSummary *task = &summary->task_summaries[victim];

        for (source = 0; source < summary->task_count; ++source) {
            if (victim == source) {
                continue;
            }
            if (task->evicted_while_preempted_by_task[source] == 0U &&
                task->crpd_from_task[source] == 0U) {
                continue;
            }

            fprintf(stream,
                "%s,%s,%s,%s,%s,%llu,%llu\n",
                summary->scenario_name,
                summary->scheduler_name,
                summary->policy_name,
                task->name,
                summary->task_summaries[source].name,
                (unsigned long long) task->evicted_while_preempted_by_task[source],
                (unsigned long long) task->crpd_from_task[source]);
        }
    }

    if (stream != stdout) {
        fclose(stream);
    }

    return true;
}

static bool run_test_case(
    const char *scenario_name,
    SchedulerKind scheduler,
    CachePolicyKind policy,
    SimulationSummary *summary
) {
    SimulationOptions options;
    const Scenario *scenario = find_scenario(scenario_name);

    if (scenario == NULL) {
        return false;
    }

    options.scenario = scenario;
    options.scheduler = scheduler;
    options.policy = policy;
    options.trace_path = NULL;

    return run_simulation(&options, summary);
}

bool run_self_test(void) {
    SimulationSummary shared;
    SimulationSummary partitioned;
    SimulationSummary colored;
    SimulationSummary compare_rms;
    SimulationSummary compare_edf;
    SimulationSummary compare_rms_partitioned;
    SimulationSummary compare_edf_partitioned;

    if (!run_test_case("demo", SCHEDULER_RMS, CACHE_POLICY_SHARED, &shared)) {
        return false;
    }
    if (!run_test_case("demo", SCHEDULER_RMS, CACHE_POLICY_PARTITIONED, &partitioned)) {
        return false;
    }
    if (!run_test_case("demo", SCHEDULER_RMS, CACHE_POLICY_COLORED, &colored)) {
        return false;
    }
    if (!run_test_case("sched_compare", SCHEDULER_RMS, CACHE_POLICY_SHARED, &compare_rms)) {
        return false;
    }
    if (!run_test_case("sched_compare", SCHEDULER_EDF, CACHE_POLICY_SHARED, &compare_edf)) {
        return false;
    }
    if (!run_test_case("sched_compare", SCHEDULER_RMS, CACHE_POLICY_PARTITIONED,
        &compare_rms_partitioned)) {
        return false;
    }
    if (!run_test_case("sched_compare", SCHEDULER_EDF, CACHE_POLICY_PARTITIONED,
        &compare_edf_partitioned)) {
        return false;
    }

    if (shared.cross_task_evictions <= colored.cross_task_evictions ||
        colored.cross_task_evictions <= partitioned.cross_task_evictions) {
        return false;
    }
    if (shared.crpd_cycles <= colored.crpd_cycles ||
        colored.crpd_cycles < partitioned.crpd_cycles) {
        return false;
    }
    if (partitioned.crpd_cycles != 0U) {
        return false;
    }
    if (colored.cross_task_evictions == partitioned.cross_task_evictions &&
        colored.cache_misses == partitioned.cache_misses) {
        return false;
    }
    if (colored.analytic_crpd_bound < colored.crpd_cycles) {
        return false;
    }
    if (compare_rms_partitioned.deadline_misses != 0U ||
        compare_edf_partitioned.deadline_misses != 0U) {
        return false;
    }
    if (compare_rms.cross_task_evictions <= compare_rms_partitioned.cross_task_evictions ||
        compare_edf.cross_task_evictions <= compare_edf_partitioned.cross_task_evictions) {
        return false;
    }
    if (compare_rms.preemptions == compare_edf.preemptions &&
        compare_rms.deadline_misses == compare_edf.deadline_misses &&
        compare_rms.crpd_cycles == compare_edf.crpd_cycles &&
        compare_rms.max_response_time == compare_edf.max_response_time &&
        compare_rms_partitioned.preemptions == compare_edf_partitioned.preemptions &&
        compare_rms_partitioned.max_response_time ==
            compare_edf_partitioned.max_response_time) {
        return false;
    }

    return true;
}
