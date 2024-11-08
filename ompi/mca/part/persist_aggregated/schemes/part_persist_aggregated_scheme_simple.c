/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2024      High Performance Computing Center Stuttgart,
 *                         University of Stuttgart.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 *
 */

#include "part_persist_aggregated_scheme_simple.h"

#include <stdlib.h>
#include <string.h>

// wrapper around opal_ring_buffer operations to allow for positive integers (including 0) to be inserted
static void *custom_ring_buffer_push(opal_ring_buffer_t *buffer, int internal_partition)
{
    return opal_ring_buffer_push(buffer, (void *) (internal_partition + 1));
}

// wrapper around opal_ring_buffer operations to allow for positive integers (including 0) to be inserted
static int custom_ring_buffer_pop(opal_ring_buffer_t *buffer)
{
    return ((int) opal_ring_buffer_pop(buffer)) - 1;
}

// converts the index of a public partition to the index of its corresponding internal partition
static int internal_partition(struct part_persist_aggregation_state *state, int public_part)
{
    return public_part / state->aggregation_count;
}

void part_persist_aggregate_simple_init(struct part_persist_aggregation_state *state,
                                        int internal_partition_count, int factor, int last_internal_partition_size)
{
    state->public_partition_count = (internal_partition_count - 1) * factor + last_internal_partition_size;
    state->internal_partition_count = internal_partition_count;

    // number of user-partitions per internal partition (except for the last one)
    state->aggregation_count = factor;
    // number of user-partitions corresponding to the last internal partition
    state->last_internal_partition_size = last_internal_partition_size;

    // initialize counters
    state->internal_parts_ready = (opal_atomic_int32_t *) calloc(state->internal_partition_count,
                                                                 sizeof(opal_atomic_int32_t));

    // initialize ring buffer
    OBJ_CONSTRUCT(&state->public_parts_ready, opal_ring_buffer_t);
    opal_ring_buffer_init(&state->public_parts_ready, state->public_partition_count + 1);
}

void part_persist_aggregate_simple_reset(struct part_persist_aggregation_state *state)
{
    // reset flags
    if (NULL != state->internal_parts_ready) {
        memset(state->internal_parts_ready, 0, state->internal_partition_count * sizeof(opal_atomic_int32_t));
    }

    // clear ring buffer (should be empty already)
    while (NULL != opal_ring_buffer_poke(&state->public_parts_ready, 0)) {
        custom_ring_buffer_pop(&state->public_parts_ready);
    }
}

static inline int is_last_partition(struct part_persist_aggregation_state *state, int partition) {
    return (partition == state->internal_partition_count - 1);
}

static inline int num_public_parts(struct part_persist_aggregation_state *state, int partition) {
    return is_last_partition(state, partition) ? state->last_internal_partition_size : state->aggregation_count;
}

void part_persist_aggregate_simple_push(struct part_persist_aggregation_state *state, int partition)
{
    int internal_part = internal_partition(state, partition);
    // this is the new value (after adding)
    int count = opal_atomic_add_fetch_32(&state->internal_parts_ready[internal_part], 1);

    // push to buffer if internal partition is ready
    if (count == num_public_parts(state, internal_part)) {
        custom_ring_buffer_push(&state->public_parts_ready, internal_part);
    }
}

int part_persist_aggregate_simple_pull(struct part_persist_aggregation_state *state)
{
    return custom_ring_buffer_pop(&state->public_parts_ready);
}

void part_persist_aggregate_simple_free(struct part_persist_aggregation_state *state)
{
    if (state->internal_parts_ready != NULL)
        free((void*)state->internal_parts_ready);
    state->internal_parts_ready = NULL;

    OBJ_DESTRUCT(&state->public_parts_ready);
}
