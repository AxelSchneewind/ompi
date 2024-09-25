#include "part_persist_aggregated_scheme_simple.h"

#include <stdlib.h>
#include <stdatomic.h>
#include <stddef.h>

#include <assert.h>

#include <stdio.h>


static void ring_buffer_init(struct ring_buffer* rb, size_t count, size_t dtype_size) {
    rb->capacity = (count + 1) * dtype_size;
    rb->buffer = (char*)malloc(rb->capacity);
    rb->begin = 0;
    rb->end = 0;
}

static void ring_buffer_push(struct ring_buffer* rb, void* data, size_t count) {
    for (int i = 0; i < count; i++) {
        rb->buffer[rb->end] = ((char*)data)[i];
        rb->end = (++rb->end) % rb->capacity;
    }
}

static void ring_buffer_clear(struct ring_buffer* rb) {
    rb->end = rb->begin;
}

static int ring_buffer_empty(struct ring_buffer* rb) {
    return rb->begin == rb->end;
}

static void* ring_buffer_pull(struct ring_buffer* rb, size_t count) {
    void* result = &rb->buffer[rb->begin];
    rb->begin = (rb->begin + count) % rb->capacity;
    return result;
}

static void ring_buffer_free(struct ring_buffer* rb) {
    free(rb->buffer);
    rb->buffer = NULL;
}



int internal_partition(struct part_persist_aggregation_state* state, int public_part) {
    return public_part / state->aggregation_count;
}

int first_public_partition(struct part_persist_aggregation_state* state, int internal_part) {
    return internal_part * state->aggregation_count;
}

void part_persist_aggregate_simple_init(struct part_persist_aggregation_state* state, int internal_partition_count, int public_partition_count) {
    state->public_partition_count = public_partition_count;
    state->internal_partition_count = internal_partition_count;

    state->aggregation_count = state->public_partition_count / state->internal_partition_count;
    assert(state->aggregation_count > 0);

    state->internal_parts_ready = (atomic_int*) calloc(state->internal_partition_count, sizeof(atomic_int));
    ring_buffer_init(&state->public_parts_ready, state->public_partition_count, sizeof(int));
}

void part_persist_aggregate_simple_reset(struct part_persist_aggregation_state* state) {
    if (NULL != state->internal_parts_ready) {
        for(int i = 0; i < state->internal_partition_count; i++) {
            state->internal_parts_ready[i] = ATOMIC_VAR_INIT(0);
        }
    }
}

void part_persist_aggregate_simple_push(struct part_persist_aggregation_state* state, int partition) {
    int internal_part = internal_partition(state, partition);
    int count = atomic_fetch_add(&state->internal_parts_ready[internal_part], 1);

    // push to buffer if public partition is ready
    if (count == state->aggregation_count - 1) {
        ring_buffer_push(&state->public_parts_ready, &internal_part, sizeof(int));
    }
}

int part_persist_aggregate_simple_pull(struct part_persist_aggregation_state* state) {
    // 
    if (ring_buffer_empty(&state->public_parts_ready))
        return -1;

    int* partition_ptr = (int*)ring_buffer_pull(&state->public_parts_ready, sizeof(int));
    return *partition_ptr;
}

int part_persist_aggregate_simple_extract(struct part_persist_aggregation_state* state, int public_partition) {
    int internal_part = internal_partition(state, public_partition);
    int count = atomic_fetch_add(&state->internal_parts_ready[internal_part], 0);
    if (count == state->aggregation_count - 1) {
        atomic_fetch_add(&state->internal_parts_ready[internal_part], 1);
        return internal_part;
    }

    return -1;
}

void part_persist_aggregate_simple_free(struct part_persist_aggregation_state* state) {
    free(state->internal_parts_ready);
    state->internal_parts_ready = NULL;
    ring_buffer_free(&state->public_parts_ready);
}
