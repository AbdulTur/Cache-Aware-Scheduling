#include "simulator.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static void print_usage(const char *program_name) {
    printf("Usage: %s [options]\n", program_name);
    printf("  --scenario <name>      Scenario to run (default: demo)\n");
    printf("  --scheduler <rms|edf>  Scheduler to simulate (default: rms)\n");
    printf("  --policy <shared|partitioned|colored>  Cache policy (default: shared)\n");
    printf("  --trace-csv <path>     Write detailed trace CSV\n");
    printf("  --summary-csv <path>   Write one-run summary CSV, use - for stdout\n");
    printf("  --pairwise-csv <path>  Write pairwise CRPD attribution CSV\n");
    printf("  --miss-penalty <n>     Override cache miss penalty for the selected scenario\n");
    printf("  --color-count <n>      Override cache color count for the selected scenario\n");
    printf("  --list-scenarios       Print built-in scenarios\n");
    printf("  --self-test            Run simulator validation checks\n");
    printf("  --help                 Show this help message\n");
}

int main(int argc, char **argv) {
    const Scenario *scenario = find_scenario("demo");
    SchedulerKind scheduler = SCHEDULER_RMS;
    CachePolicyKind policy = CACHE_POLICY_SHARED;
    const char *trace_path = NULL;
    const char *summary_csv = NULL;
    const char *pairwise_csv = NULL;
    int miss_penalty_override = -1;
    int color_count_override = -1;
    int i;

    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
        if (strcmp(argv[i], "--list-scenarios") == 0) {
            print_scenarios();
            return 0;
        }
        if (strcmp(argv[i], "--self-test") == 0) {
            if (run_self_test()) {
                printf("Self-test passed.\n");
                return 0;
            }
            fprintf(stderr, "Self-test failed.\n");
            return 1;
        }
        if ((strcmp(argv[i], "--scenario") == 0) && (i + 1 < argc)) {
            scenario = find_scenario(argv[++i]);
            if (scenario == NULL) {
                fprintf(stderr, "Unknown scenario.\n");
                return 1;
            }
            continue;
        }
        if ((strcmp(argv[i], "--scheduler") == 0) && (i + 1 < argc)) {
            if (!parse_scheduler(argv[++i], &scheduler)) {
                fprintf(stderr, "Unknown scheduler.\n");
                return 1;
            }
            continue;
        }
        if ((strcmp(argv[i], "--policy") == 0) && (i + 1 < argc)) {
            if (!parse_policy(argv[++i], &policy)) {
                fprintf(stderr, "Unknown policy.\n");
                return 1;
            }
            continue;
        }
        if ((strcmp(argv[i], "--trace-csv") == 0) && (i + 1 < argc)) {
            trace_path = argv[++i];
            continue;
        }
        if ((strcmp(argv[i], "--summary-csv") == 0) && (i + 1 < argc)) {
            summary_csv = argv[++i];
            continue;
        }
        if ((strcmp(argv[i], "--pairwise-csv") == 0) && (i + 1 < argc)) {
            pairwise_csv = argv[++i];
            continue;
        }
        if ((strcmp(argv[i], "--miss-penalty") == 0) && (i + 1 < argc)) {
            miss_penalty_override = atoi(argv[++i]);
            continue;
        }
        if ((strcmp(argv[i], "--color-count") == 0) && (i + 1 < argc)) {
            color_count_override = atoi(argv[++i]);
            continue;
        }

        fprintf(stderr, "Unknown option: %s\n", argv[i]);
        print_usage(argv[0]);
        return 1;
    }

    {
        SimulationOptions options;
        SimulationSummary summary;
        Scenario scenario_override;

        if (miss_penalty_override > 0 || color_count_override > 0) {
            scenario_override = *scenario;
            if (miss_penalty_override > 0) {
                scenario_override.cache.miss_penalty = miss_penalty_override;
            }
            if (color_count_override > 0) {
                scenario_override.cache.color_count = color_count_override;
            }
            options.scenario = &scenario_override;
        } else {
            options.scenario = scenario;
        }
        options.scheduler = scheduler;
        options.policy = policy;
        options.trace_path = trace_path;

        if (!run_simulation(&options, &summary)) {
            fprintf(stderr, "Simulation failed.\n");
            return 1;
        }

        print_summary(&summary, stdout);
        if (!write_summary_csv(&summary, summary_csv)) {
            fprintf(stderr, "Failed to write summary CSV.\n");
            return 1;
        }
        if (!write_pairwise_csv(&summary, pairwise_csv)) {
            fprintf(stderr, "Failed to write pairwise CSV.\n");
            return 1;
        }
    }

    return 0;
}
