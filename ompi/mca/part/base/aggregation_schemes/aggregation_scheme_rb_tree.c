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

#include "aggregation_scheme_rb_tree.h"

#include <stdlib.h>

// comparator for intervals: -1 if interval2 is left of interval1, 1 if interval2 right of interval1 and 0 if interval2 contained in interval1
static int interval_comp(interval_state_t* interval1, interval_state_t* interval2)
{
    if (interval2->left >= interval1->right) return 1;
    if (interval2->right <= interval1->left) return -1;
    return 0;
}

opal_rb_tree_comp_fn_t interval_comp_fn = (opal_rb_tree_comp_fn_t) interval_comp;


void aggregation_scheme_rb_tree_init(struct part_persist_aggregation_state_it *state, int factor)
{
    // number of user-partitions per internal partition (except for the last one)
    state->factor = factor;

    // TODO: allow arbitrary list sizes
    // OBJ_CONSTRUCT(&state->interval_state, opal_list_t);
    state->interval_states = calloc(1024, sizeof(interval_state_t));
    state->interval_count = 0;

    // 
    OBJ_CONSTRUCT(&state->intervals, opal_rb_tree_t);
    opal_rb_tree_init(&state->intervals, interval_comp_fn);
}

void aggregation_scheme_rb_tree_reset(struct part_persist_aggregation_state_it *state)
{
    state->interval_count = 0;

    opal_rb_tree_destroy(&state->intervals);
    opal_rb_tree_init(&state->intervals, interval_comp_fn);
}

int aggregation_scheme_rb_tree_pready_range(struct part_persist_aggregation_state_it *state,
                                            int min, int max, 
                                            int* available_partitions_first, int* available_partitions_last)
{
    // try to find adjacent intervals
    interval_state_t key;
    key.left = min-1; key.right = min-1;
    interval_state_t* left  = opal_rb_tree_find(&state->intervals, &key);
    key.left = max+1; key.right = max+1;
    interval_state_t* right = opal_rb_tree_find(&state->intervals, &key);

    if (NULL == left && NULL == right) // no adjacent interval
    {
        // new interval state object
        size_t index = opal_atomic_add_fetch_size_t(&state->interval_count, 1);
        state->interval_states[index].left  = min;
        state->interval_states[index].right = max;
        state->interval_states[index].consumed = false;

        opal_rb_tree_insert(&state->intervals, &state->interval_states[index], &state->interval_states[index]);
        return 0;
    }
    else if (NULL != left && !left->consumed && NULL != right && !right->consumed) // intervals can be merged
    {
        opal_rb_tree_delete(&state->intervals, &left);
        opal_rb_tree_delete(&state->intervals, &right);

        // grow left interval
        left->right = max;
        opal_rb_tree_insert(&state->intervals, &left, &left);

        if (right->right - left->left >= state->factor)
        {
            // large enough to extract
            left->consumed = true;
            *available_partitions_first = left->left;
            *available_partitions_last  = right->right;
            return 1;
        }
        
        return 0;
    }
    else if (NULL != left && !left->consumed) // left interval can be extended
    {
        opal_rb_tree_delete(&state->intervals, &left);

        // grow left interval
        left->right = max;
        opal_rb_tree_insert(&state->intervals, &left, &left);

        if (max + 1 - left->left >= state->factor)
        {
            *available_partitions_first = left->left;
            *available_partitions_last  = max;
            left->consumed = true;
            return 1;
        }
        
        return 0;
    }
    else if (NULL != right && !right->consumed) // right interval can be extended
    {
        opal_rb_tree_delete(&state->intervals, &right);

        // grow left interval
        right->left = min;
        opal_rb_tree_insert(&state->intervals, &right, &right);

        if (right->right + 1 - min >= state->factor)
        {
            right->consumed = true;
            *available_partitions_first = min;
            *available_partitions_last  = right->right;
            return 1;
        }
        
        return 0;
    }
    else 
    {
        // new interval state object
        size_t index = opal_atomic_add_fetch_size_t(&state->interval_count, 1);
        state->interval_states[index].left  = min;
        state->interval_states[index].right = max;
        state->interval_states[index].consumed = true;

        opal_rb_tree_insert(&state->intervals, &state->interval_states[index], &state->interval_states[index]);

        *available_partitions_first = min;
        *available_partitions_last  = max;

        return 1;
    }
}

int aggregation_scheme_rb_tree_pready(struct part_persist_aggregation_state_it *state, int partition, int* available_partitions_left, int* available_partitions_right)
{
    return aggregation_scheme_rb_tree_pready_range(state, partition, partition, available_partitions_left, available_partitions_right);
}

// reuse interval_state list as list of remaining intervals
void aggregation_scheme_rb_tree_remaining(struct part_persist_aggregation_state_it *state, interval_state_t** remaining, size_t* remaining_count)
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

void aggregation_scheme_rb_tree_free(struct part_persist_aggregation_state_it *state)
{
    if (state->interval_states != NULL)
        free((void*)state->interval_states);
    state->interval_states = NULL;

    opal_rb_tree_destroy(&state->intervals);
    OBJ_DESTRUCT(&state->intervals);
}

