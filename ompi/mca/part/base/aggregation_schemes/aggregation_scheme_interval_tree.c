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

void aggregation_scheme_interval_tree_init(struct part_persist_interval_tree_aggregation_state_t *state, int parts, int factor)
{
    // number of user-partitions per internal partition (except for the last one)
    state->parts = parts;
    state->factor = factor;

    state->interval_states = calloc(state->parts, sizeof(interval_state_t));
    state->interval_count = 0;

    // 
    OBJ_CONSTRUCT(&state->intervals, opal_interval_tree_t);
    opal_interval_tree_init(&state->intervals);
}

void aggregation_scheme_interval_tree_reset(struct part_persist_interval_tree_aggregation_state_t *state)
{
    opal_atomic_swap_64(&state->interval_count, 0);

    opal_interval_tree_destroy(&state->intervals);
    opal_interval_tree_init(&state->intervals);
}

int aggregation_scheme_interval_tree_pready_range(struct part_persist_interval_tree_aggregation_state_t *state,
                                                int min, int max, 
                                                int* available_partitions_first, int* available_partitions_last)
{
    int err = OPAL_SUCCESS;
    // 
    interval_state_t dummy;
    dummy.left = -1; dummy.right = -1;
    dummy.consumed = true;

    // try to find adjacent intervals
    interval_state_t *left = &dummy, *right = &dummy; 
    if (min > 0) {
        left = opal_interval_tree_find_overlapping(&state->intervals, min - 1, min - 1);
    }
    if (max < state->parts-1)
    {
        right = opal_interval_tree_find_overlapping(&state->intervals, max + 1, max + 1);
    }

    bool empty_left  = NULL == left;
    bool empty_right = NULL == right;

    bool merge_left  = !empty_left  && !left->consumed;
    bool merge_right = !empty_right && !right->consumed;

    interval_state_t* candidate = NULL;

    if (merge_left && merge_right) // intervals can be merged
    {
        // mark right interval as consumed, as we might see it again when searching for remaining partitions
        right->consumed = true;
        err = opal_interval_tree_delete(&state->intervals, right->left, right->right, NULL);
        assert(OPAL_SUCCESS == err);

        // extend the left interval
        err = opal_interval_tree_delete(&state->intervals, left->left, left->right, NULL);
        assert(OPAL_SUCCESS == err);
        left->right = right->right;
        err = opal_interval_tree_insert(&state->intervals, left, left->left, left->right);
        assert(OPAL_SUCCESS == err);

        candidate = left;

        // both neighbors invalidated
        left = NULL; 
        right = NULL; 

        // we can now assume that the candidate interval is blocked or free on both sides
    }
    else if (merge_left) // left interval can be extended
    {
        // extend interval
        err = opal_interval_tree_delete(&state->intervals, left->left, left->right, NULL);
        assert(OPAL_SUCCESS == err);
        left->right = max;
        err = opal_interval_tree_insert(&state->intervals, left, left->left, left->right);
        assert(OPAL_SUCCESS == err);

        candidate = left;

        // left neighbor invalidated
        left = NULL; 

        // we can now assume that the candidate interval is blocked or free on both sides
    }
    else if (merge_right) // right interval can be extended
    {
        // extend interval
        err = opal_interval_tree_delete(&state->intervals, right->left, right->right, NULL);
        assert(OPAL_SUCCESS == err);
        right->left = min;
        err = opal_interval_tree_insert(&state->intervals, right, right->left, right->right);
        assert(OPAL_SUCCESS == err);

        candidate = right;

        // left neighbor invalidated
        right = NULL; 

        // we can now assume that the candidate interval is blocked or free on both sides
    }
    else
    {
        // new interval state object
        size_t index = opal_atomic_fetch_add_64(&state->interval_count, 1);
        struct interval_state_t* new = &state->interval_states[index];
        new->left  = min;
        new->right = max;
        new->consumed = false;

        err = opal_interval_tree_insert(&state->intervals, new, new->left, new->right);
        assert(OPAL_SUCCESS == err);

        candidate = new;
        left = NULL; 
        right = NULL;

        // we can now assume that the candidate interval is blocked or free on both sides
    }

    bool do_extract = candidate->right + 1 - candidate->left >= state->factor;

    // if interval is too small, check if it is blocked
    if (!do_extract)
    {
        if (NULL == left) {
            left = &dummy;
            if (candidate->left > 0)
            {
                interval_state_t lkey; 
                lkey.left  = candidate->left - 1; lkey.right = candidate->left - 1;
                left = opal_interval_tree_find_overlapping(&state->intervals, lkey.left, lkey.right);
            }
        }
        if (NULL == right)
        {
            right = &dummy;
            if (candidate->right < state->parts-1)
            {
                interval_state_t rkey; 
                rkey.left  = candidate->right + 1; rkey.right = candidate->right + 1;
                right = opal_interval_tree_find_overlapping(&state->intervals, rkey.left, rkey.right);
            }
        }

        // if we found a neighbor, it has to be consumed as otherwise, we would have merged earlier
        bool blocked_left  = NULL != left ;
        bool blocked_right = NULL != right;

        do_extract = blocked_left && blocked_right;
    }

    if (do_extract)
    {
        candidate->consumed = true;
        *available_partitions_first = candidate->left;
        *available_partitions_last  = candidate->right;
        return 1;
    }
    else
    {
        *available_partitions_first = 0;
        *available_partitions_last  = -1;
        return 0;
    }
}

int aggregation_scheme_interval_tree_pready(struct part_persist_interval_tree_aggregation_state_t *state, int partition, int* available_partitions_left, int* available_partitions_right)
{
    return aggregation_scheme_interval_tree_pready_range(state, partition, partition, available_partitions_left, available_partitions_right);
}

// reuse interval_state list as list of remaining intervals
void aggregation_scheme_interval_tree_remaining(struct part_persist_interval_tree_aggregation_state_t *state, interval_state_t** remaining, size_t* remaining_count)
{
    size_t count = opal_atomic_fetch_add_64(&state->interval_count, 0);
    for (size_t i = 0; i < count; i++)
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

void aggregation_scheme_interval_tree_free(struct part_persist_interval_tree_aggregation_state_t *state)
{
    if (state->interval_states != NULL)
        free((void*)state->interval_states);
    state->interval_states = NULL;

    opal_interval_tree_destroy(&state->intervals);
    OBJ_DESTRUCT(&state->intervals);
}

