/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2026      High Performance Computing Center Stuttgart,
 *                         University of Stuttgart.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 *
 */

#include "aggregation_scheme_interval_tree.h"

#include <stdlib.h>

void part_persist_aggregate_interval_tree_init(struct part_persist_aggregation_state_it *state, int factor)
{
    // number of user-partitions per internal partition (except for the last one)
    state->factor = factor;

    // 
    // OBJ_CONSTRUCT(&state->interval_state, opal_list_t);
    state->interval_states = calloc(1024, sizeof(interval_state_t));
    state->interval_count = 0;

    // 
    OBJ_CONSTRUCT(&state->intervals, opal_interval_tree_t);
    opal_interval_tree_init(&state->intervals);
}

void part_persist_aggregate_interval_tree_reset(struct part_persist_aggregation_state_it *state)
{
    state->interval_count = 0;

    opal_interval_tree_destroy(&state->intervals);
    opal_interval_tree_init(&state->intervals);
}

int part_persist_aggregate_interval_tree_pready_range(struct part_persist_aggregation_state_it *state,
                                                int min, int max, 
                                                int* available_partitions_first, int* available_partitions_last)
{
    // try to find adjacent intervals
    interval_state_t* left  = opal_interval_tree_find_overlapping(&state->intervals, min - 1, min - 1);
    interval_state_t* right = opal_interval_tree_find_overlapping(&state->intervals, max + 1, max + 1);

    if (NULL == left && NULL == right) // no adjacent interval
    {
        // new interval state object
        state->interval_states[state->interval_count].left  = min;
        state->interval_states[state->interval_count].right = max;

        opal_interval_tree_insert(&state->intervals, &state->interval_states[state->interval_count], min, max);
        state->interval_count++;
        return 0;
    }
    else if (NULL != left && !left->consumed && NULL != right && !right->consumed) // intervals can be merged
    {
        opal_interval_tree_delete(&state->intervals, left->left,  left->right,  NULL);
        opal_interval_tree_delete(&state->intervals, right->left, right->right, NULL);

        if (right->right - left->left >= state->factor)
        {
            // large enough to extract
            *available_partitions_first = left->left;
            *available_partitions_last  = right->right;
            return 1;
        }
        else 
        {
            // reinsert as one interval
            opal_interval_tree_insert(&state->intervals, left, left->left, right->right);
            return 0;
        }
    }
    else if (NULL != left && !left->consumed) // left interval can be extended
    {
        opal_interval_tree_delete(&state->intervals, left->left, left->right, NULL);

        if (max + 1 - left->left >= state->factor)
        {
            *available_partitions_first = left->left;
            *available_partitions_last  = max;
            return 1;
        }
        else 
        {
            // grow and reinsert
            left->right = max;
            opal_interval_tree_insert(&state->intervals, left, left->left, left->right);
            return 0;
        }
    }
    else if (NULL != right && !right->consumed) // right interval can be extended
    {

        if (right->right + 1 - min >= state->factor)
        {
            *available_partitions_first = min;
            *available_partitions_last  = right->right;
            return 1;
        }
        else 
        {
            // grow and reinsert
            right->left = min;
            opal_interval_tree_insert(&state->intervals, right, right->left, right->right);
            return 0;
        }
    }
}

int part_persist_aggregate_interval_tree_pready(struct part_persist_aggregation_state_it *state, int partition, int* available_partitions_left, int* available_partitions_right)
{
    return part_persist_aggregate_interval_tree_pready_range(state, partition, partition, available_partitions_left, available_partitions_right);
}

// reuse interval_state list as list of remaining intervals
void part_persist_aggregate_interval_tree_remaining(struct part_persist_aggregation_state_it *state, interval_state_t** remaining, size_t* remaining_count)
{
    int count = state->interval_count;
    for (int i = 0; i < count; i++)
    {
        if (state->interval_states[i].consumed)
        {
            // swap from end
            interval_state_t temp = state->interval_states[i];
            state->interval_states[i] = state->interval_states[--count];
            state->interval_states[--count]= temp;
        }
    }

    *remaining = state->interval_states;
    *remaining_count = count;
}

void part_persist_aggregate_interval_tree_free(struct part_persist_aggregation_state_it *state)
{
    if (state->interval_states != NULL)
        free((void*)state->interval_states);
    state->interval_states = NULL;

    opal_interval_tree_destroy(&state->intervals);
    OBJ_DESTRUCT(&state->intervals);
}

