#include "part_persist_aggregated_scheme_simple.h"

#include <stdlib.h>
#include <stdatomic.h>
#include <stddef.h>

#include <assert.h>

#include <stdio.h>


static void ring_buffer_init(struct ring_buffer* rb, size_t count, size_t dtype_size) {
    rb->capacity = (count + 1) * dtype_size;
    rb->buffer = (char*) calloc(rb->capacity, sizeof(char));
    rb->begin = ATOMIC_VAR_INIT(0);
    rb->end = ATOMIC_VAR_INIT(0);
    rb->end_internal = ATOMIC_VAR_INIT(0);
}


static void ring_buffer_push(struct ring_buffer* rb, void* data, size_t count) {
    int offset = atomic_fetch_add(&rb->end_internal, count);        // advance internal write pointer
    offset = offset % rb->capacity;

    for (int i = 0; i < count; i++) {       // write data (TODO: make this atomic too?)
        rb->buffer[(offset + i) % rb->capacity] = ((char*)data)[i];
    }

    atomic_fetch_add(&rb->end, count);      // now data is actually ready to be read
}

static void* ring_buffer_pull(struct ring_buffer* rb, size_t count) {
    int index = atomic_fetch_add(&rb->begin, count);
    return &rb->buffer[index % rb->capacity];
}

static void ring_buffer_clear(struct ring_buffer* rb) {
    // reset all indices
    atomic_fetch_and(&rb->end_internal, 0);
    atomic_fetch_and(&rb->end, 0);
    atomic_fetch_and(&rb->begin, 0);
}

static int ring_buffer_empty(struct ring_buffer* rb) {
    return atomic_fetch_add(&rb->begin, 0) == atomic_fetch_add(&rb->end, 0);
}

static int ring_buffer_elements(struct ring_buffer* rb) {
    return (atomic_fetch_add(&rb->end, 0) - atomic_fetch_add(&rb->begin, 0) + rb->capacity) % rb->capacity;
}

static void ring_buffer_free(struct ring_buffer* rb) {
    if (NULL != rb->buffer)
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

    // initialize counters
    state->internal_parts_ready = (atomic_int*) calloc(state->internal_partition_count, sizeof(atomic_int));

    // initialized locked ring buffer
    ring_buffer_init(&state->public_parts_ready, state->public_partition_count, sizeof(int));
}

int part_persist_aggregate_simple_elements(struct part_persist_aggregation_state* state) {
    return ring_buffer_elements(&state->public_parts_ready);
}

void part_persist_aggregate_simple_reset(struct part_persist_aggregation_state* state) {
    if (NULL != state->internal_parts_ready) {
        for(int i = 0; i < state->internal_partition_count; i++) {
            state->internal_parts_ready[i] = ATOMIC_VAR_INIT(0);
        }
    }
    ring_buffer_clear(&state->public_parts_ready);
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
    if (ring_buffer_empty(&state->public_parts_ready)) {
        return -1;
    }

    int* partition_ptr = (int*)ring_buffer_pull(&state->public_parts_ready, sizeof(int));

    return *partition_ptr;   
}
    
void part_persist_aggregate_simple_free(struct part_persist_aggregation_state* state) {
    if (state->internal_parts_ready != NULL)
        free(state->internal_parts_ready);
    state->internal_parts_ready = NULL;
    ring_buffer_free(&state->public_parts_ready);
}
