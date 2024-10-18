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

// wrapper around opal_ring_buffer operations to allow for 0 to be inserted
static void *custom_ring_buffer_push(opal_ring_buffer_t *buffer, int internal_partition)
{
    return opal_ring_buffer_push(buffer, (void *) (internal_partition + 1));
}

//
static int custom_ring_buffer_pop(opal_ring_buffer_t *buffer)
{
    return (int) opal_ring_buffer_pop(buffer) - 1;
}

static int internal_partition(struct part_persist_aggregation_state *state, int public_part)
{
    return public_part / state->aggregation_count;
}

void part_persist_aggregate_simple_init(struct part_persist_aggregation_state *state,
                                        int internal_partition_count, int public_partition_count)
{
    state->public_partition_count = public_partition_count;
    state->internal_partition_count = internal_partition_count;

    state->aggregation_count = state->public_partition_count / state->internal_partition_count;

    // initialize counters
    state->internal_parts_ready = (opal_atomic_int32_t *) calloc(state->internal_partition_count,
                                                                 sizeof(opal_atomic_int32_t));

    // initialize ring buffer
    OBJ_CONSTRUCT(&state->public_parts_ready, opal_ring_buffer_t);
    opal_ring_buffer_init(&state->public_parts_ready, state->public_partition_count + 1);
}

void part_persist_aggregate_simple_reset(struct part_persist_aggregation_state *state)
{
    if (NULL != state->internal_parts_ready) {
        for (int i = 0; i < state->internal_partition_count; i++) {
            state->internal_parts_ready[i] = 0;
        }
    }

    // clear ring buffer (should be empty already)
    while (NULL != opal_ring_buffer_poke(&state->public_parts_ready, 0)) {
        custom_ring_buffer_pop(&state->public_parts_ready);
    }
}

void part_persist_aggregate_simple_push(struct part_persist_aggregation_state *state, int partition)
{
    int internal_part = internal_partition(state, partition);
    int count = opal_atomic_add_fetch_32(&state->internal_parts_ready[internal_part], 1);

    // push to buffer if public partition is ready
    if (count == state->aggregation_count - 1) {
        custom_ring_buffer_push(&state->public_parts_ready, internal_part);
    }
}

int part_persist_aggregate_simple_pull(struct part_persist_aggregation_state *state)
{
    // check if element can be pulled
    if (NULL == opal_ring_buffer_poke(&state->public_parts_ready, 0)) {
        return -1;
    }

    return custom_ring_buffer_pop(&state->public_parts_ready);
}

void part_persist_aggregate_simple_free(struct part_persist_aggregation_state *state)
{
    if (state->internal_parts_ready != NULL)
        free((void*)state->internal_parts_ready);
    state->internal_parts_ready = NULL;
    OBJ_DESTRUCT(&state->public_parts_ready);
}
