#include "cache.h"

#include <limits.h>
#include <string.h>

static uint64_t all_sets_mask(int set_count) {
    if (set_count >= 64) {
        return UINT64_MAX;
    }

    return (1ULL << set_count) - 1ULL;
}

static int popcount64(uint64_t value) {
    int count = 0;

    while (value != 0ULL) {
        value &= (value - 1ULL);
        ++count;
    }

    return count;
}

static int nth_set_from_mask(uint64_t mask, int ordinal) {
    int set_index;

    for (set_index = 0; set_index < 64; ++set_index) {
        if ((mask & (1ULL << set_index)) == 0ULL) {
            continue;
        }
        if (ordinal == 0) {
            return set_index;
        }
        --ordinal;
    }

    return 0;
}

static int max_partition_group(const Scenario *scenario) {
    int i;
    int max_group = 0;

    for (i = 0; i < scenario->task_count; ++i) {
        if (scenario->tasks[i].partition_group > max_group) {
            max_group = scenario->tasks[i].partition_group;
        }
    }

    return max_group;
}

static void configure_partition_masks(CacheState *cache, const Scenario *scenario) {
    int groups = max_partition_group(scenario) + 1;
    int start = 0;
    uint64_t group_masks[MAX_TASKS] = {0};
    int group;

    for (group = 0; group < groups; ++group) {
        int remaining_sets = cache->config.set_count - start;
        int remaining_groups = groups - group;
        int width = remaining_sets / remaining_groups;
        uint64_t mask = 0ULL;
        int set_index;

        if (width <= 0) {
            width = 1;
        }

        for (set_index = 0; set_index < width && (start + set_index) < cache->config.set_count; ++set_index) {
            mask |= (1ULL << (start + set_index));
        }

        group_masks[group] = mask;
        start += width;
    }

    for (group = 0; group < scenario->task_count; ++group) {
        int partition_group = scenario->tasks[group].partition_group;
        cache->task_set_masks[group] = group_masks[partition_group];
    }
}

static void configure_color_masks(CacheState *cache, const Scenario *scenario) {
    int colors = scenario->cache.color_count;
    int task_index;

    if (colors <= 0) {
        colors = scenario->task_count;
    }
    if (colors > cache->config.set_count) {
        colors = cache->config.set_count;
    }

    for (task_index = 0; task_index < scenario->task_count; ++task_index) {
        int color = scenario->tasks[task_index].color_group % colors;
        uint64_t mask = 0ULL;
        int set_index;

        for (set_index = 0; set_index < cache->config.set_count; ++set_index) {
            if ((set_index % colors) == color) {
                mask |= (1ULL << set_index);
            }
        }

        cache->task_set_masks[task_index] = mask;
    }
}

static int choose_set(const CacheState *cache, const TaskSpec *task, int physical_line) {
    uint64_t mask = cache->task_set_masks[task->id];
    int set_count = popcount64(mask);
    int ordinal;

    if (set_count <= 0) {
        return physical_line % cache->config.set_count;
    }

    ordinal = physical_line % set_count;
    return nth_set_from_mask(mask, ordinal);
}

static CacheAccessResult install_line(
    CacheState *cache,
    const TaskSpec *task,
    int physical_line
) {
    int set_index = choose_set(cache, task, physical_line);
    int way;
    int victim = -1;
    uint64_t oldest_tick = ULLONG_MAX;
    CacheAccessResult result = {
        .hit = false,
        .evicted_owner = -1,
        .evicted_physical_line = -1
    };

    for (way = 0; way < cache->config.ways; ++way) {
        CacheEntry *entry = &cache->entries[set_index][way];

        if (!entry->valid) {
            victim = way;
            break;
        }
        if (entry->last_touch < oldest_tick) {
            oldest_tick = entry->last_touch;
            victim = way;
        }
    }

    if (victim >= 0) {
        CacheEntry *entry = &cache->entries[set_index][victim];

        if (entry->valid && entry->owner_task_id != task->id) {
            ++cache->cross_task_evictions;
            result.evicted_owner = entry->owner_task_id;
            result.evicted_physical_line = entry->physical_line;
        }
        entry->valid = true;
        entry->owner_task_id = task->id;
        entry->physical_line = physical_line;
        entry->last_touch = cache->tick;
    }

    return result;
}

bool cache_init(CacheState *cache, const Scenario *scenario, CachePolicyKind policy) {
    int task_index;

    if (scenario->cache.set_count <= 0 || scenario->cache.set_count > MAX_CACHE_SETS) {
        return false;
    }
    if (scenario->cache.ways <= 0 || scenario->cache.ways > MAX_CACHE_WAYS) {
        return false;
    }

    memset(cache, 0, sizeof(*cache));
    cache->config = scenario->cache;
    cache->policy = policy;
    cache->task_count = scenario->task_count;

    for (task_index = 0; task_index < scenario->task_count; ++task_index) {
        cache->task_set_masks[task_index] = all_sets_mask(cache->config.set_count);
    }

    if (policy == CACHE_POLICY_PARTITIONED) {
        configure_partition_masks(cache, scenario);
    } else if (policy == CACHE_POLICY_COLORED) {
        configure_color_masks(cache, scenario);
    }

    return true;
}

int cache_physical_line(const TaskSpec *task, int local_line) {
    return task->base_line + local_line;
}

bool cache_is_resident(const CacheState *cache, int task_id, int physical_line) {
    int set_index;
    int way;

    for (set_index = 0; set_index < cache->config.set_count; ++set_index) {
        for (way = 0; way < cache->config.ways; ++way) {
            const CacheEntry *entry = &cache->entries[set_index][way];
            if (entry->valid &&
                entry->owner_task_id == task_id &&
                entry->physical_line == physical_line) {
                return true;
            }
        }
    }

    return false;
}

CacheAccessResult cache_access(CacheState *cache, const TaskSpec *task, int physical_line) {
    int set_index = choose_set(cache, task, physical_line);
    int way;
    CacheAccessResult result = {
        .hit = false,
        .evicted_owner = -1,
        .evicted_physical_line = -1
    };

    ++cache->tick;

    for (way = 0; way < cache->config.ways; ++way) {
        CacheEntry *entry = &cache->entries[set_index][way];
        if (entry->valid &&
            entry->owner_task_id == task->id &&
            entry->physical_line == physical_line) {
            entry->last_touch = cache->tick;
            result.hit = true;
            return result;
        }
    }

    result = install_line(cache, task, physical_line);

    return result;
}

CacheAccessResult cache_reload_line(
    CacheState *cache,
    const TaskSpec *task,
    int physical_line
) {
    CacheAccessResult result = {
        .hit = false,
        .evicted_owner = -1,
        .evicted_physical_line = -1
    };

    if (cache_is_resident(cache, task->id, physical_line)) {
        result.hit = true;
        return result;
    }

    ++cache->tick;
    return install_line(cache, task, physical_line);
}

uint64_t cache_overlap_mask(const CacheState *cache, int task_a, int task_b) {
    return cache->task_set_masks[task_a] & cache->task_set_masks[task_b];
}
