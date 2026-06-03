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

#include "aggregation_scheme_regular.h"
#include "aggregation_scheme_rb_tree.h"
#include "aggregation_scheme_interval_tree.h"

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

void aggregation_scheme_dynamic_init(struct part_persist_aggregation_state_t *state, aggregation_algorithm algorithm, ...)
{
    va_list args;

    state->algorithm = algorithm;

    // to forward multiple variadic arguments. 
    // Required as (va_arg(args, int), va_arg(args, int)) has undefined evaluation order
    int int_args[5];

    // default-initialize function pointers with null
    state->reset  = NULL;
    state->free   = NULL;
    state->pready = NULL;
    state->pready_range = NULL;
    state->remaining  = NULL;

    void* actual_aggregation_state;
    switch (state->algorithm)
    {
        case INTERVAL_TREE:
            state->reset  = (unary_fn_ptr)&aggregation_scheme_interval_tree_reset;
            state->free   = (unary_fn_ptr)&aggregation_scheme_interval_tree_free;
            state->pready = (pready_fn_ptr)&aggregation_scheme_interval_tree_pready;
            state->pready_range = (pready_range_fn_ptr)(&aggregation_scheme_interval_tree_pready_range);

            va_start(args, 1);
            actual_aggregation_state = malloc(sizeof(struct part_persist_interval_tree_aggregation_state_t));
            aggregation_scheme_interval_tree_init((struct part_persist_interval_tree_aggregation_state_t*)actual_aggregation_state, va_arg(args, int));
            va_end(args);
            break;
        case RB_TREE:
            state->reset  = (unary_fn_ptr)&aggregation_scheme_rb_tree_reset;
            state->free   = (unary_fn_ptr)&aggregation_scheme_rb_tree_free;
            state->pready = (pready_fn_ptr)&aggregation_scheme_rb_tree_pready;
            state->pready_range = (pready_range_fn_ptr)(&aggregation_scheme_rb_tree_pready_range);

            va_start(args, 1);
            actual_aggregation_state = malloc(sizeof(struct part_persist_rb_tree_aggregation_state_t));
            aggregation_scheme_rb_tree_init((struct part_persist_rb_tree_aggregation_state_t*)actual_aggregation_state, va_arg(args, int));
            va_end(args);
            break;
        case REGULAR:
            state->reset  = (unary_fn_ptr)&aggregation_scheme_regular_reset;
            state->free   = (unary_fn_ptr)&aggregation_scheme_regular_free;
            state->pready = (pready_fn_ptr)&aggregation_scheme_regular_pready;
            state->pready_range = NULL;

            va_start(args, 3);
            actual_aggregation_state = malloc(sizeof(struct part_persist_regular_aggregation_state_t));
            int_args[0] = va_arg(args, int);
            int_args[1] = va_arg(args, int);
            int_args[2] = va_arg(args, int);
            aggregation_scheme_regular_init((struct part_persist_regular_aggregation_state_t*)actual_aggregation_state, int_args[0], int_args[1], int_args[2]);
            va_end(args);
            break;
    }
    state->aggregation_state = actual_aggregation_state;
}

void aggregation_scheme_dynamic_reset(struct part_persist_aggregation_state_t *state)
{
    state->reset(state);
}

void aggregation_scheme_dynamic_pready(struct part_persist_aggregation_state_t *state, int partition, int* available_partition_min, int* available_partition_max)
{
    state->pready(state, partition, available_partition_min, available_partition_max);
}

void aggregation_scheme_dynamic_pready_range(struct part_persist_aggregation_state_t *state,
                                       int partition_min, int partition_max, int* available_partition_min, int* available_partition_max)
{
    if (NULL != state->pready_range)
    {
        state->pready_range(state, partition_min, partition_max, available_partition_min, available_partition_max);
    }
    else 
    {
        int some_available = 0;
        for (size_t partition = partition_min; partition <= partition_max; partition++)
        {
            int out_min, out_max;
            state->pready(state, partition, &out_min, &out_max);
            if (out_min <= out_max)
            { // set or grow output interval
                if (!some_available || out_min < *available_partition_min)
                    *available_partition_min = out_min;
                if (!some_available || out_max > *available_partition_max)
                    *available_partition_max = out_max;
                some_available = 1;
            }
        }

        if (!some_available)
        {
            *available_partition_min = 0;
            *available_partition_max = -1;
        }
    }
}

void aggregation_scheme_dynamic_free(struct part_persist_aggregation_state_t *state)
{
    state->free(state);
}

void aggregation_scheme_dynamic_remaining(struct part_persist_aggregation_state_t *state, void **remaining, size_t *remaining_count)
{
    if (NULL != state->remaining)
    {
        state->remaining(state, remaining, remaining_count);
    }
    else
    {
        *remaining = NULL;
        *remaining_count = 0;
    }
}
