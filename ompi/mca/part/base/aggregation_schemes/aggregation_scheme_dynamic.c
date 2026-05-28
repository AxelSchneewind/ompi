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

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

void aggregation_scheme_dynamic_psend_init(struct part_persist_aggregation_state *state, aggregation_algorithm algorithm, ...)
{
    va_list args;

    state->algorithm = algorithm;

    void* actual_aggregation_state;
    switch (state->algorithm)
    {
        case INTERVAL_TREE:
            va_start(args, 1);
            actual_aggregation_state = malloc(sizeof(part_persist_interval_tree_aggregation_state));
            part_persist_interval_tree_aggregation_state_init(actual_aggregation_state, va_arg(args, int));
            va_end(args);
            break;
        case RB_TREE:
            va_start(args, 1);
            actual_aggregation_state = malloc(sizeof(part_persist_rb_tree_aggregation_state));
            part_persist_rb_tree_aggregation_state_init(actual_aggregation_state, va_arg(args, int));
            va_end(args);
            break;
        case RB_TREE:
            va_start(args, 3);
            actual_aggregation_state = malloc(sizeof(part_persist_rb_tree_aggregation_state));
            part_persist_rb_tree_aggregation_state_init(actual_aggregation_state, va_arg(args, int), va_arg(args, int), va_arg(args, int));
            va_end(args);
            break;
    }
    state->aggregation_state = actual_aggregation_state;
}

void aggregation_scheme_dynamic_reset(struct part_persist_aggregation_state *state)
{
    switch (state->algorithm)
    {
        case INTERVAL_TREE:
            part_persist_interval_tree_aggregation_state_reset(state->aggregation_state);
            break;
        case RB_TREE:
            part_persist_rb_aggregation_state_reset(state->aggregation_state);
            break;
        case RB_TREE:
            // TODO
            break;
    }
}

void aggregation_scheme_dynamic_pready(struct part_persist_aggregation_state *state, int partition, int* available_partition_min, int* available_partition_max)
{

}

void aggregation_scheme_dynamic_free(struct part_persist_aggregation_state *state)
{

}
