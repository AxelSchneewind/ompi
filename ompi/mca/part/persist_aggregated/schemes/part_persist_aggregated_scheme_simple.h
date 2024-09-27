#pragma once // TODO switch to traditional header guards

#include <stdlib.h>
#include <stddef.h>

#include "opal/include/opal_stdatomic.h"
#include "opal/class/opal_ring_buffer.h"

// simple and (possibly) unsafe ring buffer
struct ring_buffer{
    size_t capacity;
    opal_atomic_uint32_t begin;           // read index
    opal_atomic_uint32_t end;             // past-last index of available data
    opal_atomic_uint32_t end_internal;    // internal write index
    char* buffer;
};

struct part_persist_aggregation_state {
    // counters for each public partition
    opal_atomic_uint32_t* internal_parts_ready;

    // parameters for message aggregation
    int aggregation_count;              // how many partitions may be aggregated
    int internal_partition_count;
    int public_partition_count;

    // buffer for public partitions ready to send
    opal_ring_buffer_t public_parts_ready;
};

void part_persist_aggregate_simple_init(struct part_persist_aggregation_state* state, int internal_partition_count, int public_partition_count);

void part_persist_aggregate_simple_reset(struct part_persist_aggregation_state* state);

int part_persist_aggregate_simple_elements(struct part_persist_aggregation_state* state);

void part_persist_aggregate_simple_push(struct part_persist_aggregation_state* state, int partition);

int part_persist_aggregate_simple_pull(struct part_persist_aggregation_state* state);

void part_persist_aggregate_simple_free(struct part_persist_aggregation_state* state);
