#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>
#include <stdint.h>

#define PROJECT_VERSION "1.0.0"
#define MAX_TASKS 8
#define MAX_WORKING_SET 32
#define MAX_CACHE_SETS 64
#define MAX_CACHE_WAYS 8

typedef enum {
    SCHEDULER_RMS = 0,
    SCHEDULER_EDF = 1
} SchedulerKind;

typedef enum {
    CACHE_POLICY_SHARED = 0,
    CACHE_POLICY_PARTITIONED = 1,
    CACHE_POLICY_COLORED = 2
} CachePolicyKind;

typedef struct {
    int set_count;
    int ways;
    int miss_penalty;
    int color_count;
} CacheConfig;

const char *scheduler_name(SchedulerKind scheduler);
const char *policy_name(CachePolicyKind policy);
bool parse_scheduler(const char *value, SchedulerKind *scheduler);
bool parse_policy(const char *value, CachePolicyKind *policy);

#endif
