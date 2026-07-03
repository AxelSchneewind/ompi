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

#include "aggregation_scheme_dynamic.h"

#include "aggregation_scheme_identity.h"
#include "aggregation_scheme_regular.h"
#include "aggregation_scheme_rb_tree.h"
#include "aggregation_scheme_interval_tree.h"

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

void aggregation_scheme_dynamic_init(struct part_persist_aggregation_state_t *state, aggregation_algorithm algorithm, int parts, int factor)
{
    OBJ_CONSTRUCT(&state->lock, opal_mutex_t);

    state->algorithm = algorithm;

    // default-initialize function pointers with null
    state->reset  = NULL;
    state->free   = NULL;
    state->pready_range = NULL;
    state->remaining  = NULL;

    void* actual_aggregation_state = NULL;
    switch (state->algorithm)
    {
        case IDENTITY:
            state->reset  = (unary_fn_ptr)&aggregation_scheme_identity_reset;
            state->free   = (unary_fn_ptr)&aggregation_scheme_identity_free;
            state->pready_range = (pready_range_fn_ptr)&aggregation_scheme_identity_pready_range;

            actual_aggregation_state = malloc(sizeof(struct part_persist_identity_aggregation_state_t));

            aggregation_scheme_identity_init((struct part_persist_identity_aggregation_state_t*)actual_aggregation_state);
            break;
        case REGULAR:
            state->reset  = (unary_fn_ptr)&aggregation_scheme_regular_reset;
            state->free   = (unary_fn_ptr)&aggregation_scheme_regular_free;
            state->pready_range = (pready_range_fn_ptr)&aggregation_scheme_regular_pready_range;

            actual_aggregation_state = malloc(sizeof(struct part_persist_regular_aggregation_state_t));

            aggregation_scheme_regular_init((struct part_persist_regular_aggregation_state_t*)actual_aggregation_state, parts, factor);
            break;
        case INTERVAL_TREE:
            state->reset  = (unary_fn_ptr)&aggregation_scheme_interval_tree_reset;
            state->free   = (unary_fn_ptr)&aggregation_scheme_interval_tree_free;
            state->pready_range = (pready_range_fn_ptr)(&aggregation_scheme_interval_tree_pready_range);
            state->remaining = (remaining_fn_ptr)(&aggregation_scheme_interval_tree_remaining);

            actual_aggregation_state = malloc(sizeof(struct part_persist_interval_tree_aggregation_state_t));

            aggregation_scheme_interval_tree_init((struct part_persist_interval_tree_aggregation_state_t*)actual_aggregation_state, parts, factor);
            break;
        case RB_TREE:
            state->reset  = (unary_fn_ptr)&aggregation_scheme_rb_tree_reset;
            state->free   = (unary_fn_ptr)&aggregation_scheme_rb_tree_free;
            state->pready_range = (pready_range_fn_ptr)(&aggregation_scheme_rb_tree_pready_range);
            state->remaining = (remaining_fn_ptr)(&aggregation_scheme_rb_tree_remaining);

            actual_aggregation_state = malloc(sizeof(struct part_persist_rb_tree_aggregation_state_t));

            aggregation_scheme_rb_tree_init((struct part_persist_rb_tree_aggregation_state_t*)actual_aggregation_state, parts, factor);
            break;
    }
    state->aggregation_state = actual_aggregation_state;
}

void aggregation_scheme_dynamic_reset(struct part_persist_aggregation_state_t *state)
{
    OPAL_THREAD_LOCK(&state->lock);
    state->reset(state->aggregation_state);
    OPAL_THREAD_UNLOCK(&state->lock);
}

int aggregation_scheme_dynamic_pready(struct part_persist_aggregation_state_t *state, int partition, int* available_partition_min, int* available_partition_max)
{
    int result;

    if (NULL != state->pready_range)
    {
        OPAL_THREAD_LOCK(&state->lock);
        result = state->pready_range(state->aggregation_state, partition, partition, available_partition_min, available_partition_max);
        OPAL_THREAD_UNLOCK(&state->lock);
    }

    return result;
}

int aggregation_scheme_dynamic_pready_range(struct part_persist_aggregation_state_t *state,
                                       int partition_min, int partition_max, int* available_partition_min, int* available_partition_max)
{
    int result;

    if (NULL != state->pready_range)
    {
        OPAL_THREAD_LOCK(&state->lock);
        result = state->pready_range(state->aggregation_state, partition_min, partition_max, available_partition_min, available_partition_max);
        OPAL_THREAD_UNLOCK(&state->lock);
    }

    return result;
}

void aggregation_scheme_dynamic_free(struct part_persist_aggregation_state_t *state)
{
    if (NULL != state->free)
    {
        OPAL_THREAD_LOCK(&state->lock);
        state->free(state->aggregation_state);
        OPAL_THREAD_UNLOCK(&state->lock);
    }
    OBJ_DESTRUCT(&state->lock);
}

void aggregation_scheme_dynamic_remaining(struct part_persist_aggregation_state_t *state, void **remaining, size_t *remaining_count)
{
    OPAL_THREAD_LOCK(&state->lock);
    if (NULL != state->remaining)
    {
        state->remaining(state->aggregation_state, remaining, remaining_count);
    }
    else
    {
        *remaining = NULL;
        *remaining_count = 0;
    }
    OPAL_THREAD_UNLOCK(&state->lock);
}
