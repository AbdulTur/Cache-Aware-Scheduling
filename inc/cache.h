#ifndef CACHE_H
#define CACHE_H

#include <stdbool.h>
#include <stdint.h>

#include "scenario.h"

typedef struct {
    bool hit;
    int evicted_owner;
    int evicted_physical_line;
} CacheAccessResult;

typedef struct {
    bool valid;
    int owner_task_id;
    int physical_line;
    uint64_t last_touch;
} CacheEntry;

typedef struct {
    CacheConfig config;
    CachePolicyKind policy;
    int task_count;
    uint64_t tick;
    uint64_t cross_task_evictions;
    uint64_t task_set_masks[MAX_TASKS];
    CacheEntry entries[MAX_CACHE_SETS][MAX_CACHE_WAYS];
} CacheState;

bool cache_init(
    CacheState *cache,
    const Scenario *scenario,
    CachePolicyKind policy
);

int cache_physical_line(const TaskSpec *task, int local_line);
bool cache_is_resident(
    const CacheState *cache,
    int task_id,
    int physical_line
);
CacheAccessResult cache_access(
    CacheState *cache,
    const TaskSpec *task,
    int physical_line
);
CacheAccessResult cache_reload_line(
    CacheState *cache,
    const TaskSpec *task,
    int physical_line
);
uint64_t cache_overlap_mask(
    const CacheState *cache,
    int task_a,
    int task_b
);

#endif
