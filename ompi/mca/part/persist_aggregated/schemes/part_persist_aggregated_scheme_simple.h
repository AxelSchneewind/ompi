#pragma once // TODO switch to traditional header guards

#include <stdlib.h>
#include <stdatomic.h>
#include <stddef.h>

// simple and (possibly) unsafe ring buffer
struct ring_buffer{
    size_t capacity;
    atomic_int begin;           // read index
    atomic_int end;             // past-last index of available data
    atomic_int end_internal;    // internal write index
    char* buffer;
};

struct part_persist_aggregation_state {
    // counters for each public partition
    atomic_int* internal_parts_ready;

    // parameters for message aggregation
    int aggregation_count;              // how many partitions may be aggregated
    int internal_partition_count;
    int public_partition_count;

    // buffer for public partitions ready to send
    struct ring_buffer public_parts_ready;
};

void part_persist_aggregate_simple_init(struct part_persist_aggregation_state* state, int internal_partition_count, int public_partition_count);

void part_persist_aggregate_simple_reset(struct part_persist_aggregation_state* state);

int part_persist_aggregate_simple_elements(struct part_persist_aggregation_state* state);

void part_persist_aggregate_simple_push(struct part_persist_aggregation_state* state, int partition);

int part_persist_aggregate_simple_pull(struct part_persist_aggregation_state* state);

void part_persist_aggregate_simple_free(struct part_persist_aggregation_state* state);
