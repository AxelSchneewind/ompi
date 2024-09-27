#include "part_persist_aggregated_scheme_simple.h"

#include <stdlib.h>
#include <stdatomic.h>
#include <stddef.h>

#include <assert.h>

#include <stdio.h>

static int internal_partition(struct part_persist_aggregation_state* state, int public_part) {
    return public_part / state->aggregation_count;
}

void part_persist_aggregate_simple_init(struct part_persist_aggregation_state* state, int internal_partition_count, int public_partition_count) {
    state->public_partition_count = public_partition_count;
    state->internal_partition_count = internal_partition_count;

    state->aggregation_count = state->public_partition_count / state->internal_partition_count;
    assert(state->aggregation_count > 0);

    // initialize counters
    state->internal_parts_ready = (opal_atomic_int32_t*) calloc(state->internal_partition_count, sizeof(atomic_int));

    // initialize ring buffer
    OBJ_CONSTRUCT(&state->public_parts_ready, opal_ring_buffer_t);
    opal_ring_buffer_init(&state->public_parts_ready, state->public_partition_count + 1);
}


void part_persist_aggregate_simple_reset(struct part_persist_aggregation_state* state) {
    if (NULL != state->internal_parts_ready) {
        for(int i = 0; i < state->internal_partition_count; i++) {
            state->internal_parts_ready[i] = 0;
        }
    }

    // clear ring buffer (should be empty already)
    while (NULL != opal_ring_buffer_poke(&state->public_parts_ready, 0)) {
        opal_ring_buffer_pop(&state->public_parts_ready);
    }
}

void part_persist_aggregate_simple_push(struct part_persist_aggregation_state* state, int partition) {
    int internal_part = internal_partition(state, partition);
    int count = opal_atomic_add_fetch_32(&state->internal_parts_ready[internal_part], 1);

    // push to buffer if public partition is ready
    if (count == state->aggregation_count - 1) {
        opal_ring_buffer_push(&state->public_parts_ready, (void*) (internal_part + 1));
    }
}

int part_persist_aggregate_simple_pull(struct part_persist_aggregation_state* state) {
    // check if element can be pulled
    if (NULL == opal_ring_buffer_poke(&state->public_parts_ready, 0)) {
        return -1;
    }

    return ((int)opal_ring_buffer_pop(&state->public_parts_ready)) - 1;
}
    
void part_persist_aggregate_simple_free(struct part_persist_aggregation_state* state) {
    if (state->internal_parts_ready != NULL)
        free(state->internal_parts_ready);
    state->internal_parts_ready = NULL;
    OBJ_DESTRUCT(&state->public_parts_ready);
}
